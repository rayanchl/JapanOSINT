// Compute, for a given wall-clock time, every GTFS trip currently between
// its first-stop departure and last-stop arrival, and project its position
// along its shape_id polyline.
//
// Used by the /api/transit/active-trips HTTP endpoint (Slice C).
import db from './database.js';

const DOW_COLS = ['sun', 'mon', 'tue', 'wed', 'thu', 'fri', 'sat'];

/**
 * Linear interpolation along a shape polyline sorted by dist_m.
 * Clamps to endpoints. Returns `{ lat, lon }` or `null` for empty input.
 */
export function interpolateAlongShape(shape, distM) {
  if (!Array.isArray(shape) || shape.length === 0) return null;
  if (shape.length === 1 || distM <= shape[0].dist_m) {
    return { lat: shape[0].lat, lon: shape[0].lon };
  }
  const last = shape[shape.length - 1];
  if (distM >= last.dist_m) return { lat: last.lat, lon: last.lon };
  for (let i = 1; i < shape.length; i++) {
    if (shape[i].dist_m >= distM) {
      const a = shape[i - 1];
      const b = shape[i];
      const span = b.dist_m - a.dist_m;
      if (span <= 0) return { lat: a.lat, lon: a.lon };
      const t = (distM - a.dist_m) / span;
      return {
        lat: a.lat + t * (b.lat - a.lat),
        lon: a.lon + t * (b.lon - a.lon),
      };
    }
  }
  return { lat: last.lat, lon: last.lon };
}

function ymd(d) {
  return `${d.getFullYear()}${String(d.getMonth() + 1).padStart(2, '0')}${String(d.getDate()).padStart(2, '0')}`;
}

function secOfDay(d) {
  return d.getHours() * 3600 + d.getMinutes() * 60 + d.getSeconds();
}

/**
 * Return the list of trips currently active at `now`, projected to a lat/lon.
 *
 * Each result is `{ trip_id, route_id, route_name, route_color, operator,
 * lat, lon, headsign }`. `trip_id` is globally unique via
 * `${org_id}|${feed_id}|${trip_id}` so the client can dedupe across operators.
 */
export function getActiveTripsAt({ now = new Date(), bbox = null, limit = 500 } = {}) {
  const dow = DOW_COLS[now.getDay()];
  const today = ymd(now);
  const nowSec = secOfDay(now);

  // Oversample from the DB by 4x — bbox filtering drops trips outside the
  // viewport AFTER projection, so we need headroom to hit `limit` inside it.
  const candidates = db.prepare(`
    WITH active_services AS (
      SELECT org_id, feed_id, service_id
      FROM gtfs_calendar
      WHERE ${dow} = 1
        AND (start_date IS NULL OR start_date <= @today)
        AND (end_date   IS NULL OR end_date   >= @today)
    ),
    active_trips AS (
      SELECT t.org_id, t.feed_id, t.trip_id, t.route_id, t.shape_id, t.headsign
      FROM gtfs_trips t
      JOIN active_services s
        ON s.org_id = t.org_id AND s.feed_id = t.feed_id AND s.service_id = t.service_id
    ),
    trip_bounds AS (
      SELECT st.org_id, st.feed_id, st.trip_id,
             MIN(st.departure_sec) AS min_dep,
             MAX(st.arrival_sec)   AS max_arr
      FROM gtfs_stop_times st
      JOIN active_trips a USING (org_id, feed_id, trip_id)
      GROUP BY st.org_id, st.feed_id, st.trip_id
    )
    SELECT a.org_id, a.feed_id, a.trip_id, a.route_id, a.shape_id, a.headsign,
           tb.min_dep, tb.max_arr
    FROM active_trips a
    JOIN trip_bounds tb USING (org_id, feed_id, trip_id)
    WHERE tb.min_dep <= @nowSec AND tb.max_arr >= @nowSec
    LIMIT @sampleLimit
  `).all({ today, nowSec, sampleLimit: limit * 4 });

  const stmtStops = db.prepare(`
    SELECT stop_sequence, arrival_sec, departure_sec, shape_dist_traveled
    FROM gtfs_stop_times
    WHERE org_id = ? AND feed_id = ? AND trip_id = ?
    ORDER BY stop_sequence ASC
  `);
  const stmtShape = db.prepare(`
    SELECT lat, lon, dist_m
    FROM gtfs_shapes
    WHERE org_id = ? AND feed_id = ? AND shape_id = ?
    ORDER BY seq ASC
  `);
  const stmtRoute = db.prepare(`
    SELECT short_name, long_name, color
    FROM gtfs_routes
    WHERE org_id = ? AND feed_id = ? AND route_id = ?
  `);

  const out = [];
  for (const c of candidates) {
    if (out.length >= limit) break;
    const stops = stmtStops.all(c.org_id, c.feed_id, c.trip_id);
    if (stops.length < 2) continue;

    // Find the bracketing pair: prev.dep <= nowSec < next.arr
    let prevIdx = -1;
    for (let i = 0; i < stops.length - 1; i++) {
      const dep = stops[i].departure_sec ?? stops[i].arrival_sec;
      const nextArr = stops[i + 1].arrival_sec ?? stops[i + 1].departure_sec;
      if (dep == null || nextArr == null) continue;
      if (dep <= nowSec && nowSec < nextArr) { prevIdx = i; break; }
    }
    if (prevIdx < 0) continue;

    const prev = stops[prevIdx];
    const next = stops[prevIdx + 1];
    const prevT = prev.departure_sec ?? prev.arrival_sec;
    const nextT = next.arrival_sec ?? next.departure_sec;
    const span = Math.max(1, nextT - prevT);
    const ratio = Math.max(0, Math.min(1, (nowSec - prevT) / span));

    const prevDist = prev.shape_dist_traveled;
    const nextDist = next.shape_dist_traveled;
    if (prevDist == null || nextDist == null) continue;
    const distM = prevDist + ratio * (nextDist - prevDist);

    const shape = stmtShape.all(c.org_id, c.feed_id, c.shape_id);
    if (shape.length < 2) continue;
    const pos = interpolateAlongShape(shape, distM);
    if (!pos) continue;

    if (bbox) {
      if (pos.lon < bbox.minLng || pos.lon > bbox.maxLng ||
          pos.lat < bbox.minLat || pos.lat > bbox.maxLat) continue;
    }

    const route = stmtRoute.get(c.org_id, c.feed_id, c.route_id) || {};
    out.push({
      trip_id: `${c.org_id}|${c.feed_id}|${c.trip_id}`,
      route_id: c.route_id,
      route_name: route.short_name || route.long_name || c.route_id,
      route_color: route.color ? `#${route.color}` : null,
      operator: c.org_id,
      lat: pos.lat,
      lon: pos.lon,
      headsign: c.headsign || null,
    });
  }
  return out;
}
