/**
 * ODPT StationTimetable lazy ingest.
 *
 * Fetched once per station on the first click in the station popup, stored in
 * `odpt_station_timetable`. No TTL refresh — re-ingesting requires deleting
 * the row in `odpt_station_timetable_fetched`. Published ODPT schedules only
 * change on timetable-revision days (a few times a year), so once-per-install
 * is fine until we have a reason to invalidate.
 *
 * Token: ODPT_TOKEN | ODPT_CONSUMER_KEY | ODPT_CHALLENGE_TOKEN. No-op when
 * unset.
 *
 * Input shape the route passes in: an ODPT station id like
 *   odpt.Station:JR-East.Yamanote.Tokyo
 * which we forward to `odpt:StationTimetable?odpt:station=<id>`.
 */

import db from './database.js';
import { getOdptToken } from './odptAuth.js';

const ENDPOINT_PROD      = 'https://api.odpt.org/api/v4/odpt:StationTimetable';
const ENDPOINT_CHALLENGE = 'https://api-challenge.odpt.org/api/v4/odpt:StationTimetable';
const FETCH_TIMEOUT_MS = 15_000;

const stmtFetchedGet = db.prepare(
  'SELECT station_id FROM odpt_station_timetable_fetched WHERE station_id = ?',
);
const stmtFetchedSet = db.prepare(
  `INSERT INTO odpt_station_timetable_fetched (station_id, fetched_at, entry_count)
   VALUES (?, datetime('now'), ?)
   ON CONFLICT(station_id) DO UPDATE SET
     fetched_at = excluded.fetched_at,
     entry_count = excluded.entry_count`,
);
const stmtInsertEntry = db.prepare(`
  INSERT OR REPLACE INTO odpt_station_timetable (
    station_id, line_id, calendar, direction, seq,
    departure_time, destination_ja, destination_en,
    train_type, train_name, is_last, is_origin, org_id
  ) VALUES (
    @station_id, @line_id, @calendar, @direction, @seq,
    @departure_time, @destination_ja, @destination_en,
    @train_type, @train_name, @is_last, @is_origin, @org_id
  )
`);

// In-flight dedupe: if two popup clicks race on the same station, only one
// actual fetch goes out. Resolves to the same result for both callers.
const inflight = new Map();

export function hasTimetableForStation(stationId) {
  return !!stmtFetchedGet.get(stationId);
}

function stripPrefix(s, prefix) {
  if (typeof s !== 'string') return null;
  return s.startsWith(prefix) ? s.slice(prefix.length) : s;
}

function firstString(...vals) {
  for (const v of vals) {
    if (typeof v === 'string' && v.length) return v;
  }
  return null;
}

// ODPT StationTimetable documents don't come in multi-language objects for
// most fields, but destination titles do: { ja: '...', en: '...' }.
function lang(obj, key) {
  const ja = obj?.[key]?.ja ?? null;
  const en = obj?.[key]?.en ?? null;
  return { ja, en };
}

async function fetchOdptStationTimetable(stationId, token) {
  const q = `?odpt:station=${encodeURIComponent(stationId)}&acl:consumerKey=${encodeURIComponent(token)}`;
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), FETCH_TIMEOUT_MS);
  try {
    // Prod first (most registered tokens work there); fall back to challenge.
    for (const base of [ENDPOINT_PROD, ENDPOINT_CHALLENGE]) {
      try {
        const res = await fetch(`${base}${q}`, {
          signal: controller.signal,
          headers: { Accept: 'application/json', 'User-Agent': 'JapanOSINT/1.0' },
        });
        if (res.ok) {
          const body = await res.json();
          return Array.isArray(body) ? body : [];
        }
      } catch { /* try next base */ }
    }
    return null;
  } finally {
    clearTimeout(timer);
  }
}

/**
 * Ingest ONE station's timetable. Idempotent — returns { cached: true } if
 * we've already ingested it before. No-op (returns { skipped: 'no token' })
 * when no ODPT token is configured.
 */
export async function ingestStationTimetable(stationId) {
  if (typeof stationId !== 'string' || !stationId.startsWith('odpt.Station:')) {
    return { skipped: 'not an ODPT station id' };
  }
  if (hasTimetableForStation(stationId)) return { cached: true };
  const existing = inflight.get(stationId);
  if (existing) return existing;

  const token = getOdptToken();
  if (!token) return { skipped: 'no ODPT token' };

  const p = (async () => {
    const docs = await fetchOdptStationTimetable(stationId, token);
    if (!docs) return { skipped: 'upstream unreachable' };
    if (docs.length === 0) {
      // Mark as fetched so we don't re-poll stations that legitimately have
      // no timetable (e.g. terminus-only records, operators not publishing).
      stmtFetchedSet.run(stationId, 0);
      return { entries: 0 };
    }

    let entries = 0;
    const tx = db.transaction(() => {
      for (const doc of docs) {
        const lineId = stripPrefix(doc['odpt:railway'], '');
        const calendar = stripPrefix(doc['odpt:calendar'], '');
        const direction = stripPrefix(doc['odpt:railDirection'], '');
        const operatorRaw = doc['odpt:operator'] || '';
        const orgId = typeof operatorRaw === 'string'
          ? operatorRaw.replace(/^odpt\.Operator:/, '')
          : null;
        const arr = Array.isArray(doc['odpt:stationTimetableObject'])
          ? doc['odpt:stationTimetableObject']
          : [];
        for (let i = 0; i < arr.length; i++) {
          const e = arr[i];
          const dest = lang(e, 'odpt:destinationStation');
          // ODPT sometimes uses `odpt:destinationStationTitle` as a string; try both.
          const destFallback = e['odpt:destinationStationTitle'];
          const destJa = firstString(dest.ja, typeof destFallback === 'string' ? destFallback : null);
          const destEn = dest.en;

          stmtInsertEntry.run({
            station_id: stationId,
            line_id: lineId,
            calendar,
            direction,
            seq: i,
            departure_time: e['odpt:departureTime'] ?? e['odpt:arrivalTime'] ?? null,
            destination_ja: destJa,
            destination_en: destEn,
            train_type: stripPrefix(e['odpt:trainType'], '') || null,
            train_name: e['odpt:trainName']?.ja ?? e['odpt:trainName']?.en ?? null,
            is_last: e['odpt:isLast'] ? 1 : 0,
            is_origin: e['odpt:isOrigin'] ? 1 : 0,
            org_id: orgId,
          });
          entries++;
        }
      }
      stmtFetchedSet.run(stationId, entries);
    });
    tx();

    return { entries };
  })();

  inflight.set(stationId, p);
  try { return await p; }
  finally { inflight.delete(stationId); }
}

/**
 * Read the stored timetable for one station, optionally filtered to a
 * calendar (e.g. 'Weekday', 'SaturdayHoliday') and trimmed to entries at or
 * after a given HH:MM cutoff. Returns entries sorted by departure_time.
 */
const stmtRead = db.prepare(`
  SELECT station_id, line_id, calendar, direction, seq,
         departure_time, destination_ja, destination_en,
         train_type, train_name, is_last, is_origin, org_id
  FROM odpt_station_timetable
  WHERE station_id = ?
  ORDER BY departure_time ASC, seq ASC
`);

export function getStationTimetable(stationId, { calendar = null, afterHHMM = null, limit = 20 } = {}) {
  const rows = stmtRead.all(stationId);
  const filtered = [];
  for (const r of rows) {
    if (calendar && r.calendar && !r.calendar.endsWith(calendar)) continue;
    if (afterHHMM && r.departure_time && r.departure_time < afterHHMM) continue;
    filtered.push(r);
    if (filtered.length >= limit) break;
  }
  return filtered;
}
