/**
 * Live aircraft poller — single source of truth for the unified-flights layer.
 *
 * Two pollers run inside this module:
 *
 *   • Live ADS-B poll (every 12 s) — fans out to OpenSky and adsb.lol in
 *     parallel and merges by ICAO24 via `mergeLiveByIcao()`. adsb.lol wins
 *     on geometry (community feeders see positions seconds after squawk,
 *     OpenSky's aggregation runs minutes behind), but property bags are
 *     unioned. Either source returning data is enough — backoff only kicks
 *     in when *both* fail. Each surviving aircraft drives a `live_vehicle`
 *     WS event with the full `properties` blob so iOS popups don't need
 *     a follow-up fetch. With no OpenSky credentials and adsb.lol's open
 *     endpoint, the poller is fully functional without any API keys.
 *
 *   • AeroDataBox airport poll (every 5 min) — pulls the next 11 h of
 *     scheduled arrivals/departures for NRT + HND. Updates a separate
 *     `schedSnapshot` keyed by flight number. A scheduled row is dropped
 *     if the same flight is already airborne (a row exists in
 *     `liveSnapshot` whose normalized callsign matches the schedule's
 *     normalized flight number).
 *
 * `getSnapshot()` returns a single FeatureCollection: live rows first, then
 * scheduled rows that don't collide. The `/api/data/unified-flights` route
 * serves this directly — no DB indirection.
 *
 * adsb.lol is unauthenticated and has no documented rate limit; OpenSky's
 * anonymous tier reliably 429s and OAuth credentials lift it to ~4000
 * calls/day. We poll every 12 s by default — comfortable for both, fast
 * enough that animation gaps stay under the iOS 1 s linear interpolation
 * window.
 *
 * Both-source failure triggers exponential backoff (30 s → 5 min cap), then
 * resets after the next successful poll. AeroDataBox failures are silent
 * (the helper already returns []). Idempotent: startPlanePoller() cancels
 * any existing timers first.
 */

import { broadcastEvent } from './collectorTap.js';
import {
  tryOpenSkyAPI,
  tryAdsbLol,
  tryAeroDataBoxAirport,
  mergeLiveByIcao,
  normalizeCallsign,
  AERODATABOX_AIRPORTS,
} from '../collectors/flightAdsb.js';
import { classifyMilitary } from '../collectors/_militaryIcao.js';

const DEFAULT_INTERVAL_MS = Number(process.env.PLANE_POLLER_INTERVAL_MS || 12_000);
const SCHED_INTERVAL_MS = Number(process.env.PLANE_SCHED_INTERVAL_MS || 5 * 60_000);
const MIN_BACKOFF_MS = 30_000;
const MAX_BACKOFF_MS = 5 * 60_000;

/// In-memory snapshots. The poller is the only writer; readers (the route
/// handler + the WS subscribers) only see consistent post-update state.
const liveSnapshot = new Map();   // icao24        → Feature (live position)
const schedSnapshot = new Map();  // flight_number → Feature (scheduled)

let liveTimer = null;
let schedTimer = null;
let consecutiveFails = 0;
let lastPolledAt = null;
let lastOkAt = null;
let lastStatus = null;
let lastBroadcastCount = 0;
let lastOpenskyCount = null;   // null → not attempted, -1 → failed, ≥0 → row count
let lastAdsbLolCount = null;
let lastSchedCount = 0;
let lastSchedAt = null;

async function pollLive() {
  lastPolledAt = new Date().toISOString();
  let openskyFeatures = null;
  let adsbLolFeatures = null;
  try {
    [openskyFeatures, adsbLolFeatures] = await Promise.all([
      tryOpenSkyAPI(),
      tryAdsbLol(),
    ]);
  } catch (err) {
    // Both helpers swallow their own errors and return null on failure, so
    // a thrown exception here is unexpected — treat it as a hard failure.
    consecutiveFails += 1;
    const code = err?.name === 'TimeoutError' || err?.name === 'AbortError'
      ? 'timeout'
      : (err?.message || String(err));
    lastStatus = `err ${code.slice(0, 80)}`;
    lastOpenskyCount = -1;
    lastAdsbLolCount = -1;
    return;
  }

  const openskyOk = Array.isArray(openskyFeatures);
  const adsbLolOk = Array.isArray(adsbLolFeatures);
  lastOpenskyCount = openskyOk ? openskyFeatures.length : -1;
  lastAdsbLolCount = adsbLolOk ? adsbLolFeatures.length : -1;

  if (!openskyOk && !adsbLolOk) {
    consecutiveFails += 1;
    lastStatus = 'all_sources_failed';
    return;
  }

  // OpenSky first so adsb.lol's fresher position wins on the union, while
  // OpenSky-only fields (origin_country, position_source, …) survive on
  // the merged Feature.
  const merged = mergeLiveByIcao(openskyFeatures, adsbLolFeatures);
  const fresh = new Map();
  for (const f of merged) {
    const icao = f?.properties?.icao24;
    if (!icao) continue;
    // Stamp military classification + canonical id used by iOS popups.
    const tag = classifyMilitary({
      icao24: icao,
      callsign: f.properties.callsign || null,
    });
    f.properties = { ...f.properties, ...tag };
    fresh.set(icao, f);
  }

  // Garbage-collect stale rows: anything not in this poll is dropped from
  // the snapshot. Both upstream feeds drop aircraft once their last contact
  // ages out, so reuse that signal directly instead of timing it ourselves.
  for (const icao of [...liveSnapshot.keys()]) {
    if (!fresh.has(icao)) liveSnapshot.delete(icao);
  }

  let count = 0;
  for (const [icao, feat] of fresh) {
    liveSnapshot.set(icao, feat);
    const [lon, lat] = feat.geometry.coordinates;
    broadcastEvent({
      type: 'live_vehicle',
      kind: 'plane',
      id: icao,
      lat,
      lon,
      heading: feat.properties.heading ?? null,
      speed: feat.properties.ground_speed_knots ?? null,
      label: feat.properties.callsign || icao,
      properties: feat.properties,
      timestamp: lastPolledAt,
    });
    count += 1;
  }

  lastBroadcastCount = count;
  lastOkAt = lastPolledAt;
  lastStatus = `ok ${count} planes (opensky=${lastOpenskyCount} adsblol=${lastAdsbLolCount})`;
  consecutiveFails = 0;
}

async function pollAeroDataBox() {
  try {
    const all = await Promise.all(AERODATABOX_AIRPORTS.map(tryAeroDataBoxAirport));
    const fresh = new Map();
    // Build a normalized-callsign index of the live snapshot so we can drop
    // scheduled rows that collide with airborne flights (live wins).
    const liveCallsigns = new Set();
    for (const lf of liveSnapshot.values()) {
      const k = normalizeCallsign(lf.properties.callsign);
      if (k) liveCallsigns.add(k);
    }
    for (const f of all.flat()) {
      const fn = f?.properties?.flight_number;
      if (!fn) continue;
      const k = normalizeCallsign(fn);
      if (k && liveCallsigns.has(k)) continue;
      fresh.set(fn, f);
    }
    schedSnapshot.clear();
    for (const [k, v] of fresh) schedSnapshot.set(k, v);
    lastSchedCount = schedSnapshot.size;
    lastSchedAt = new Date().toISOString();
  } catch {
    // tryAeroDataBoxAirport already swallows its own errors — nothing to do.
  }
}

function nextLiveDelayMs() {
  if (consecutiveFails === 0) return DEFAULT_INTERVAL_MS;
  return Math.min(MAX_BACKOFF_MS, MIN_BACKOFF_MS * Math.pow(2, Math.max(0, consecutiveFails - 1)));
}

function scheduleLiveNext() {
  liveTimer = setTimeout(async () => {
    liveTimer = null;
    await pollLive();
    if (liveTimer === null) scheduleLiveNext();
  }, nextLiveDelayMs());
}

function scheduleSchedNext() {
  schedTimer = setTimeout(async () => {
    schedTimer = null;
    await pollAeroDataBox();
    if (schedTimer === null) scheduleSchedNext();
  }, SCHED_INTERVAL_MS);
}

/// Single FeatureCollection serving the unified-flights layer. Live rows
/// first (rich properties + heading), then scheduled rows that don't
/// collide. Returns a fresh `features` array each call so callers can mutate
/// it without poisoning the snapshot.
export function getSnapshot() {
  return {
    type: 'FeatureCollection',
    features: [
      ...liveSnapshot.values(),
      ...schedSnapshot.values(),
    ],
    _meta: {
      live_count: liveSnapshot.size,
      sched_count: schedSnapshot.size,
      last_polled_at: lastPolledAt,
      last_sched_at: lastSchedAt,
      last_opensky_count: lastOpenskyCount,
      last_adsblol_count: lastAdsbLolCount,
    },
  };
}

/** Idempotent — re-calling cancels any existing timers first. */
export function startPlanePoller() {
  stopPlanePoller();
  console.log(`[planePoller] starting — live=${DEFAULT_INTERVAL_MS}ms sched=${SCHED_INTERVAL_MS}ms`);
  // Stagger first live poll by 2-7 s so it doesn't pile on top of the
  // GTFS-RT burst, then run the scheduled poll right after the first live
  // one so dedup has a populated liveSnapshot to compare against.
  const jitter = 2000 + Math.floor(Math.random() * 5000);
  liveTimer = setTimeout(async () => {
    liveTimer = null;
    await pollLive();
    if (liveTimer === null) scheduleLiveNext();
    // First scheduled poll fires ~1 s after the first live poll.
    schedTimer = setTimeout(async () => {
      schedTimer = null;
      await pollAeroDataBox();
      if (schedTimer === null) scheduleSchedNext();
    }, 1000);
  }, jitter);
}

export function stopPlanePoller() {
  if (liveTimer)  { clearTimeout(liveTimer);  liveTimer = null; }
  if (schedTimer) { clearTimeout(schedTimer); schedTimer = null; }
}

export function planePollerStatus() {
  return {
    running: liveTimer !== null || schedTimer !== null,
    interval_ms: DEFAULT_INTERVAL_MS,
    sched_interval_ms: SCHED_INTERVAL_MS,
    last_polled_at: lastPolledAt,
    last_ok_at: lastOkAt,
    last_status: lastStatus,
    consecutive_fails: consecutiveFails,
    last_broadcast_count: lastBroadcastCount,
    last_opensky_count: lastOpenskyCount,
    last_adsblol_count: lastAdsbLolCount,
    live_snapshot_size: liveSnapshot.size,
    sched_snapshot_size: schedSnapshot.size,
    last_sched_at: lastSchedAt,
    last_sched_count: lastSchedCount,
  };
}
