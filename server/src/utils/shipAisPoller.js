/**
 * Live ship position poller.
 *
 * Polls one of two AIS aggregators at a fixed interval and re-broadcasts
 * each vessel as a `live_vehicle` WS event keyed by MMSI — same envelope
 * shape as gtfsRtPoller emits for trains/subways/buses, so the iOS
 * LiveVehiclesOverlay animates the `kind:'ship'` branch automatically.
 *
 * Source preference: MarineTraffic (if MARINETRAFFIC_API_KEY set) →
 * VesselFinder (if VESSELFINDER_API_KEY set) → no-op (poller exits silently).
 * Both APIs are key-gated and have aggressive rate limits, so the default
 * interval is 5 minutes (env-tunable via SHIP_POLLER_INTERVAL_MS).
 *
 * Same backoff shape as gtfsRtPoller and planeAdsbPoller.
 */

import { broadcastEvent } from './collectorTap.js';

const DEFAULT_INTERVAL_MS = Number(process.env.SHIP_POLLER_INTERVAL_MS || 5 * 60_000);
const MIN_BACKOFF_MS = 60_000;
const MAX_BACKOFF_MS = 15 * 60_000;

const MARINETRAFFIC_KEY = () => process.env.MARINETRAFFIC_API_KEY || '';
const VESSELFINDER_KEY = () => process.env.VESSELFINDER_API_KEY || '';

const MARINETRAFFIC_URL = (k) =>
  `https://services.marinetraffic.com/api/exportvessels/v:8/${k}/protocol:jsono/minlat:24/maxlat:46/minlon:122/maxlon:154`;
const VESSELFINDER_URL = (k) =>
  `https://api.vesselfinder.com/vesselslist?userkey=${k}&bbox=122,24,154,46&format=json`;

let timer = null;
let consecutiveFails = 0;
let lastPolledAt = null;
let lastOkAt = null;
let lastStatus = null;
let lastBroadcastCount = 0;
let activeSource = null;

async function fetchMarineTraffic() {
  const key = MARINETRAFFIC_KEY();
  if (!key) return null;
  const res = await fetch(MARINETRAFFIC_URL(key), { signal: AbortSignal.timeout(10_000) });
  if (!res.ok) throw new Error(`MarineTraffic HTTP ${res.status}`);
  const data = await res.json();
  if (!Array.isArray(data)) throw new Error('MarineTraffic: not an array');
  return data.map((v) => ({
    mmsi: String(v.MMSI || ''),
    lat: parseFloat(v.LAT),
    lon: parseFloat(v.LON),
    heading: typeof v.HEADING === 'number' ? v.HEADING : (v.HEADING != null ? parseFloat(v.HEADING) : null),
    // MarineTraffic SPEED is tenths of knots; normalize to knots.
    speed_knots: v.SPEED != null ? parseFloat(v.SPEED) / 10 : null,
    label: v.SHIPNAME || null,
    source: 'marinetraffic',
  }));
}

async function fetchVesselFinder() {
  const key = VESSELFINDER_KEY();
  if (!key) return null;
  const res = await fetch(VESSELFINDER_URL(key), { signal: AbortSignal.timeout(10_000) });
  if (!res.ok) throw new Error(`VesselFinder HTTP ${res.status}`);
  const data = await res.json();
  if (!Array.isArray(data)) throw new Error('VesselFinder: not an array');
  return data.map((v) => ({
    mmsi: String(v.MMSI || ''),
    lat: parseFloat(v.LATITUDE),
    lon: parseFloat(v.LONGITUDE),
    heading: typeof v.COURSE === 'number' ? v.COURSE : (v.COURSE != null ? parseFloat(v.COURSE) : null),
    speed_knots: v.SPEED != null ? parseFloat(v.SPEED) : null,
    label: v.NAME || null,
    source: 'vesselfinder',
  }));
}

async function pollOnce() {
  if (!MARINETRAFFIC_KEY() && !VESSELFINDER_KEY()) {
    lastStatus = 'no_keys_configured';
    return;
  }
  try {
    const vessels = (await fetchMarineTraffic()) || (await fetchVesselFinder()) || [];
    activeSource = vessels[0]?.source || null;
    lastPolledAt = new Date().toISOString();

    let count = 0;
    for (const v of vessels) {
      if (!v.mmsi || !Number.isFinite(v.lat) || !Number.isFinite(v.lon)) continue;
      broadcastEvent({
        type: 'live_vehicle',
        kind: 'ship',
        id: v.mmsi,
        lat: v.lat,
        lon: v.lon,
        heading: Number.isFinite(v.heading) ? v.heading : null,
        speed: Number.isFinite(v.speed_knots) ? v.speed_knots : null,
        label: v.label || v.mmsi,
        timestamp: lastPolledAt,
      });
      count += 1;
    }
    lastBroadcastCount = count;
    lastOkAt = lastPolledAt;
    lastStatus = `ok ${count} vessels via ${activeSource}`;
    consecutiveFails = 0;
  } catch (err) {
    consecutiveFails += 1;
    lastStatus = `err ${(err?.message || String(err)).slice(0, 80)}`;
    lastPolledAt = new Date().toISOString();
  }
}

function nextDelayMs() {
  if (consecutiveFails === 0) return DEFAULT_INTERVAL_MS;
  return Math.min(MAX_BACKOFF_MS, MIN_BACKOFF_MS * Math.pow(2, Math.max(0, consecutiveFails - 1)));
}

function scheduleNext() {
  timer = setTimeout(async () => {
    timer = null;
    await pollOnce();
    if (timer === null) scheduleNext();
  }, nextDelayMs());
}

/** Idempotent — re-calling cancels any existing timer first. */
export function startShipPoller() {
  stopShipPoller();
  if (!MARINETRAFFIC_KEY() && !VESSELFINDER_KEY()) {
    console.log('[shipPoller] no AIS API key set; idle');
    return;
  }
  console.log(`[shipPoller] starting — interval=${DEFAULT_INTERVAL_MS}ms`);
  const jitter = 5000 + Math.floor(Math.random() * 10_000);
  timer = setTimeout(async () => {
    timer = null;
    await pollOnce();
    if (timer === null) scheduleNext();
  }, jitter);
}

export function stopShipPoller() {
  if (timer) { clearTimeout(timer); timer = null; }
}

export function shipPollerStatus() {
  return {
    running: timer !== null,
    interval_ms: DEFAULT_INTERVAL_MS,
    active_source: activeSource,
    last_polled_at: lastPolledAt,
    last_ok_at: lastOkAt,
    last_status: lastStatus,
    consecutive_fails: consecutiveFails,
    last_broadcast_count: lastBroadcastCount,
  };
}
