// Read helpers for the GTFS tables. Paired with gtfsIngest for writes.
// Scope: hydration state + next-departures at a stop.

import db from './database.js';

/** Return true if gtfs_operators row has a hydrated_at timestamp. */
export function isOperatorHydrated(orgId) {
  const row = db.prepare(
    'SELECT hydrated_at FROM gtfs_operators WHERE org_id = ?',
  ).get(orgId);
  return !!row?.hydrated_at;
}

/** Upsert the gtfs_operators row, stamping hydrated_at to now. */
export function markOperatorHydrated(orgId, orgName, feedIds, counts) {
  db.prepare(`
    INSERT INTO gtfs_operators (
      org_id, org_name, hydrated_at, feed_ids, stop_count, trip_count
    ) VALUES (
      ?, ?, datetime('now'), ?, ?, ?
    )
    ON CONFLICT(org_id) DO UPDATE SET
      org_name    = excluded.org_name,
      hydrated_at = excluded.hydrated_at,
      feed_ids    = excluded.feed_ids,
      stop_count  = excluded.stop_count,
      trip_count  = excluded.trip_count
  `).run(
    orgId,
    orgName || orgId,
    JSON.stringify(feedIds || []),
    counts?.stops || 0,
    counts?.trips || 0,
  );
}

// Days of week in the order JS Date.getDay() returns them (0 = Sunday).
const DOW_COLS = ['sun', 'mon', 'tue', 'wed', 'thu', 'fri', 'sat'];

/**
 * Return up to `limit` upcoming departures at `stopId` on the service day
 * implied by `now` (default new Date()).
 *
 * A service is "active" when:
 *   (a) the weekday flag matches the day of `now`, AND
 *   (b) today's YYYYMMDD is within [start_date, end_date] (either may be null).
 *
 * Times returned carry `seconds_until = departure_sec - secOfDay`. Trips
 * whose departure has already passed (more than 60 s ago) are omitted.
 */
export function getDeparturesAt(stopId, now = new Date(), limit = 5) {
  const rows = db.prepare(`
    SELECT st.org_id, st.feed_id, st.trip_id, st.arrival_sec, st.departure_sec,
           st.stop_sequence, t.route_id, t.headsign, t.service_id,
           r.short_name AS route_short, r.long_name AS route_long, r.color AS route_color
    FROM gtfs_stop_times st
    JOIN gtfs_trips   t
      ON t.org_id = st.org_id AND t.feed_id = st.feed_id AND t.trip_id = st.trip_id
    LEFT JOIN gtfs_routes r
      ON r.org_id = t.org_id AND r.feed_id = t.feed_id AND r.route_id = t.route_id
    WHERE st.stop_id = ?
  `).all(stopId);

  const dowCol = DOW_COLS[now.getDay()];
  const ymd = `${now.getFullYear()}${String(now.getMonth() + 1).padStart(2, '0')}${String(now.getDate()).padStart(2, '0')}`;
  const secOfDay = now.getHours() * 3600 + now.getMinutes() * 60 + now.getSeconds();

  // Cache service activity lookups so each (org, feed, service) hits the DB
  // at most once per call.
  const svcCache = new Map();
  const isServiceActive = (orgId, feedId, serviceId) => {
    const key = `${orgId}|${feedId}|${serviceId}`;
    if (svcCache.has(key)) return svcCache.get(key);
    const cal = db.prepare(`
      SELECT ${DOW_COLS.map(c => c).join(', ')}, start_date, end_date
      FROM gtfs_calendar
      WHERE org_id = ? AND feed_id = ? AND service_id = ?
    `).get(orgId, feedId, serviceId);
    if (!cal) { svcCache.set(key, false); return false; }
    const inRange =
      (!cal.start_date || ymd >= cal.start_date) &&
      (!cal.end_date   || ymd <= cal.end_date);
    const runsToday = cal[dowCol] === 1;
    const active = inRange && runsToday;
    svcCache.set(key, active);
    return active;
  };

  const out = [];
  for (const r of rows) {
    if (!isServiceActive(r.org_id, r.feed_id, r.service_id)) continue;
    const tSec = r.departure_sec ?? r.arrival_sec;
    if (tSec == null) continue;
    // GTFS lets departure_sec exceed 86400 (service day crosses midnight).
    // The canonical wall-clock offset within the "now" day is tSec % 86400.
    // We compute seconds_until by normalizing: if the trip already passed
    // in wall-clock terms, assume it's tomorrow (add 86400). Trips that
    // passed more than 60 s ago in the current day get skipped.
    const wallSec = tSec % 86400;
    let secondsUntil = wallSec - secOfDay;
    if (secondsUntil + 60 < 0) continue; // already passed
    if (secondsUntil < 0) secondsUntil += 86400; // within grace window
    out.push({
      trip_id: r.trip_id,
      route_id: r.route_id,
      headsign: r.headsign || null,
      route_name: r.route_short || r.route_long || r.route_id,
      route_color: r.route_color ? `#${r.route_color}` : null,
      departure_sec: tSec,
      seconds_until: secondsUntil,
    });
  }
  out.sort((a, b) => a.seconds_until - b.seconds_until);
  return out.slice(0, limit);
}

/** Return all hydrated operators (one row each). */
export function listHydratedOperators() {
  return db.prepare(`
    SELECT org_id, org_name, hydrated_at, stop_count, trip_count
    FROM gtfs_operators
    ORDER BY hydrated_at DESC
  `).all();
}

/** Fetch the operator catalogue from gtfs-data.jp and return a list of org_ids. */
export async function listUpstreamOperatorIds() {
  const res = await fetch('https://api.gtfs-data.jp/v2/organizations');
  if (!res.ok) throw new Error(`organizations HTTP ${res.status}`);
  const body = await res.json();
  const list = Array.isArray(body?.body) ? body.body : [];
  return list
    .map((o) => o.organization_id || o.id || o.organizationID)
    .filter(Boolean);
}

/**
 * Return org_ids whose `hydrated_at` is NULL or older than `ageDays`,
 * ordered NULL-first then oldest-hydrated-first.
 */
export function listStaleOperatorIds(ageDays) {
  const cutoff = new Date(Date.now() - ageDays * 24 * 3600 * 1000).toISOString();
  return db.prepare(`
    SELECT org_id FROM gtfs_operators
    WHERE hydrated_at IS NULL OR hydrated_at < ?
    ORDER BY hydrated_at IS NULL DESC, hydrated_at ASC
  `).all(cutoff).map((r) => r.org_id);
}
