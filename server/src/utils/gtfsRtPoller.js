// GTFS Realtime poller. For each row in gtfs_rt_feeds, fetch the protobuf
// on a recurring timer, decode VehiclePositions, and upsert into
// gtfs_rt_positions. A separate TTL sweep drops stale rows.
//
// Every feed gets its own timer so one slow operator can't hold up others.
// Failures trigger exponential backoff; after 10 consecutive fails a feed
// is paused for 1 hour, then retried from scratch.

import db from './database.js';
import { listRtFeeds } from './gtfsStore.js';
import { getBroadcaster } from './collectorTap.js';
import { writeAlertFts, pruneExpiredAlerts } from './gtfsRtAlertsStore.js';
import GtfsRtBindings from 'gtfs-realtime-bindings';

const { transit_realtime: { FeedMessage } } = GtfsRtBindings;

const MIN_BACKOFF_MS = 30_000;
const MAX_BACKOFF_MS = 5 * 60_000;
const LONG_PAUSE_MS = 60 * 60_000;
const FAIL_THRESHOLD_FOR_PAUSE = 10;
const TTL_SWEEP_MS = 60_000;
const POSITION_TTL_S = 600;   // 10 minutes
const ALERT_TTL_S = 3600;     // 60 minutes

const timers = new Map(); // feed_id → NodeJS.Timeout
let sweepTimer = null;

const stmtUpsertPos = db.prepare(`
  INSERT INTO gtfs_rt_positions (
    org_id, trip_id, route_id, lat, lon, bearing, speed_mps,
    reported_at, received_at
  ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, datetime('now'))
  ON CONFLICT(org_id, trip_id) DO UPDATE SET
    route_id    = excluded.route_id,
    lat         = excluded.lat,
    lon         = excluded.lon,
    bearing     = excluded.bearing,
    speed_mps   = excluded.speed_mps,
    reported_at = excluded.reported_at,
    received_at = excluded.received_at
`);

const stmtUpsertTripUpdate = db.prepare(`
  INSERT INTO gtfs_rt_trip_updates (
    org_id, trip_id, route_id,
    stop_id, stop_sequence,
    arrival_delay_s, departure_delay_s,
    reported_at, received_at
  ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, datetime('now'))
  ON CONFLICT(org_id, trip_id, stop_sequence) DO UPDATE SET
    stop_id           = excluded.stop_id,
    arrival_delay_s   = excluded.arrival_delay_s,
    departure_delay_s = excluded.departure_delay_s,
    reported_at       = excluded.reported_at,
    received_at       = excluded.received_at
`);

const stmtUpsertAlert = db.prepare(`
  INSERT INTO gtfs_rt_alerts (
    org_id, alert_id, route_ids, trip_ids, stop_ids,
    header_text, description_text, cause, effect,
    reported_at, received_at
  ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'))
  ON CONFLICT(org_id, alert_id) DO UPDATE SET
    route_ids        = excluded.route_ids,
    trip_ids         = excluded.trip_ids,
    stop_ids         = excluded.stop_ids,
    header_text      = excluded.header_text,
    description_text = excluded.description_text,
    cause            = excluded.cause,
    effect           = excluded.effect,
    reported_at      = excluded.reported_at,
    received_at      = excluded.received_at
`);

const stmtUpdateFeed = db.prepare(`
  UPDATE gtfs_rt_feeds SET
    last_polled_at    = datetime('now'),
    last_ok_at        = CASE WHEN @ok = 1 THEN datetime('now') ELSE last_ok_at END,
    last_status       = @status,
    consecutive_fails = CASE WHEN @ok = 1 THEN 0 ELSE consecutive_fails + 1 END
  WHERE feed_id = @feed_id
`);

const stmtTtlSweepPositions = db.prepare(
  "DELETE FROM gtfs_rt_positions WHERE reported_at < unixepoch('now') - ?",
);
const stmtTtlSweepTripUpdates = db.prepare(
  "DELETE FROM gtfs_rt_trip_updates WHERE reported_at < unixepoch('now') - ?",
);
// gtfs_rt_alerts is swept atomically with its FTS mirror via
// gtfsRtAlertsStore.pruneExpiredAlerts() so orphan FTS rows can't accumulate.

// ── Live-vehicle WS broadcast: route_type lookup + delay/alert enrichment ──
// Resolved at broadcast time so iOS can filter by mode (train/subway/bus) and
// render a delay badge on each marker. Alerts go through the same table that
// odptToGtfsRt.js writes ODPT TrainInformation into, so JR / Tokyo Metro
// disruption text is fused with native GTFS-RT alerts here.

const stmtRouteMeta = db.prepare(`
  SELECT route_type, short_name, long_name
  FROM gtfs_routes
  WHERE org_id = ? AND route_id = ?
  LIMIT 1
`);
const ROUTE_META_CACHE = new Map();

function routeMeta(orgId, routeId) {
  if (!routeId) return null;
  const key = `${orgId}|${routeId}`;
  if (ROUTE_META_CACHE.has(key)) return ROUTE_META_CACHE.get(key);
  const row = stmtRouteMeta.get(orgId, routeId) || null;
  ROUTE_META_CACHE.set(key, row);
  return row;
}

function kindForRouteType(rt) {
  switch (rt) {
    case 1:                          return 'subway';
    case 2:                          return 'train';
    case 3: case 11:                 return 'bus';
    case 0: case 5: case 7: case 12: return 'subway';   // tram/cable/funicular/monorail
    default:                         return null;
  }
}

const stmtLatestTripDelay = db.prepare(`
  SELECT arrival_delay_s, departure_delay_s, stop_sequence
  FROM gtfs_rt_trip_updates
  WHERE org_id = ? AND trip_id = ?
    AND (arrival_delay_s IS NOT NULL OR departure_delay_s IS NOT NULL)
  ORDER BY received_at DESC, stop_sequence ASC
  LIMIT 1
`);

const stmtAlertForRoute = db.prepare(`
  SELECT a.header_text, a.description_text
  FROM gtfs_rt_alerts a, json_each(a.route_ids) je
  WHERE a.org_id = ? AND je.value = ?
  ORDER BY a.reported_at DESC
  LIMIT 1
`);

function broadcastLiveVehicle(feed, v, latitude, longitude, bearing) {
  const wsServer = getBroadcaster();
  if (!wsServer) return;
  const meta = routeMeta(feed.ag_id, v.trip.route_id);
  const kind = kindForRouteType(meta?.route_type);
  if (!kind) return;

  let delaySeconds = null;
  let delayKind = null;
  const td = stmtLatestTripDelay.get(feed.ag_id, v.trip.trip_id);
  if (td) {
    if (td.arrival_delay_s != null) {
      delaySeconds = td.arrival_delay_s; delayKind = 'arrival';
    } else if (td.departure_delay_s != null) {
      delaySeconds = td.departure_delay_s; delayKind = 'departure';
    }
  }

  let alert = null;
  if (v.trip.route_id) {
    alert = stmtAlertForRoute.get(feed.ag_id, v.trip.route_id) || null;
  }

  const payload = {
    type: 'live_vehicle',
    id: `${feed.ag_id}:${v.trip.trip_id}`,
    kind,
    label: meta?.short_name || meta?.long_name || v.trip.route_id || feed.ag_name,
    lat: latitude,
    lon: longitude,
    heading: typeof bearing === 'number' ? bearing : null,
    delay_s: delaySeconds,
    delay_kind: delayKind,
    alert_header: alert?.header_text || null,
    alert_text:   alert?.description_text || null,
  };
  let msg;
  try { msg = JSON.stringify(payload); } catch { return; }
  for (const client of wsServer.clients) {
    if (client.readyState === 1) {
      try { client.send(msg); } catch { /* ignore single-client error */ }
    }
  }
}

function toSeconds(v) {
  if (v == null) return Math.floor(Date.now() / 1000);
  if (typeof v === 'object' && typeof v.toNumber === 'function') return v.toNumber();
  return Number(v);
}

// TranslatedString → best-effort JA/EN text.
function pickTranslation(ts) {
  const ts2 = ts?.translation || [];
  const ja = ts2.find((t) => t.language === 'ja');
  const en = ts2.find((t) => t.language === 'en');
  return (ja || en || ts2[0])?.text || null;
}

async function pollOne(feed) {
  const label = `${feed.ag_id}`;
  try {
    const res = await fetch(feed.rt_url, {
      headers: { 'User-Agent': 'japan-osint/1.0', 'Accept': 'application/x-protobuf' },
      signal: AbortSignal.timeout(15_000),
    });
    if (!res.ok) {
      stmtUpdateFeed.run({ feed_id: feed.feed_id, ok: 0, status: `HTTP ${res.status}` });
      return { ok: false, reason: `HTTP ${res.status}` };
    }
    const ct = (res.headers.get('content-type') || '').toLowerCase();
    if (ct && !/(protobuf|octet-stream)/.test(ct)) {
      stmtUpdateFeed.run({ feed_id: feed.feed_id, ok: 0, status: `bad content-type ${ct.slice(0, 40)}` });
      return { ok: false, reason: `bad content-type ${ct}` };
    }
    const ab = await res.arrayBuffer();
    if (ab.byteLength === 0) {
      stmtUpdateFeed.run({ feed_id: feed.feed_id, ok: 1, status: 'ok empty body' });
      return { ok: true, count: 0 };
    }
    const msg = FeedMessage.decode(new Uint8Array(ab));
    let count = 0;
    const tx = db.transaction((entities) => {
      for (const ent of entities) {
        if (ent.vehicle) {
          const v = ent.vehicle;
          if (!v.trip?.trip_id || !v.position) continue;
          const { latitude, longitude, bearing, speed } = v.position;
          if (typeof latitude !== 'number' || typeof longitude !== 'number') continue;
          const reportedAt = toSeconds(v.timestamp ?? msg.header?.timestamp);
          stmtUpsertPos.run(
            feed.ag_id,
            v.trip.trip_id,
            v.trip.route_id || null,
            latitude,
            longitude,
            typeof bearing === 'number' ? bearing : null,
            typeof speed === 'number' ? speed : null,
            reportedAt,
          );
          count++;
          broadcastLiveVehicle(feed, v, latitude, longitude, bearing);
        } else if (ent.trip_update) {
          const tu = ent.trip_update;
          if (!tu.trip?.trip_id) continue;
          const reportedAt = toSeconds(tu.timestamp ?? msg.header?.timestamp);
          for (const stu of tu.stop_time_update || []) {
            if (stu.stop_sequence == null) continue;
            stmtUpsertTripUpdate.run(
              feed.ag_id,
              tu.trip.trip_id,
              tu.trip.route_id || null,
              stu.stop_id || null,
              stu.stop_sequence,
              stu.arrival?.delay ?? null,
              stu.departure?.delay ?? null,
              reportedAt,
            );
            count++;
          }
        } else if (ent.alert) {
          const a = ent.alert;
          const informedRouteIds = [];
          const informedTripIds = [];
          const informedStopIds = [];
          for (const ie of a.informed_entity || []) {
            if (ie.route_id) informedRouteIds.push(ie.route_id);
            if (ie.trip?.trip_id) informedTripIds.push(ie.trip.trip_id);
            if (ie.stop_id) informedStopIds.push(ie.stop_id);
          }
          const header = pickTranslation(a.header_text);
          const desc = pickTranslation(a.description_text);
          stmtUpsertAlert.run(
            feed.ag_id,
            ent.id,
            JSON.stringify(informedRouteIds),
            JSON.stringify(informedTripIds),
            JSON.stringify(informedStopIds),
            header,
            desc,
            a.cause != null ? String(a.cause) : null,
            a.effect != null ? String(a.effect) : null,
            toSeconds(msg.header?.timestamp),
          );
          writeAlertFts({
            org_id:           feed.ag_id,
            alert_id:         ent.id,
            header_text:      header,
            description_text: desc,
          });
          count++;
        }
      }
    });
    tx(msg.entity || []);
    stmtUpdateFeed.run({ feed_id: feed.feed_id, ok: 1, status: `ok ${count} vehicles` });
    return { ok: true, count };
  } catch (err) {
    const code = err?.cause?.code || err?.code || null;
    const base = err?.name === 'TimeoutError' || err?.name === 'AbortError'
      ? 'timeout'
      : (err?.message || String(err));
    const reason = code ? `${base} (${code})` : base;
    stmtUpdateFeed.run({ feed_id: feed.feed_id, ok: 0, status: `err ${reason.slice(0, 80)}` });
    console.warn(`[gtfsRtPoller] ${label} poll failed: ${reason}`);
    return { ok: false, reason };
  }
}

function scheduleNext(feed, lastResult) {
  // Re-read the feed row to pick up the updated consecutive_fails count.
  const latest = db.prepare(
    'SELECT feed_id, ag_id, rt_url, poll_interval_s, consecutive_fails FROM gtfs_rt_feeds WHERE feed_id = ?',
  ).get(feed.feed_id);
  if (!latest) return;  // feed was removed from the catalogue

  let delayMs;
  if (lastResult.ok) {
    delayMs = Math.max(MIN_BACKOFF_MS, latest.poll_interval_s * 1000);
  } else if (latest.consecutive_fails >= FAIL_THRESHOLD_FOR_PAUSE) {
    delayMs = LONG_PAUSE_MS;
    console.warn(`[gtfsRtPoller] ${latest.ag_id} paused for 1h after ${latest.consecutive_fails} consecutive fails`);
  } else {
    // Exponential backoff capped at MAX_BACKOFF_MS.
    delayMs = Math.min(
      MAX_BACKOFF_MS,
      MIN_BACKOFF_MS * Math.pow(2, Math.max(0, latest.consecutive_fails - 1)),
    );
  }

  const t = setTimeout(async () => {
    timers.delete(latest.feed_id);
    const result = await pollOne(latest);
    scheduleNext(latest, result);
  }, delayMs);
  timers.set(latest.feed_id, t);
}

function runSweep() {
  try {
    const a = stmtTtlSweepPositions.run(POSITION_TTL_S);
    const b = stmtTtlSweepTripUpdates.run(POSITION_TTL_S);
    const c = pruneExpiredAlerts(ALERT_TTL_S);
    const total = (a.changes || 0) + (b.changes || 0) + (c || 0);
    if (total > 0) console.log(`[gtfsRtPoller] TTL swept ${total} stale rows`);
  } catch (err) {
    console.warn('[gtfsRtPoller] sweep failed:', err?.message);
  }
}

/** Kick off polling for every configured feed. Idempotent — re-calling
 * restarts all timers from scratch, which is what we want after a catalogue
 * refresh adds or removes operators. */
export function startRtPoller() {
  stopRtPoller();
  const feeds = listRtFeeds();
  if (feeds.length === 0) {
    console.log('[gtfsRtPoller] no RT feeds configured; idle');
    // Still run the TTL sweep in case someone seeds feeds later via cron.
    sweepTimer = setInterval(runSweep, TTL_SWEEP_MS);
    return;
  }
  console.log(`[gtfsRtPoller] starting ${feeds.length} feed(s)`);
  for (const feed of feeds) {
    // Stagger initial kick by 1-5 s to avoid a burst.
    const jitter = 1000 + Math.floor(Math.random() * 4000);
    const t = setTimeout(async () => {
      timers.delete(feed.feed_id);
      const result = await pollOne(feed);
      scheduleNext(feed, result);
    }, jitter);
    timers.set(feed.feed_id, t);
  }
  sweepTimer = setInterval(runSweep, TTL_SWEEP_MS);
}

/** Cancel every feed timer + the TTL sweep. */
export function stopRtPoller() {
  for (const t of timers.values()) clearTimeout(t);
  timers.clear();
  if (sweepTimer) { clearInterval(sweepTimer); sweepTimer = null; }
}

/** Snapshot for /api/transit/gtfs/rt-status diagnostics (route added later). */
export function rtPollerStatus() {
  return db.prepare(`
    SELECT ag_id, ag_name, rt_url,
           last_polled_at, last_ok_at, last_status, consecutive_fails,
           poll_interval_s
    FROM gtfs_rt_feeds
    ORDER BY ag_id
  `).all();
}
