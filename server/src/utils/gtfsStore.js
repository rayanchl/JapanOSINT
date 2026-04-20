// Read helpers for the GTFS tables. Paired with gtfsIngest for writes.
// Scope: hydration state + next-departures at a stop.

import db from './database.js';
import { parseCsv } from './gtfsIngest.js';

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

export async function listUpstreamOperatorIds() {
  const rows = db.prepare(`
    SELECT DISTINCT ag_id FROM gtfs_feeds
    WHERE ag_id IS NOT NULL
      AND fixed_current_url IS NOT NULL
      AND api_key_required = 0
    ORDER BY ag_id
  `).all();
  return rows.map((r) => r.ag_id);
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

const SHIMADA_CATALOGUE_URL = 'https://tshimada291.sakura.ne.jp/transport/dat/GTFS_opendata_jp_catalog.csv';

/**
 * Fetch the T.Shimada authoritative Japanese GTFS catalogue and upsert rows
 * into gtfs_feeds. Called at boot and weekly via cron. Safe to call repeatedly
 * — each row is keyed by feed_id so re-running refreshes URLs in place.
 *
 * Returns { total }.
 */
export async function refreshFeedCatalogue() {
  const res = await fetch(SHIMADA_CATALOGUE_URL, {
    headers: { 'User-Agent': 'japan-osint/1.0' },
  });
  if (!res.ok) throw new Error(`catalogue HTTP ${res.status}`);
  const text = await res.text();
  const { rows } = parseCsv(text);

  const upsert = db.prepare(`
    INSERT INTO gtfs_feeds (
      feed_id, ag_id, ag_name, pref_code, pref_name, feed_name,
      fixed_current_url, license_name, license_url,
      api_key_required, feed_end_date,
      rt_catalog_url, rt_api_key_required, rt_status,
      last_refreshed_at
    ) VALUES (
      @feed_id, @ag_id, @ag_name, @pref_code, @pref_name, @feed_name,
      @fixed_current_url, @license_name, @license_url,
      @api_key_required, @feed_end_date,
      @rt_catalog_url, @rt_api_key_required, @rt_status,
      datetime('now')
    )
    ON CONFLICT(feed_id) DO UPDATE SET
      ag_id               = excluded.ag_id,
      ag_name             = excluded.ag_name,
      pref_code           = excluded.pref_code,
      pref_name           = excluded.pref_name,
      feed_name           = excluded.feed_name,
      fixed_current_url   = excluded.fixed_current_url,
      license_name        = excluded.license_name,
      license_url         = excluded.license_url,
      api_key_required    = excluded.api_key_required,
      feed_end_date       = excluded.feed_end_date,
      rt_catalog_url      = excluded.rt_catalog_url,
      rt_api_key_required = excluded.rt_api_key_required,
      rt_status           = excluded.rt_status,
      last_refreshed_at   = datetime('now')
  `);

  let count = 0;
  const tx = db.transaction((entries) => {
    for (const r of entries) {
      if (!r.feed_id) continue;
      upsert.run({
        feed_id: r.feed_id,
        ag_id: r.ag_id || null,
        ag_name: r.ag_name || null,
        pref_code: r.prefcode || null,
        pref_name: r.prefname || null,
        feed_name: r.feed_name || r.catalog_name || null,
        fixed_current_url: r.fixed_current_url || null,
        license_name: r.license_name || null,
        license_url: r.license_url || null,
        api_key_required: /^[1-9]/.test(r.api_key || '0') ? 1 : 0,
        feed_end_date: r.feed_end_date || null,
        rt_catalog_url: r.rt_catalog_url || null,
        rt_api_key_required: /^[1-9]/.test(r.rt_api_key || '0') ? 1 : 0,
        rt_status: r.rt_status || null,
      });
      count++;
    }
  });
  tx(rows);
  return { total: count };
}

/** Return feeds for one agency. */
export function getAgencyFeeds(agId) {
  return db.prepare(`
    SELECT feed_id, fixed_current_url, api_key_required, rt_catalog_url, rt_api_key_required
    FROM gtfs_feeds
    WHERE ag_id = ?
  `).all(agId);
}

// Map of Shimada ag_id → ODPT agency code used in the GTFS-RT URL
// api.odpt.org/api/v4/gtfs/realtime/<CODE>?acl:consumerKey=<TOKEN>.
// Hand-maintained from ckan.odpt.org dataset slugs.
const ODPT_AGENCY_MAP = {
  'a13001': 'ToeiBus',
  'a13101': 'KeiseiBus',
  'a33208': 'UnoBus',
  'a02201': 'AomoriCityBus',
  'a10205': 'NagaiTransportation',
};

/**
 * Refresh the gtfs_rt_feeds table. For every agency in gtfs_feeds whose
 * rt_catalog_url references ckan.odpt.org AND whose ag_id is in the
 * ODPT_AGENCY_MAP, upsert an entry with the canonical ODPT RT URL.
 *
 * Requires ODPT_CHALLENGE_TOKEN (or ODPT_TOKEN / ODPT_CONSUMER_KEY) in env;
 * otherwise this is a no-op and the poller will run against nothing.
 *
 * Returns { seeded, skipped, reason? }.
 */
export function refreshRtFeedCatalogue() {
  const token =
    process.env.ODPT_CHALLENGE_TOKEN ||
    process.env.ODPT_TOKEN ||
    process.env.ODPT_CONSUMER_KEY ||
    null;
  if (!token) {
    return { seeded: 0, skipped: 0, reason: 'no ODPT_CHALLENGE_TOKEN in env' };
  }

  const odptRows = db.prepare(`
    SELECT feed_id, ag_id, ag_name
    FROM gtfs_feeds
    WHERE rt_catalog_url LIKE '%ckan.odpt.org%'
      AND ag_id IN (${Object.keys(ODPT_AGENCY_MAP).map(() => '?').join(', ') || "''"})
  `).all(...Object.keys(ODPT_AGENCY_MAP));

  const upsert = db.prepare(`
    INSERT INTO gtfs_rt_feeds
      (feed_id, ag_id, ag_name, rt_url, poll_interval_s)
    VALUES (?, ?, ?, ?, 30)
    ON CONFLICT(feed_id) DO UPDATE SET
      ag_id   = excluded.ag_id,
      ag_name = excluded.ag_name,
      rt_url  = excluded.rt_url
  `);

  let seeded = 0;
  let skipped = 0;
  const tx = db.transaction(() => {
    for (const r of odptRows) {
      const code = ODPT_AGENCY_MAP[r.ag_id];
      if (!code) { skipped++; continue; }
      const rtUrl = `https://api.odpt.org/api/v4/gtfs/realtime/${code}?acl:consumerKey=${encodeURIComponent(token)}`;
      upsert.run(r.feed_id, r.ag_id, r.ag_name, rtUrl);
      seeded++;
    }
  });
  tx();
  return { seeded, skipped };
}

/** Return all configured RT feed rows. */
export function listRtFeeds() {
  return db.prepare(`
    SELECT feed_id, ag_id, ag_name, rt_url, poll_interval_s,
           last_polled_at, last_ok_at, last_status, consecutive_fails
    FROM gtfs_rt_feeds
    ORDER BY ag_id
  `).all();
}
