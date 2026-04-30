/**
 * Persistent store for nationwide OSM station-building polygons.
 *
 * Feeds two consumers:
 *   - `/api/data/unified-station-footprints?bbox=...` — bbox-filtered Polygon
 *     FeatureCollection for the map layer (renders as translucent floor-plan
 *     fill at zoom >= 15).
 *   - The cluster→footprint linker in the transport runner: for each cluster,
 *     find the footprint (if any) whose bbox contains the cluster centroid,
 *     then stamp `cluster_uid` onto that footprint so popups can hyperlink
 *     the polygon back to its canonical station.
 */

import db from './database.js';

function safeJson(text, fallback) {
  try { return JSON.parse(text); } catch { return fallback; }
}

function computeBbox(polygon) {
  // GeoJSON Polygon: coordinates is [[ring1], [hole?], ...]. Outer ring only.
  const ring = Array.isArray(polygon?.coordinates) ? polygon.coordinates[0] : null;
  if (!Array.isArray(ring) || ring.length === 0) return null;
  let minLon = Infinity, minLat = Infinity, maxLon = -Infinity, maxLat = -Infinity;
  for (const [lon, lat] of ring) {
    if (!Number.isFinite(lon) || !Number.isFinite(lat)) continue;
    if (lon < minLon) minLon = lon;
    if (lon > maxLon) maxLon = lon;
    if (lat < minLat) minLat = lat;
    if (lat > maxLat) maxLat = lat;
  }
  if (!Number.isFinite(minLon)) return null;
  return { minLon, minLat, maxLon, maxLat };
}

const stmtGet = db.prepare('SELECT * FROM station_footprints WHERE footprint_id = ?');
const stmtInsert = db.prepare(`
  INSERT INTO station_footprints (
    footprint_id, cluster_uid, name, name_ja, geometry,
    bbox_min_lat, bbox_min_lon, bbox_max_lat, bbox_max_lon, source
  ) VALUES (
    @footprint_id, @cluster_uid, @name, @name_ja, @geometry,
    @bbox_min_lat, @bbox_min_lon, @bbox_max_lat, @bbox_max_lon, @source
  )
`);
const stmtUpdate = db.prepare(`
  UPDATE station_footprints SET
    name           = @name,
    name_ja        = @name_ja,
    geometry       = @geometry,
    bbox_min_lat   = @bbox_min_lat,
    bbox_min_lon   = @bbox_min_lon,
    bbox_max_lat   = @bbox_max_lat,
    bbox_max_lon   = @bbox_max_lon,
    source         = @source,
    last_seen_at   = datetime('now')
  WHERE footprint_id = @footprint_id
`);

export function upsertFootprint(feature) {
  const p = feature?.properties || {};
  const id = p.footprint_id;
  if (!id) return null;
  const bbox = computeBbox(feature.geometry);
  if (!bbox) return null;
  const row = {
    footprint_id: id,
    cluster_uid: null,
    name: p.name || null,
    name_ja: p.name_ja || null,
    geometry: JSON.stringify(feature.geometry),
    bbox_min_lat: bbox.minLat,
    bbox_min_lon: bbox.minLon,
    bbox_max_lat: bbox.maxLat,
    bbox_max_lon: bbox.maxLon,
    source: p.source || null,
  };
  const existing = stmtGet.get(id);
  if (existing) {
    stmtUpdate.run(row);
    return { kind: 'updated', id };
  }
  stmtInsert.run(row);
  return { kind: 'new', id };
}

export const upsertFootprintsTx = db.transaction((features) => {
  const out = [];
  for (const f of features) {
    const r = upsertFootprint(f);
    if (r) out.push(r);
  }
  return out;
});

// --- Cluster linking ---
// Given an array of { cluster_uid, lat, lon }, stamp cluster_uid onto each
// footprint whose bbox contains the centroid. Uses the bbox index so scans
// only check the few candidates in the relevant grid cell.
const stmtClearClusterLinks = db.prepare('UPDATE station_footprints SET cluster_uid = NULL');
const stmtFootprintsContaining = db.prepare(`
  SELECT footprint_id FROM station_footprints
  WHERE bbox_min_lon <= ? AND bbox_max_lon >= ?
    AND bbox_min_lat <= ? AND bbox_max_lat >= ?
`);
const stmtLinkCluster = db.prepare(
  'UPDATE station_footprints SET cluster_uid = ? WHERE footprint_id = ?',
);

export const linkClustersToFootprintsTx = db.transaction((clusters) => {
  stmtClearClusterLinks.run();
  let linked = 0;
  for (const c of clusters) {
    const hits = stmtFootprintsContaining.all(c.lon, c.lon, c.lat, c.lat);
    for (const h of hits) {
      stmtLinkCluster.run(c.cluster_uid, h.footprint_id);
      linked++;
    }
  }
  return linked;
});

// --- Read API ---
// LEFT JOIN on station_clusters so each footprint carries the linked
// cluster's mode_set — the client uses that to filter footprints per
// mode layer (Trains layer only shows footprints for clusters that
// include a train line, and so on).
const stmtByBbox = db.prepare(`
  SELECT f.footprint_id, f.cluster_uid, f.name, f.name_ja, f.geometry,
         c.mode_set
  FROM station_footprints f
  LEFT JOIN station_clusters c ON c.cluster_uid = f.cluster_uid
  WHERE f.bbox_min_lon <= ? AND f.bbox_max_lon >= ?
    AND f.bbox_min_lat <= ? AND f.bbox_max_lat >= ?
`);
const stmtAll = db.prepare(`
  SELECT f.footprint_id, f.cluster_uid, f.name, f.name_ja, f.geometry,
         c.mode_set
  FROM station_footprints f
  LEFT JOIN station_clusters c ON c.cluster_uid = f.cluster_uid
`);

function rowToFeature(row) {
  return {
    type: 'Feature',
    geometry: safeJson(row.geometry, null),
    properties: {
      footprint_id: row.footprint_id,
      cluster_uid: row.cluster_uid,
      name: row.name,
      name_ja: row.name_ja,
      mode_set: safeJson(row.mode_set, []),
    },
  };
}

export function getFootprintsInBbox(minLon, minLat, maxLon, maxLat) {
  return stmtByBbox
    .all(maxLon, minLon, maxLat, minLat)
    .map(rowToFeature)
    .filter((f) => f.geometry);
}

export function getAllFootprints() {
  return stmtAll.all().map(rowToFeature).filter((f) => f.geometry);
}

export function footprintCount() {
  return db.prepare('SELECT COUNT(*) c FROM station_footprints').get().c;
}
