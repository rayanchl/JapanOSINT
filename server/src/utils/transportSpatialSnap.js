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
    for (let i = 0; i < coords.length - 1; i++) {
      const a = coords[i];
      const b = coords[i + 1];
      if (!Array.isArray(a) || !Array.isArray(b)) continue;
      const seg = { ax: a[0], ay: a[1], bx: b[0], by: b[1], color };
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

    let bestDsq = Infinity;
    let bestColor = null;
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
        }
      }
    }

    if (bestColor && bestDsq <= radiusSq) {
      const uid = st.properties?.station_uid;
      if (uid) {
        updates.push({ station_uid: uid, color: bestColor });
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
