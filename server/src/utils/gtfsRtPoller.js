// GTFS Realtime poller. For each row in gtfs_rt_feeds, fetch the protobuf
// on a recurring timer, decode VehiclePositions, and upsert into
// gtfs_rt_positions. A separate TTL sweep drops stale rows.
//
// Every feed gets its own timer so one slow operator can't hold up others.
// Failures trigger exponential backoff; after 10 consecutive fails a feed
// is paused for 1 hour, then retried from scratch.

import db from './database.js';
import { listRtFeeds } from './gtfsStore.js';
import GtfsRtBindings from 'gtfs-realtime-bindings';

const { transit_realtime: { FeedMessage } } = GtfsRtBindings;

const MIN_BACKOFF_MS = 30_000;
const MAX_BACKOFF_MS = 5 * 60_000;
const LONG_PAUSE_MS = 60 * 60_000;
const FAIL_THRESHOLD_FOR_PAUSE = 10;
const TTL_SWEEP_MS = 60_000;
const POSITION_TTL_S = 600;

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

const stmtUpdateFeed = db.prepare(`
  UPDATE gtfs_rt_feeds SET
    last_polled_at    = datetime('now'),
    last_ok_at        = CASE WHEN @ok = 1 THEN datetime('now') ELSE last_ok_at END,
    last_status       = @status,
    consecutive_fails = CASE WHEN @ok = 1 THEN 0 ELSE consecutive_fails + 1 END
  WHERE feed_id = @feed_id
`);

const stmtTtlSweep = db.prepare(
  "DELETE FROM gtfs_rt_positions WHERE reported_at < unixepoch('now') - ?",
);

async function pollOne(feed) {
  const label = `${feed.ag_id}`;
  try {
    const res = await fetch(feed.rt_url, {
      headers: { 'User-Agent': 'japan-osint/1.0', 'Accept': 'application/x-protobuf' },
      // No timeout API on fetch directly — rely on the RT poll cadence being
      // short enough that a hung request self-heals on the next schedule.
    });
    if (!res.ok) {
      stmtUpdateFeed.run({ feed_id: feed.feed_id, ok: 0, status: `HTTP ${res.status}` });
      return { ok: false, reason: `HTTP ${res.status}` };
    }
    const ab = await res.arrayBuffer();
    const msg = FeedMessage.decode(new Uint8Array(ab));
    let count = 0;
    const tx = db.transaction((entities) => {
      for (const ent of entities) {
        const v = ent.vehicle;
        if (!v || !v.trip || !v.trip.trip_id || !v.position) continue;
        const { latitude, longitude, bearing, speed } = v.position;
        if (typeof latitude !== 'number' || typeof longitude !== 'number') continue;
        const tsRaw = v.timestamp ?? msg.header?.timestamp ?? Math.floor(Date.now() / 1000);
        const reportedAt = typeof tsRaw === 'object' && tsRaw !== null && typeof tsRaw.toNumber === 'function'
          ? tsRaw.toNumber()  // Long → number for older protobuf runtimes
          : Number(tsRaw);
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
      }
    });
    tx(msg.entity || []);
    stmtUpdateFeed.run({ feed_id: feed.feed_id, ok: 1, status: `ok ${count} vehicles` });
    return { ok: true, count };
  } catch (err) {
    const reason = err?.message || String(err);
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
    const r = stmtTtlSweep.run(POSITION_TTL_S);
    if (r.changes > 0) {
      console.log(`[gtfsRtPoller] TTL sweep purged ${r.changes} stale positions`);
    }
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
