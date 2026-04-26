/**
 * Cross-mode station clusterer.
 *
 * Produces one canonical `station_clusters` row per physical place by
 * merging every `transport_stations` row (across train / subway / bus)
 * that refers to the same interchange. Runs after `snapStationsToNearestLine`
 * so each member already has its per-mode `line_colors[]` populated.
 *
 * Two-pass algorithm:
 *   1. Exact-identity merge by wikidata Q-id. Japan's rail stations are
 *      heavily tagged with a single wikidata id per physical station, so
 *      the three JR-Shinjuku rows and the two Tokyo-Metro-Shinjuku rows
 *      collapse into one group.
 *   2. Spatial + name merge (union-find) for remaining singletons:
 *      - haversine distance <= SPATIAL_RADIUS_M
 *      - AND station_name_fingerprint equal, OR Levenshtein <= 2 on the
 *        folded hiragana form (stationNameFingerprint in _dedupe.js).
 *
 * Bus mode is intentionally skipped — bus stop names are noisy enough
 * that 150 m + name-match produces too many false merges. Bus stops keep
 * their own per-mode layer.
 */

import crypto from 'node:crypto';
import db from './database.js';
import { stationNameFingerprint, levenshtein } from '../collectors/_dedupe.js';
import { buildSnapIndexForMode, snapAllWaysAt } from './transportSpatialSnap.js';

const SPATIAL_RADIUS_M = 150;
const LEVENSHTEIN_THRESHOLD = 2;
const CELL_SIZE_DEG = 0.005; // ~550 m at Japan latitude; 3x3 covers ~1.6 km
const METERS_PER_DEG_LAT = 111_320;

// Haversine-lite (equirectangular) metres. Fine at 150 m scale.
function distSqM(ax, ay, bx, by) {
  const cosLat = Math.cos(((ay + by) * 0.5 * Math.PI) / 180);
  const dx = (bx - ax) * cosLat * METERS_PER_DEG_LAT;
  const dy = (by - ay) * METERS_PER_DEG_LAT;
  return dx * dx + dy * dy;
}

function cellKey(lon, lat) {
  const cx = Math.floor(lon / CELL_SIZE_DEG);
  const cy = Math.floor(lat / CELL_SIZE_DEG);
  return `${cx}:${cy}`;
}

// --- Union-find (disjoint set) ---
function makeDSU(n) {
  const p = new Int32Array(n);
  const r = new Int8Array(n);
  for (let i = 0; i < n; i++) p[i] = i;
  function find(x) {
    while (p[x] !== x) {
      p[x] = p[p[x]]; // path compression
      x = p[x];
    }
    return x;
  }
  function union(a, b) {
    const ra = find(a);
    const rb = find(b);
    if (ra === rb) return;
    if (r[ra] < r[rb]) p[ra] = rb;
    else if (r[ra] > r[rb]) p[rb] = ra;
    else { p[rb] = ra; r[ra]++; }
  }
  return { find, union };
}

// --- Read side ---
const stmtStations = db.prepare(`
  SELECT station_uid, mode, name, operator, lat, lon, properties
  FROM transport_stations
  WHERE mode IN ('train', 'subway')
`);

function loadStations() {
  return stmtStations.all().map((r) => {
    let p = {};
    try { p = JSON.parse(r.properties); } catch { /* leave empty */ }
    return {
      uid: r.station_uid,
      mode: r.mode,
      name: r.name,
      name_ja: p.name_ja || null,
      operator: r.operator,
      lat: r.lat,
      lon: r.lon,
      wikidata: p.wikidata || null,
      line_color: p.line_color || null,
      line_colors: Array.isArray(p.line_colors) ? p.line_colors : [],
      // NOTE: transportSpatialSnap only writes line_color/line_colors today —
      // line names/refs aren't propagated to the station row. We carry the
      // station's own declared line/ref as a fallback for the cluster's
      // line_names[] array.
      line_name: p.line || p.line_name || null,
      line_ref: p.line_ref || null,
      fingerprint: stationNameFingerprint(r.name || p.name_ja || ''),
    };
  });
}

// --- Clustering ---
function cluster(stations) {
  const dsu = makeDSU(stations.length);

  // Pass 1: wikidata identity merge.
  const byWikidata = new Map();
  for (let i = 0; i < stations.length; i++) {
    const q = stations[i].wikidata;
    if (!q) continue;
    if (byWikidata.has(q)) dsu.union(byWikidata.get(q), i);
    else byWikidata.set(q, i);
  }

  // Pass 2: spatial + name merge. Bucket stations by coarse cell so we
  // only pairwise-compare within a 3x3 neighbourhood.
  const cells = new Map();
  for (let i = 0; i < stations.length; i++) {
    const k = cellKey(stations[i].lon, stations[i].lat);
    let bucket = cells.get(k);
    if (!bucket) { bucket = []; cells.set(k, bucket); }
    bucket.push(i);
  }

  const radiusSq = SPATIAL_RADIUS_M * SPATIAL_RADIUS_M;
  for (const [key, bucket] of cells) {
    const [cx, cy] = key.split(':').map(Number);
    const candidates = [];
    for (let dx = -1; dx <= 1; dx++) {
      for (let dy = -1; dy <= 1; dy++) {
        const neighbour = cells.get(`${cx + dx}:${cy + dy}`);
        if (neighbour) for (const i of neighbour) candidates.push(i);
      }
    }
    for (const i of bucket) {
      for (const j of candidates) {
        if (j <= i) continue;
        const a = stations[i];
        const b = stations[j];
        if (!a.fingerprint || !b.fingerprint) continue;
        if (distSqM(a.lon, a.lat, b.lon, b.lat) > radiusSq) continue;
        // Accept exact fingerprint match OR Levenshtein <= 2. One-character
        // names (e.g. Chinese-district stations) can't tolerate distance 2,
        // so require exact match for short names.
        if (a.fingerprint === b.fingerprint) {
          dsu.union(i, j);
          continue;
        }
        const len = Math.min(a.fingerprint.length, b.fingerprint.length);
        if (len >= 3 && levenshtein(a.fingerprint, b.fingerprint) <= LEVENSHTEIN_THRESHOLD) {
          dsu.union(i, j);
        }
      }
    }
  }

  // Collect groups.
  const groups = new Map();
  for (let i = 0; i < stations.length; i++) {
    const root = dsu.find(i);
    let g = groups.get(root);
    if (!g) { g = []; groups.set(root, g); }
    g.push(i);
  }
  return [...groups.values()];
}

// --- Cluster materialisation ---
function materialise(group, stations) {
  const members = group.map((i) => stations[i]);

  // Centroid (simple mean — members are within 150 m).
  let latSum = 0, lonSum = 0;
  for (const m of members) { latSum += m.lat; lonSum += m.lon; }
  const lat = latSum / members.length;
  const lon = lonSum / members.length;

  // Pick the longest member name as canonical display name. English forms
  // tend to be longer (e.g. "Tokyo Station" vs "東京"), which matches the
  // Apple-Maps aesthetic on non-CJK basemap labels.
  let name = '';
  let name_ja = null;
  for (const m of members) {
    if (m.name && m.name.length > name.length) name = m.name;
    if (m.name_ja && !name_ja) name_ja = m.name_ja;
  }

  // Line union. Each member contributes its line_colors[] (primary-first
  // via the per-mode spatial snap). We dedupe by color and remember the
  // mode that introduced each color so the client can draw a per-mode
  // glyph next to each chip.
  const seenColors = new Set();
  const line_colors = [];
  const line_modes = [];
  const line_names = [];
  const line_refs = [];
  for (const m of members) {
    for (const c of m.line_colors) {
      if (!c || seenColors.has(c)) continue;
      seenColors.add(c);
      line_colors.push(c);
      line_modes.push(m.mode);
      // Fallbacks: use the member's own line/line_ref if the color came
      // from its primary line; otherwise leave blank (the client can still
      // render the chip as a solid dot without a ref).
      line_names.push(m.line_color === c ? (m.line_name || null) : null);
      line_refs.push(m.line_color === c ? (m.line_ref || null) : null);
    }
  }

  const mode_set = Array.from(new Set(members.map((m) => m.mode))).sort();
  const operator_set = Array.from(new Set(members.map((m) => m.operator).filter(Boolean))).sort();

  // Stable cluster_uid from sorted member UIDs (sha1). A member joining
  // or leaving changes the uid — that's fine, the clusterer rewrites the
  // whole table each run.
  const member_uids = members.map((m) => m.uid).sort();
  const cluster_uid = crypto
    .createHash('sha1')
    .update(member_uids.join('|'))
    .digest('hex');

  return {
    cluster_uid,
    name: name || '(unknown)',
    name_ja,
    lat,
    lon,
    member_uids,
    line_colors,
    line_names,
    line_refs,
    line_modes,
    mode_set,
    operator_set,
  };
}

// --- Write side ---
const stmtClearClusters = db.prepare('DELETE FROM station_clusters');
const stmtInsertCluster = db.prepare(`
  INSERT INTO station_clusters (
    cluster_uid, name, name_ja, lat, lon,
    member_uids, line_colors, line_names, line_refs, line_modes,
    mode_set, operator_set
  ) VALUES (
    @cluster_uid, @name, @name_ja, @lat, @lon,
    @member_uids, @line_colors, @line_names, @line_refs, @line_modes,
    @mode_set, @operator_set
  )
`);

const writeClustersTx = db.transaction((rows) => {
  stmtClearClusters.run();
  for (const r of rows) {
    stmtInsertCluster.run({
      cluster_uid: r.cluster_uid,
      name: r.name,
      name_ja: r.name_ja,
      lat: r.lat,
      lon: r.lon,
      member_uids: JSON.stringify(r.member_uids),
      line_colors: JSON.stringify(r.line_colors),
      line_names: JSON.stringify(r.line_names),
      line_refs: JSON.stringify(r.line_refs),
      line_modes: JSON.stringify(r.line_modes),
      mode_set: JSON.stringify(r.mode_set),
      operator_set: JSON.stringify(r.operator_set),
    });
  }
});

// --- Per-line snapped dots ---
// For each cluster, for each of its line_colors, record the closest point
// on that line's track geometry to the cluster centroid. Renders as one
// Apple-style colored dot directly ON each line.
const stmtClearDots = db.prepare('DELETE FROM station_line_dots');
const stmtInsertDot = db.prepare(`
  INSERT OR REPLACE INTO station_line_dots
    (cluster_uid, way_uid, line_color, line_mode, lon, lat)
  VALUES (?, ?, ?, ?, ?, ?)
`);
const writeDotsTx = db.transaction((dots) => {
  stmtClearDots.run();
  for (const d of dots) {
    stmtInsertDot.run(
      d.cluster_uid, d.way_uid, d.line_color, d.line_mode, d.lon, d.lat,
    );
  }
});

function computeLineDots(clusterRows) {
  // Emit one dot per physical track direction per colour that passes
  // within snap radius of the cluster — regardless of whether the
  // cluster's pre-recorded `line_colors[]` already lists that colour.
  // The per-mode spatialSnap ran earlier and stamped each member station
  // with the lines it could match, but stations without a direct OSM
  // ref to a shinkansen / crossing line end up with an incomplete set.
  // By emitting from geometry directly we cover every line the renderer
  // actually draws at this cluster.
  const indexByMode = {};
  for (const mode of ['train', 'subway']) {
    try {
      indexByMode[mode] = buildSnapIndexForMode(mode);
    } catch (err) {
      console.warn(`[stationClusterer] snap index for ${mode} failed:`, err?.message);
    }
  }

  const dots = [];
  for (const c of clusterRows) {
    for (const mode of ['train', 'subway']) {
      const idx = indexByMode[mode];
      if (!idx) continue;
      // snapAllWaysAt returns at most 2 snap points per distinct colour
      // within radius (rail A + rail B if parallel). Emit every entry.
      const snaps = snapAllWaysAt(idx.index, c.lon, c.lat, { radiusSqM: idx.radiusSqM });
      const countByColor = new Map(); // color -> running index for way_uid
      for (const s of snaps) {
        const k = countByColor.get(s.color) || 0;
        countByColor.set(s.color, k + 1);
        dots.push({
          cluster_uid: c.cluster_uid,
          way_uid: `${mode}:${s.color}:${k}`,
          line_color: s.color,
          line_mode: mode,
          lon: s.lon,
          lat: s.lat,
        });
      }
    }
  }
  return dots;
}

export function runStationClusterer() {
  const stations = loadStations();
  if (stations.length === 0) return { stations: 0, clusters: 0, dots: 0 };
  const groups = cluster(stations);
  const rows = groups.map((g) => materialise(g, stations));
  writeClustersTx(rows);

  // Per-line snapped dots — one per (cluster, line_color) pair, snapped
  // onto the nearest segment of the line's track geometry.
  let dotCount = 0;
  try {
    const dots = computeLineDots(rows);
    writeDotsTx(dots);
    dotCount = dots.length;
  } catch (err) {
    console.warn('[stationClusterer] line dots failed:', err?.message);
  }

  return {
    stations: stations.length,
    clusters: rows.length,
    merges: stations.length - rows.length,
    dots: dotCount,
  };
}

// --- Read API (used by the read-collector for /api/data/unified-stations) ---

const stmtAllClusters = db.prepare(`
  SELECT cluster_uid, name, name_ja, lat, lon,
         line_colors, line_names, line_refs, line_modes,
         mode_set, operator_set, member_uids
  FROM station_clusters
`);

function safeJson(text, fallback) {
  try { return JSON.parse(text); } catch { return fallback; }
}

export function getAllClusterFeatures() {
  return stmtAllClusters.all().map((row) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [row.lon, row.lat] },
    properties: {
      cluster_uid: row.cluster_uid,
      station_uid: row.cluster_uid, // alias so StationPopup-style code still works
      name: row.name,
      name_ja: row.name_ja,
      line_colors: safeJson(row.line_colors, []),
      line_names: safeJson(row.line_names, []),
      line_refs: safeJson(row.line_refs, []),
      line_modes: safeJson(row.line_modes, []),
      mode_set: safeJson(row.mode_set, []),
      operator_set: safeJson(row.operator_set, []),
      member_uids: safeJson(row.member_uids, []),
    },
  }));
}

export function getClusterByUid(uid) {
  return db.prepare(`
    SELECT cluster_uid, name, name_ja, lat, lon,
           line_colors, line_names, line_refs, line_modes,
           mode_set, operator_set, member_uids
    FROM station_clusters WHERE cluster_uid = ?
  `).get(uid);
}

const stmtAllDots = db.prepare(`
  SELECT d.cluster_uid, d.way_uid, d.line_color, d.line_mode, d.lon, d.lat,
         c.name, c.name_ja
  FROM station_line_dots d
  LEFT JOIN station_clusters c ON c.cluster_uid = d.cluster_uid
`);

/**
 * One GeoJSON Point per (cluster, way) pair — snapped onto that track's
 * geometry. Two-track lines produce two dots per cluster, one on each
 * rail. Apple-Maps style.
 */
export function getAllLineDotFeatures() {
  return stmtAllDots.all().map((row) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [row.lon, row.lat] },
    properties: {
      cluster_uid: row.cluster_uid,
      station_uid: row.cluster_uid, // alias for StationPopup routing
      way_uid: row.way_uid,
      name: row.name,
      name_ja: row.name_ja,
      line_color: row.line_color,
      line_mode: row.line_mode,
    },
  }));
}

const MAX_UNCERTAIN_PAIRS = 500;

/**
 * Return up to MAX_UNCERTAIN_PAIRS station pairs that the existing
 * Pass-2 (fingerprint + Levenshtein ≤ 2) clusterer would NOT merge,
 * but which are within 150 m of each other and worth asking an LLM
 * about. Used by llmEnricher.
 *
 * Pair ordering: uid_a < uid_b lexically, so each pair appears once.
 */
export function findUncertainStationPairs() {
  const stations = loadStations();
  if (stations.length === 0) return [];

  const cells = new Map();
  for (let i = 0; i < stations.length; i++) {
    const k = cellKey(stations[i].lon, stations[i].lat);
    let bucket = cells.get(k);
    if (!bucket) { bucket = []; cells.set(k, bucket); }
    bucket.push(i);
  }

  const radiusSq = SPATIAL_RADIUS_M * SPATIAL_RADIUS_M;
  const out = [];
  for (const [key, bucket] of cells) {
    const [cx, cy] = key.split(':').map(Number);
    const candidates = [];
    for (let dx = -1; dx <= 1; dx++) {
      for (let dy = -1; dy <= 1; dy++) {
        const neighbour = cells.get(`${cx + dx}:${cy + dy}`);
        if (neighbour) for (const i of neighbour) candidates.push(i);
      }
    }
    for (const i of bucket) {
      for (const j of candidates) {
        if (j <= i) continue;
        const a = stations[i];
        const b = stations[j];
        if (distSqM(a.lon, a.lat, b.lon, b.lat) > radiusSq) continue;
        const sameFp = a.fingerprint && b.fingerprint && a.fingerprint === b.fingerprint;
        const len = Math.min(a.fingerprint?.length || 0, b.fingerprint?.length || 0);
        const closeFp = len >= 3 && a.fingerprint && b.fingerprint
          && levenshtein(a.fingerprint, b.fingerprint) <= LEVENSHTEIN_THRESHOLD;
        const operatorsDiffer = a.operator && b.operator && a.operator !== b.operator;
        // "Interesting" = Pass 2 would NOT merge, but they're spatially close.
        // Either fingerprint+Levenshtein both miss, OR the names match but the
        // operators differ (cross-operator station that wikidata didn't link).
        const wouldPass2Merge = sameFp || closeFp;
        const interesting = !wouldPass2Merge || (sameFp && operatorsDiffer);
        if (!interesting) continue;
        const [pa, pb] = a.uid < b.uid ? [a, b] : [b, a];
        out.push({
          uid_a: pa.uid,  name_a: pa.name,  name_ja_a: pa.name_ja,  operator_a: pa.operator,  line_a: pa.line_name, mode_a: pa.mode, lat_a: pa.lat, lon_a: pa.lon,
          uid_b: pb.uid,  name_b: pb.name,  name_ja_b: pb.name_ja,  operator_b: pb.operator,  line_b: pb.line_name, mode_b: pb.mode, lat_b: pb.lat, lon_b: pb.lon,
          dist_m: Math.sqrt(distSqM(a.lon, a.lat, b.lon, b.lat)),
        });
        if (out.length >= MAX_UNCERTAIN_PAIRS) return out;
      }
    }
  }
  return out;
}
