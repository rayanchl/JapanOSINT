/**
 * Snap every station in a given mode to the color of the nearest line track
 * within a short radius (default 300 m).
 *
 * Why: stations and tracks come from independent OSM / MLIT / ODPT sources
 * that disagree on operator and line strings ("JR" vs "JR East" vs
 * "東日本旅客鉄道"), so hashing tags yields inconsistent colors between a
 * station and its own line. Spatial proximity is the only reliable link.
 *
 * Strategy:
 *   1. Load lines-with-color once; project every segment (pair of adjacent
 *      vertices) into a 0.01°-cell bucket index (~1.1 km).
 *   2. For each station, scan its own cell + 8 neighbors. Compute
 *      point-to-segment distance in equirectangular metres. Keep smallest.
 *   3. If min distance ≤ radius, emit { station_uid, color }.
 *   4. One transactional batch UPDATE patches each station's
 *      properties.line_color in place.
 */

import {
  getLinesByMode,
  getStationsByMode,
  updateStationColorsTx,
} from './transportStore.js';

const DEFAULT_RADIUS_M = 300;
const CELL_SIZE_DEG = 0.01;
const METERS_PER_DEG_LAT = 111_320;

function cellKey(lon, lat) {
  const cx = Math.floor(lon / CELL_SIZE_DEG);
  const cy = Math.floor(lat / CELL_SIZE_DEG);
  return `${cx}:${cy}`;
}

/**
 * Squared distance in metres from point (px,py) to segment (ax,ay)-(bx,by),
 * using an equirectangular projection. Good enough at Japan latitudes and
 * 300 m scale; avoids pulling in a geo library.
 */
function segmentDistSqM(px, py, ax, ay, bx, by) {
  // Anchor the equirectangular projection at the segment midpoint so the
  // lon-scaling is reasonable regardless of which vertex is closer.
  const midLat = (ay + by) * 0.5;
  const cosLat = Math.cos((midLat * Math.PI) / 180);
  const ex = (bx - ax) * cosLat * METERS_PER_DEG_LAT;
  const ey = (by - ay) * METERS_PER_DEG_LAT;
  const dx = (px - ax) * cosLat * METERS_PER_DEG_LAT;
  const dy = (py - ay) * METERS_PER_DEG_LAT;

  const segLenSq = ex * ex + ey * ey;
  let t = segLenSq === 0 ? 0 : (dx * ex + dy * ey) / segLenSq;
  if (t < 0) t = 0; else if (t > 1) t = 1;

  const projX = t * ex;
  const projY = t * ey;
  const ddx = dx - projX;
  const ddy = dy - projY;
  return ddx * ddx + ddy * ddy;
}

/**
 * Same as segmentDistSqM but also returns the closest (lon, lat) on the
 * segment — used to "snap" a station's dot onto the line geometry so each
 * line's pin sits exactly ON its track instead of all dots piling at the
 * station centroid.
 */
function closestPointOnSegment(px, py, ax, ay, bx, by) {
  const midLat = (ay + by) * 0.5;
  const cosLat = Math.cos((midLat * Math.PI) / 180);
  const ex = (bx - ax) * cosLat * METERS_PER_DEG_LAT;
  const ey = (by - ay) * METERS_PER_DEG_LAT;
  const dx = (px - ax) * cosLat * METERS_PER_DEG_LAT;
  const dy = (py - ay) * METERS_PER_DEG_LAT;
  const segLenSq = ex * ex + ey * ey;
  let t = segLenSq === 0 ? 0 : (dx * ex + dy * ey) / segLenSq;
  if (t < 0) t = 0; else if (t > 1) t = 1;
  // Linear interpolation in lon/lat degrees works fine at the sub-300m
  // scale we snap within.
  const snappedLon = ax + t * (bx - ax);
  const snappedLat = ay + t * (by - ay);
  const ddx = dx - t * ex;
  const ddy = dy - t * ey;
  return { lon: snappedLon, lat: snappedLat, distSqM: ddx * ddx + ddy * ddy };
}

/**
 * Index all colored line segments into a cell map. Each segment is written
 * into every cell its endpoints touch (usually just one).
 */
function buildSegmentIndex(lines) {
  const index = new Map();
  let segCount = 0;
  for (const line of lines) {
    const color = line.properties?.line_color;
    if (!color) continue;
    const coords = line.geometry?.coordinates;
    if (!Array.isArray(coords) || coords.length < 2) continue;
    // Stamp each segment with its owning way id so the per-track snap can
    // tell up-direction rail from down-direction rail (same colour,
    // different way_uid = two parallel LineStrings).
    const wayUid = line.properties?.line_uid || line.properties?.way_id || null;
    for (let i = 0; i < coords.length - 1; i++) {
      const a = coords[i];
      const b = coords[i + 1];
      if (!Array.isArray(a) || !Array.isArray(b)) continue;
      const seg = { ax: a[0], ay: a[1], bx: b[0], by: b[1], color, way_uid: wayUid };
      const keyA = cellKey(a[0], a[1]);
      const keyB = cellKey(b[0], b[1]);
      pushIntoCell(index, keyA, seg);
      if (keyB !== keyA) pushIntoCell(index, keyB, seg);
      segCount++;
    }
  }
  return { index, segCount };
}

function pushIntoCell(index, key, seg) {
  let arr = index.get(key);
  if (!arr) { arr = []; index.set(key, arr); }
  arr.push(seg);
}

/**
 * Snap every station of a given mode to its nearest colored line.
 *
 * @param {'train'|'subway'} mode
 * @param {object} [opts]
 * @param {number} [opts.radiusM=300] max match distance in metres
 * @returns {{mode, stations, linesWithColor, segments, matched, unmatched, changed}}
 */
export function snapStationsToNearestLine(mode, opts = {}) {
  const radiusM = opts.radiusM || DEFAULT_RADIUS_M;
  const radiusSq = radiusM * radiusM;

  const lines = getLinesByMode(mode);
  const colored = lines.filter((l) => l.properties?.line_color);
  const { index, segCount } = buildSegmentIndex(colored);

  const stations = getStationsByMode(mode);
  const updates = [];
  let matched = 0;
  let unmatched = 0;

  for (const st of stations) {
    const coords = st.geometry?.coordinates;
    if (!Array.isArray(coords) || coords.length < 2) { unmatched++; continue; }
    const [lon, lat] = coords;

    // Scan the 3x3 neighborhood of the station's cell. Cells are ~1.1 km
    // so a 300 m query is fully covered by a single ring.
    const cx = Math.floor(lon / CELL_SIZE_DEG);
    const cy = Math.floor(lat / CELL_SIZE_DEG);

    // Collect every distinct color whose nearest segment is within radius.
    // Also track the overall nearest so single-color stations keep their
    // dominant line as primary.
    let bestDsq = Infinity;
    let bestColor = null;
    const colorBestSq = new Map(); // color -> nearest squared distance (m^2)
    for (let dx = -1; dx <= 1; dx++) {
      for (let dy = -1; dy <= 1; dy++) {
        const bucket = index.get(`${cx + dx}:${cy + dy}`);
        if (!bucket) continue;
        for (const seg of bucket) {
          const dsq = segmentDistSqM(lon, lat, seg.ax, seg.ay, seg.bx, seg.by);
          if (dsq < bestDsq) {
            bestDsq = dsq;
            bestColor = seg.color;
          }
          if (dsq <= radiusSq) {
            const prev = colorBestSq.get(seg.color);
            if (prev === undefined || dsq < prev) {
              colorBestSq.set(seg.color, dsq);
            }
          }
        }
      }
    }

    if (bestColor && bestDsq <= radiusSq) {
      const uid = st.properties?.station_uid;
      if (uid) {
        // Sort distinct colors by ascending distance so the closest/primary
        // line is first. Cap at 5 concentric rings to keep rendering sane.
        const sorted = [...colorBestSq.entries()]
          .sort((a, b) => a[1] - b[1])
          .slice(0, 5)
          .map(([c]) => c);
        updates.push({
          station_uid: uid,
          color: bestColor,
          line_colors: sorted,
        });
        matched++;
        continue;
      }
    }
    unmatched++;
  }

  const changed = updates.length ? updateStationColorsTx(updates) : 0;

  return {
    mode,
    stations: stations.length,
    linesWithColor: colored.length,
    segments: segCount,
    matched,
    unmatched,
    changed,
  };
}

/**
 * For a single (lon, lat) position, scan the segment index for `mode` and
 * return, for every distinct line_color within `radiusM`, the closest (lon,
 * lat) on that line. Used by the station clusterer to place one Apple-style
 * dot on EACH line at a given station centroid.
 *
 * Rebuilds the segment index on every call — cheap enough for the runner's
 * once-per-hour cluster pass; callers that need per-cluster iteration
 * should use `buildSnapIndexForMode` + `snapLinesAt` below instead.
 */
export function buildSnapIndexForMode(mode, opts = {}) {
  const radiusM = opts.radiusM || DEFAULT_RADIUS_M;
  const lines = getLinesByMode(mode);
  const colored = lines.filter((l) => l.properties?.line_color);
  const { index } = buildSegmentIndex(colored);
  return { index, radiusSqM: radiusM * radiusM };
}

export function snapLinesAt(index, lon, lat, { radiusSqM }) {
  const cx = Math.floor(lon / CELL_SIZE_DEG);
  const cy = Math.floor(lat / CELL_SIZE_DEG);
  const bestByColor = new Map(); // color -> { distSq, lon, lat }
  for (let dx = -1; dx <= 1; dx++) {
    for (let dy = -1; dy <= 1; dy++) {
      const bucket = index.get(`${cx + dx}:${cy + dy}`);
      if (!bucket) continue;
      for (const seg of bucket) {
        const snap = closestPointOnSegment(lon, lat, seg.ax, seg.ay, seg.bx, seg.by);
        if (snap.distSqM > radiusSqM) continue;
        const prev = bestByColor.get(seg.color);
        if (!prev || snap.distSqM < prev.distSqM) {
          bestByColor.set(seg.color, { distSqM: snap.distSqM, lon: snap.lon, lat: snap.lat });
        }
      }
    }
  }
  return bestByColor;
}

/**
 * Per-direction snap: for each colour within radius, return one dot per
 * physical track direction at the station's perpendicular projection.
 *
 * Strategy:
 *   1. Find the nearest snap across all segments of this colour → rail A.
 *   2. Find the next-nearest snap whose point is at least MIN_SEPARATION_M
 *      away from rail A's snap, measured in screen space — that's rail B.
 *   3. Emit 1 dot (single-track) or 2 (parallel double-track).
 *
 * The second-rail search ignores along-track displacement entirely. What
 * matters is: two snap points on different parallel rails are separated
 * by the rail gauge (~5-15 m apart in projected space) at the same
 * station-perpendicular line. A second-best snap within MIN_SEPARATION_M
 * of rail A is just another fragment of the SAME rail — dropped. The
 * first snap more than MIN_SEPARATION_M away is the parallel rail.
 *
 * Returns Array<{ lon, lat, color, distSqM }>.
 */
const MIN_RAIL_SEPARATION_M = 3;    // rails closer than this = same track
const MIN_RAIL_SEPARATION_SQ = MIN_RAIL_SEPARATION_M * MIN_RAIL_SEPARATION_M;

export function snapAllWaysAt(index, lon, lat, { radiusSqM }) {
  const cx = Math.floor(lon / CELL_SIZE_DEG);
  const cy = Math.floor(lat / CELL_SIZE_DEG);

  // Collect every snap within radius, grouped by colour.
  const byColor = new Map();
  for (let dx = -1; dx <= 1; dx++) {
    for (let dy = -1; dy <= 1; dy++) {
      const bucket = index.get(`${cx + dx}:${cy + dy}`);
      if (!bucket) continue;
      for (const seg of bucket) {
        const snap = closestPointOnSegment(lon, lat, seg.ax, seg.ay, seg.bx, seg.by);
        if (snap.distSqM > radiusSqM) continue;
        let arr = byColor.get(seg.color);
        if (!arr) { arr = []; byColor.set(seg.color, arr); }
        arr.push({ lon: snap.lon, lat: snap.lat, distSqM: snap.distSqM });
      }
    }
  }

  // Cosine for local metric conversion.
  const cosLat = Math.cos((lat * Math.PI) / 180);
  const out = [];
  for (const [color, points] of byColor) {
    // Sort by distance-to-station ascending.
    points.sort((a, b) => a.distSqM - b.distSqM);
    // Rail A = closest.
    const railA = points[0];
    out.push({ lon: railA.lon, lat: railA.lat, distSqM: railA.distSqM, color });
    // Rail B = first snap at least MIN_RAIL_SEPARATION_M away from rail A.
    // Any closer is either a neighbouring fragment of rail A or a snap
    // onto the same rail from a shorter OSM way.
    for (let k = 1; k < points.length; k++) {
      const p = points[k];
      const dx = (p.lon - railA.lon) * cosLat * METERS_PER_DEG_LAT;
      const dy = (p.lat - railA.lat) * METERS_PER_DEG_LAT;
      const sqSep = dx * dx + dy * dy;
      if (sqSep < MIN_RAIL_SEPARATION_SQ) continue;
      out.push({ lon: p.lon, lat: p.lat, distSqM: p.distSqM, color });
      break;
    }
  }
  return out;
}
