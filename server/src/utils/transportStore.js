/**
 * Persistent deduplicated transport store.
 *
 * Mirrors `cameraStore.js`: every station + every track segment ever surfaced
 * by any upstream feed is upserted into one of two tables, indexed by mode
 * (train | subway | bus | ship | port). Repeat sightings union their `sources`
 * array, bump seen_count, and refresh last_seen_at while leaving first_seen_at
 * intact.
 *
 * Returns GeoJSON FeatureCollections (Points + LineStrings interleaved) so the
 * map layer reads them directly with no further translation.
 */

import crypto from 'node:crypto';
import db from './database.js';
import { upsertItemSync } from './intelStore.js';

// ── Stations ───────────────────────────────────────────────────────────────

// ── UID derivation ─────────────────────────────────────────────────────────

function lat4(v) { return (Math.round((v ?? 0) * 10000) / 10000).toFixed(4); }

function stationUid({ mode, lat, lon, name }) {
  const tail = (name || '').toString().toLowerCase().slice(0, 40);
  return `${mode}:${lat4(lat)}:${lat4(lon)}:${tail}`;
}

function lineUid({ mode, name, operator, coordinates }) {
  const head = (operator || '').toString().toLowerCase().slice(0, 24);
  const nm = (name || '').toString().toLowerCase().slice(0, 40);
  const hash = crypto
    .createHash('md5')
    .update(JSON.stringify(coordinates || []))
    .digest('hex')
    .slice(0, 12);
  return `${mode}:${head}:${nm}:${hash}`;
}

// ── Row → Feature ──────────────────────────────────────────────────────────

function safeJson(text, fallback) {
  try { return JSON.parse(text); } catch { return fallback; }
}

/**
 * Chaikin corner-cutting. One iteration replaces every interior vertex
 * with two new points at 1/4 and 3/4 along its adjacent segments; the
 * first and last coordinate are preserved. Iterated, the sequence
 * converges to a quadratic B-spline — smooth arcs that round *inside*
 * each corner and never overshoot the control polygon.
 *
 * With K iterations, an N-point input balloons to ~2^K × (N-1). On the
 * 134k-fragment train dataset that made a multi-hundred-MB response; we
 * compose with `simplify` below to throw away the collinear points
 * Chaikin emits on long straights while keeping the actual arc geometry.
 *
 * Used so the rendered track AND the live-vehicle simulator ride the
 * same smoothed curve — the train can't drift off a track it's
 * traversing the coords of.
 */
function chaikinOnce(coords) {
  if (!Array.isArray(coords) || coords.length < 3) return coords;
  const out = [coords[0]];
  for (let i = 0; i < coords.length - 1; i++) {
    const [ax, ay] = coords[i];
    const [bx, by] = coords[i + 1];
    out.push([ax + 0.25 * (bx - ax), ay + 0.25 * (by - ay)]);
    out.push([ax + 0.75 * (bx - ax), ay + 0.75 * (by - ay)]);
  }
  out.push(coords[coords.length - 1]);
  return out;
}

// Perpendicular distance from point p to segment (a, b), in planar lon/lat
// degrees. Flat approximation is fine here — we only use this as a
// "is this point within epsilon of the chord" test for simplification.
function perpDistDeg(p, a, b) {
  const dx = b[0] - a[0];
  const dy = b[1] - a[1];
  const len2 = dx * dx + dy * dy;
  if (len2 === 0) {
    const ex = p[0] - a[0];
    const ey = p[1] - a[1];
    return Math.sqrt(ex * ex + ey * ey);
  }
  const t = Math.max(0, Math.min(1, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / len2));
  const cx = a[0] + t * dx;
  const cy = a[1] + t * dy;
  const ex = p[0] - cx;
  const ey = p[1] - cy;
  return Math.sqrt(ex * ex + ey * ey);
}

// Ramer-Douglas-Peucker polyline simplification. Iterative (explicit stack)
// so deep recursion on a 10k+-point polyline can't overflow. Preserves the
// first and last coordinate, drops every interior point whose perpendicular
// distance to the surviving chord is below `epsilonDeg`.
//
// epsilonDeg = 1e-6 ≈ 10 cm at Japan latitudes. Tight enough to preserve
// the Chaikin arc geometry (a 1 m epsilon was aggressive enough to erase
// ~90% of the smoothing, making tracks read as raw OSM again) while still
// collapsing near-collinear runs on long straights.
function simplifyPolyline(coords, epsilonDeg) {
  const n = coords.length;
  if (n < 3) return coords;
  const keep = new Uint8Array(n);
  keep[0] = 1;
  keep[n - 1] = 1;
  const stack = [[0, n - 1]];
  while (stack.length > 0) {
    const [lo, hi] = stack.pop();
    let maxD = 0;
    let idx = -1;
    for (let i = lo + 1; i < hi; i++) {
      const d = perpDistDeg(coords[i], coords[lo], coords[hi]);
      if (d > maxD) { maxD = d; idx = i; }
    }
    if (maxD > epsilonDeg && idx !== -1) {
      keep[idx] = 1;
      stack.push([lo, idx]);
      stack.push([idx, hi]);
    }
  }
  const out = [];
  for (let i = 0; i < n; i++) if (keep[i]) out.push(coords[i]);
  return out;
}

function chaikinSmooth(coords, iterations = 4, simplifyEps = 1e-6) {
  if (!Array.isArray(coords) || coords.length < 3) return coords;
  let c = coords;
  for (let i = 0; i < iterations; i++) c = chaikinOnce(c);
  return simplifyPolyline(c, simplifyEps);
}

// rowToStationFeature / rowToLineFeature were the typed-table read paths.
// Replaced by masterStationRow / masterLineRow below, which read from
// intel_items. The legacy helpers were deleted alongside the typed tables.

// ── Upserts ────────────────────────────────────────────────────────────────

// Read by intel_items uid (master form). Used by the new master-only upserts
// to read previous properties for the merge step.
const stmtMasterGet = db.prepare('SELECT properties, geom_source FROM intel_items WHERE uid = ?');

/**
 * Upsert one Point station feature into intel_items. Master-only — the
 * typed transport_stations table is gone. Preserves source-union + new-
 * payload-wins merge semantics by reading the existing master row's
 * properties JSON.
 *
 * @returns {{kind: 'new'|'updated', feature: GeoJSON feature}}
 */
export function upsertStation(feature, mode, source) {
  const p = feature?.properties || {};
  const coords = feature?.geometry?.coordinates;
  if (!Array.isArray(coords) || coords.length < 2) return null;
  const [lon, lat] = coords;
  if (!Number.isFinite(lat) || !Number.isFinite(lon)) return null;

  const sourceId = MODE_TO_SOURCE_ID[mode];
  if (!sourceId) return null;

  const name = p.name || p.station_name || null;
  const stationUidVal = stationUid({ mode, lat, lon, name });
  const masterUid = `${sourceId}|${stationUidVal}`;

  const existingRow = stmtMasterGet.get(masterUid);
  const prevProps = existingRow ? safeJson(existingRow.properties, {}) : {};
  const prevSources = Array.isArray(prevProps.sources)
    ? prevProps.sources
    : safeJson(prevProps.sources, []);
  const nextSources = Array.from(new Set([
    ...prevSources, source, ...(p.sources || []),
  ].filter(Boolean)));

  const nowIso = new Date().toISOString();
  // Fresh collector output wins for computed fields (line_color etc.) so
  // upstream algorithm fixes propagate. Falls back to previous values when
  // the new payload omits a field.
  const mergedProps = {
    ...prevProps,
    ...p,
    station_uid:   stationUidVal,
    mode,
    name:          name || prevProps.name || null,
    operator:      p.operator || prevProps.operator || null,
    line:          p.line || p.line_name || prevProps.line || null,
    sources:       nextSources,
    first_seen_at: prevProps.first_seen_at || nowIso,
    last_seen_at:  nowIso,
    seen_count:    (prevProps.seen_count ?? 0) + 1,
  };

  const result = upsertItemSync({
    uid:        masterUid,
    title:      mergedProps.name,
    lat,
    lon,
    geomSource: existingRow?.geom_source === 'llm' ? 'llm' : 'native',
    geometry:   { type: 'Point', coordinates: [lon, lat] },
    recordType: 'station',
    subSourceId: mode,
    properties: mergedProps,
  }, sourceId);

  return {
    kind: result?.kind ?? 'new',
    feature: {
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [lon, lat] },
      properties: mergedProps,
    },
  };
}

/**
 * Upsert one LineString track feature into intel_items. Master-only.
 */
export function upsertLine(feature, mode, source) {
  const p = feature?.properties || {};
  const geom = feature?.geometry;
  if (!geom) return null;
  let coordinates = null;
  if (geom.type === 'LineString' && Array.isArray(geom.coordinates) && geom.coordinates.length >= 2) {
    coordinates = geom.coordinates;
  } else if (geom.type === 'MultiLineString' && Array.isArray(geom.coordinates) && geom.coordinates.length > 0) {
    coordinates = geom.coordinates[0];
  }
  if (!coordinates || coordinates.length < 2) return null;

  const sourceId = MODE_TO_SOURCE_ID[mode];
  if (!sourceId) return null;

  const name = p.name || p.line_name || p.route_name || null;
  const operator = p.operator || null;
  const lineUidVal = lineUid({ mode, name, operator, coordinates });
  const masterUid = `${sourceId}|line:${lineUidVal}`;

  const existingRow = stmtMasterGet.get(masterUid);
  const prevProps = existingRow ? safeJson(existingRow.properties, {}) : {};
  const prevSources = Array.isArray(prevProps.sources)
    ? prevProps.sources
    : safeJson(prevProps.sources, []);
  const nextSources = Array.from(new Set([
    ...prevSources, source, ...(p.sources || []),
  ].filter(Boolean)));

  const nowIso = new Date().toISOString();
  const mergedProps = {
    ...prevProps,
    ...p,
    line_uid:      lineUidVal,
    mode,
    name:          name || prevProps.name || null,
    operator:      operator || prevProps.operator || null,
    sources:       nextSources,
    first_seen_at: prevProps.first_seen_at || nowIso,
    last_seen_at:  nowIso,
    seen_count:    (prevProps.seen_count ?? 0) + 1,
    kind:          'track',
  };

  // Indexed centroid: first coord pair. The full LineString is in geometry.
  const [lon, lat] = coordinates[0];
  const result = upsertItemSync({
    uid:        masterUid,
    title:      mergedProps.name,
    lat,
    lon,
    geomSource: existingRow?.geom_source === 'llm' ? 'llm' : 'native',
    geometry:   { type: 'LineString', coordinates },
    recordType: 'track',
    subSourceId: mode,
    properties: mergedProps,
  }, sourceId);

  return {
    kind: result?.kind ?? 'new',
    feature: {
      type: 'Feature',
      geometry: { type: 'LineString', coordinates },
      properties: mergedProps,
    },
  };
}

export const upsertStationsTx = db.transaction((features, mode, source) => {
  const out = [];
  for (const f of features) {
    const r = upsertStation(f, mode, source);
    if (r) out.push(r);
  }
  return out;
});

export const upsertLinesTx = db.transaction((features, mode, source) => {
  const out = [];
  for (const f of features) {
    const r = upsertLine(f, mode, source);
    if (r) out.push(r);
  }
  return out;
});

// ── Color patch (used by transportSpatialSnap) ─────────────────────────────

// Read + UPDATE properties JSON on intel_items by station_uid. Stations are
// keyed master-side as `<sourceId>|<station_uid>` for whichever mode owns
// them. The snap caller passes raw station_uid; we look it up across all
// transport sources via uid LIKE pattern. Cheap enough at the per-station
// granularity (callers batch into a single tx).
const stmtMasterStationGetByUidSuffix = db.prepare(`
  SELECT uid, properties FROM intel_items
   WHERE record_type = 'station'
     AND uid LIKE '%|' || ?
   LIMIT 1
`);
const stmtMasterStationPropsUpdate = db.prepare(`
  UPDATE intel_items SET properties = @properties WHERE uid = @uid
`);

/**
 * Apply a batch of { station_uid, color, line_colors } pairs: overwrite
 * each row's properties.line_color and properties.line_colors. Skips
 * writes when both fields are already equal.
 */
export const updateStationColorsTx = db.transaction((updates) => {
  let changed = 0;
  for (const { station_uid, color, line_colors } of updates) {
    const row = stmtMasterStationGetByUidSuffix.get(station_uid);
    if (!row) continue;
    const props = safeJson(row.properties, {});
    const nextColors = Array.isArray(line_colors) ? line_colors : (color ? [color] : []);
    const prevColors = Array.isArray(props.line_colors) ? props.line_colors : [];
    const colorsEqual = prevColors.length === nextColors.length
      && prevColors.every((v, i) => v === nextColors[i]);
    if (props.line_color === color && colorsEqual) continue;
    props.line_color = color;
    props.line_colors = nextColors;
    stmtMasterStationPropsUpdate.run({
      uid: row.uid,
      properties: JSON.stringify(props),
    });
    changed++;
  }
  return changed;
});

// ── Read API ───────────────────────────────────────────────────────────────

// Mode ↔ source_id mapping. Same as intelBackfill / route layer; lives here
// so the read path can swap typed-table SELECTs for intel_items SELECTs.
const MODE_TO_SOURCE_ID = {
  train:   'unified-trains',
  subway:  'unified-subways',
  bus:     'unified-buses',
  ship:    'unified-ais-ships',
  port:    'unified-port-infra',
  airport: 'unified-airports',
  flight:  'unified-flights',
};

// Stations have lat/lon set and either no stored geometry or a Point geometry.
// (Backfilled rows always have a Point geometry; live-mirrored rows may have
// either a Point geometry or rely on the indexed lat/lon.)
const stmtMasterStationsByMode = db.prepare(`
  SELECT uid, lat, lon, properties, sub_source_id, title,
         fetched_at, geom_at
    FROM intel_items
   WHERE source_id = ?
     AND lat IS NOT NULL
     AND (geometry IS NULL OR json_extract(geometry, '$.type') = 'Point')
`);

// Lines have a LineString geometry stored. Indexed lat/lon hold the centroid
// (set by the mirror / backfill) but coordinates come from the geometry JSON.
const stmtMasterLinesByMode = db.prepare(`
  SELECT uid, geometry, properties, sub_source_id, title,
         fetched_at, geom_at
    FROM intel_items
   WHERE source_id = ?
     AND geometry IS NOT NULL
     AND json_extract(geometry, '$.type') = 'LineString'
`);

const stmtMasterStationCountByMode = db.prepare(
  "SELECT COUNT(*) c FROM intel_items WHERE source_id = ? AND lat IS NOT NULL AND (geometry IS NULL OR json_extract(geometry, '$.type') = 'Point')",
);

const stmtMasterLineCountByMode = db.prepare(
  "SELECT COUNT(*) c FROM intel_items WHERE source_id = ? AND geometry IS NOT NULL AND json_extract(geometry, '$.type') = 'LineString'",
);

const stmtMasterStationNew24hByMode = db.prepare(
  "SELECT COUNT(*) c FROM intel_items WHERE source_id = ? AND lat IS NOT NULL AND fetched_at >= datetime('now', '-1 day')",
);

// Strip the "<sourceId>|" namespace prefix off a master uid to recover the
// inner key (station_uid, line_uid, etc).
function stripSourcePrefix(masterUid, sourceId) {
  if (!masterUid) return null;
  const prefix = `${sourceId}|`;
  return masterUid.startsWith(prefix) ? masterUid.slice(prefix.length) : masterUid;
}

// Strip the optional "line:" sub-prefix used by the line backfill.
function stripLinePrefix(s) {
  if (!s) return null;
  return s.startsWith('line:') ? s.slice('line:'.length) : s;
}

// Build a station feature from one intel_items row. Live-mirrored rows carry
// station_uid/mode/name/operator/sources in `properties` (rowToStationFeature
// spreads those in before mirroring). Backfilled rows store them more
// compactly: station_uid in master uid, mode in sub_source_id, name in
// title. We fall through, properties first then derived.
function masterStationRow(row, mode, sourceId) {
  const properties = safeJson(row.properties, {});
  const sources = Array.isArray(properties.sources)
    ? properties.sources
    : safeJson(properties.sources, []);
  return {
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [row.lon, row.lat] },
    properties: {
      ...properties,
      station_uid:   properties.station_uid ?? stripSourcePrefix(row.uid, sourceId),
      mode:          properties.mode ?? row.sub_source_id ?? mode,
      name:          properties.name ?? row.title ?? null,
      operator:      properties.operator ?? null,
      line:          properties.line ?? null,
      sources,
      first_seen_at: properties.first_seen_at ?? row.fetched_at ?? null,
      last_seen_at:  properties.last_seen_at ?? row.fetched_at ?? null,
      seen_count:    properties.seen_count ?? 1,
    },
  };
}

function masterLineRow(row, mode, sourceId) {
  const properties = safeJson(row.properties, {});
  const sources = Array.isArray(properties.sources)
    ? properties.sources
    : safeJson(properties.sources, []);
  const geom = safeJson(row.geometry, null);
  const coords = Array.isArray(geom?.coordinates) ? geom.coordinates : [];
  return {
    type: 'Feature',
    geometry: { type: 'LineString', coordinates: coords },
    properties: {
      ...properties,
      line_uid:      properties.line_uid ?? stripLinePrefix(stripSourcePrefix(row.uid, sourceId)),
      mode:          properties.mode ?? row.sub_source_id ?? mode,
      name:          properties.name ?? row.title ?? null,
      operator:      properties.operator ?? null,
      sources,
      first_seen_at: properties.first_seen_at ?? row.fetched_at ?? null,
      last_seen_at:  properties.last_seen_at ?? row.fetched_at ?? null,
      seen_count:    properties.seen_count ?? 1,
      kind:          properties.kind ?? 'track',
    },
  };
}

export function getStationsByMode(mode) {
  const sourceId = MODE_TO_SOURCE_ID[mode];
  if (!sourceId) return [];
  return stmtMasterStationsByMode.all(sourceId).map((r) => masterStationRow(r, mode, sourceId));
}

// Quantize a [lon, lat] to ~1 m precision so near-identical OSM endpoints
// group into the same adjacency bucket despite trailing-digit noise.
function endpointKey([x, y]) {
  return `${x.toFixed(5)},${y.toFixed(5)}`;
}

// Reverse a coord array without mutating the caller.
function reverseCoords(c) {
  return c.slice().reverse();
}

/**
 * Stitch connected LineString fragments into long polylines when the shared
 * endpoint has exactly ONE other fragment hanging off it (a "through" point,
 * not a real junction). Then Chaikin-smooth each merged polyline so curves
 * round through the former junctions instead of kinking at them.
 *
 * The collectors emit one Feature per OSM way — typically 2–5 coords per
 * fragment, with ~88k endpoints being through-points and only ~12k being
 * real 3+-way junctions (measured on the live train dataset). Stitching
 * those through-points drops fragment count by ~3× and lets smoothing
 * actually work across each continuous railway.
 *
 * Rules:
 *   - Only stitch when endpoint degree == 2 (exactly two incident fragments).
 *   - Preserve `properties`: adopt the first fragment's, since fragments on
 *     the same continuous railway share line colour/operator/ref in
 *     practice. (Where they don't — e.g. a through-station where one
 *     operator takes over — the endpoint would be tagged with a station,
 *     often bumping degree to 3+ and protecting the join from stitching.)
 *   - Features with MultiLineString geometry pass through untouched.
 *   - Bus mode skips stitching + smoothing entirely (MLIT N07 shapes are
 *     already digitised as long smooth polylines).
 */
function stitchAndSmoothLines(features, mode) {
  if (mode === 'bus') return features;

  const stitchable = [];
  const passthrough = [];
  for (const f of features) {
    if (
      f?.geometry?.type === 'LineString'
      && Array.isArray(f.geometry.coordinates)
      && f.geometry.coordinates.length >= 2
    ) {
      stitchable.push(f);
    } else {
      passthrough.push(f);
    }
  }
  if (stitchable.length === 0) return passthrough;

  // Build endpoint adjacency. Each fragment exposes two endpoint keys; the
  // value is the fragment's index into `stitchable`.
  const degree = new Map();  // key -> count
  const headOf = new Array(stitchable.length);  // idx -> start key
  const tailOf = new Array(stitchable.length);  // idx -> end key
  for (let i = 0; i < stitchable.length; i++) {
    const c = stitchable[i].geometry.coordinates;
    const a = endpointKey(c[0]);
    const b = endpointKey(c[c.length - 1]);
    headOf[i] = a;
    tailOf[i] = b;
    degree.set(a, (degree.get(a) || 0) + 1);
    degree.set(b, (degree.get(b) || 0) + 1);
  }

  // For each key, collect the list of fragment indices that touch it.
  const incident = new Map();  // key -> idx[]
  for (let i = 0; i < stitchable.length; i++) {
    for (const k of [headOf[i], tailOf[i]]) {
      let list = incident.get(k);
      if (!list) { list = []; incident.set(k, list); }
      list.push(i);
    }
  }

  // Walk: starting from each unvisited fragment, extend forward from its
  // tail and backward from its head along any endpoint of degree 2.
  const visited = new Array(stitchable.length).fill(false);
  const out = [];

  function otherIncident(key, excludeIdx) {
    const list = incident.get(key) || [];
    for (const i of list) if (i !== excludeIdx) return i;
    return -1;
  }

  for (let start = 0; start < stitchable.length; start++) {
    if (visited[start]) continue;
    visited[start] = true;

    let coords = stitchable[start].geometry.coordinates.slice();
    const baseProps = stitchable[start].properties || {};

    // Extend forward (tail side).
    let tailKey = tailOf[start];
    let tailFromIdx = start;
    while (degree.get(tailKey) === 2) {
      const next = otherIncident(tailKey, tailFromIdx);
      if (next === -1 || visited[next]) break;
      visited[next] = true;
      // Orient next fragment so its head aligns with our tail.
      const nc = stitchable[next].geometry.coordinates;
      const nextHead = headOf[next];
      const appendCoords = (nextHead === tailKey) ? nc : reverseCoords(nc);
      // Drop the duplicate shared endpoint.
      for (let j = 1; j < appendCoords.length; j++) coords.push(appendCoords[j]);
      tailKey = (nextHead === tailKey) ? tailOf[next] : headOf[next];
      tailFromIdx = next;
    }

    // Extend backward (head side).
    let headKey = headOf[start];
    let headFromIdx = start;
    while (degree.get(headKey) === 2) {
      const prev = otherIncident(headKey, headFromIdx);
      if (prev === -1 || visited[prev]) break;
      visited[prev] = true;
      const pc = stitchable[prev].geometry.coordinates;
      const prevTail = tailOf[prev];
      const prependCoords = (prevTail === headKey) ? pc : reverseCoords(pc);
      // Drop the duplicate shared endpoint (which is prependCoords' last).
      const without = prependCoords.slice(0, -1);
      coords = without.concat(coords);
      headKey = (prevTail === headKey) ? headOf[prev] : tailOf[prev];
      headFromIdx = prev;
    }

    const smoothed = chaikinSmooth(coords, 4);
    out.push({
      type: 'Feature',
      geometry: { type: 'LineString', coordinates: smoothed },
      properties: baseProps,
    });
  }

  return [...out, ...passthrough];
}

export function getLinesByMode(mode) {
  const sourceId = MODE_TO_SOURCE_ID[mode];
  if (!sourceId) return [];
  const raw = stmtMasterLinesByMode.all(sourceId).map((r) => masterLineRow(r, mode, sourceId));
  return stitchAndSmoothLines(raw, mode);
}

/**
 * One FeatureCollection per layer endpoint. Stations + lines interleaved so
 * the client can split them with a `geometry-type` filter.
 */
export function getTransportFeatureCollection(mode) {
  const stations = getStationsByMode(mode);
  const lines = getLinesByMode(mode);
  return {
    type: 'FeatureCollection',
    features: [...lines, ...stations],
    _meta: {
      source: `transport_store:${mode}`,
      stationCount: stations.length,
      lineCount: lines.length,
      recordCount: stations.length + lines.length,
      fetchedAt: new Date().toISOString(),
    },
  };
}

export function transportStats(mode) {
  const sourceId = MODE_TO_SOURCE_ID[mode];
  if (!sourceId) return { stations: 0, lines: 0, new24h: 0 };
  return {
    stations: stmtMasterStationCountByMode.get(sourceId).c,
    lines:    stmtMasterLineCountByMode.get(sourceId).c,
    new24h:   stmtMasterStationNew24hByMode.get(sourceId).c,
  };
}

export default {
  upsertStation,
  upsertLine,
  upsertStationsTx,
  upsertLinesTx,
  updateStationColorsTx,
  getStationsByMode,
  getLinesByMode,
  getTransportFeatureCollection,
  transportStats,
};
