/**
 * Persistent deduplicated camera store.
 *
 * Keyed by `camera_uid` (from cameraDiscovery.cameraUid). Every camera ever
 * surfaced by any discovery channel is upserted into the `cameras` table;
 * repeat sightings union their discovery_channels, bump seen_count, and
 * refresh last_seen_at while leaving first_seen_at intact.
 *
 * Returns GeoJSON-shaped objects so the rest of the app (map layer, WS
 * broadcasts) can consume them without translation.
 */

import db from './database.js';

const stmtGet = db.prepare('SELECT * FROM cameras WHERE camera_uid = ?');

const stmtInsert = db.prepare(`
  INSERT INTO cameras (
    camera_uid, name, camera_type, lat, lon, url, thumbnail_url,
    operator, country, discovery_channels, properties
  ) VALUES (
    @camera_uid, @name, @camera_type, @lat, @lon, @url, @thumbnail_url,
    @operator, @country, @discovery_channels, @properties
  )
`);

const stmtUpdate = db.prepare(`
  UPDATE cameras SET
    name               = @name,
    camera_type        = @camera_type,
    url                = @url,
    thumbnail_url      = @thumbnail_url,
    operator           = @operator,
    discovery_channels = @discovery_channels,
    properties         = @properties,
    last_seen_at       = datetime('now'),
    seen_count         = seen_count + 1
  WHERE camera_uid = @camera_uid
`);

const stmtAll = db.prepare('SELECT * FROM cameras');
const stmtRecent = db.prepare('SELECT * FROM cameras ORDER BY last_seen_at DESC LIMIT ?');
const stmtCountByType = db.prepare(
  "SELECT camera_type, COUNT(*) c FROM cameras GROUP BY camera_type",
);
const stmtTotal = db.prepare('SELECT COUNT(*) c FROM cameras');
const stmtNew24h = db.prepare(
  "SELECT COUNT(*) c FROM cameras WHERE first_seen_at >= datetime('now', '-1 day')",
);

function rowToFeature(row) {
  let properties = {};
  try { properties = JSON.parse(row.properties); } catch {}
  let channels = [];
  try { channels = JSON.parse(row.discovery_channels); } catch {}
  return {
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [row.lon, row.lat] },
    properties: {
      ...properties,
      camera_uid: row.camera_uid,
      name: row.name,
      camera_type: row.camera_type,
      url: row.url,
      thumbnail_url: row.thumbnail_url,
      operator: row.operator,
      country: row.country,
      discovery_channels: channels,
      first_seen_at: row.first_seen_at,
      last_seen_at: row.last_seen_at,
      seen_count: row.seen_count,
    },
  };
}

/**
 * Upsert a single GeoJSON feature produced by a camera discovery channel.
 * @param {object} feature  - GeoJSON Point feature (as built by makeFeature)
 * @param {string} channel  - discovery_channel name
 * @returns {{kind: 'new'|'updated', camera: object}}
 */
export function upsertCamera(feature, channel) {
  const p = feature?.properties || {};
  const coords = feature?.geometry?.coordinates;
  if (!p.camera_uid || !Array.isArray(coords) || coords.length < 2) return null;
  const [lon, lat] = coords;
  if (!Number.isFinite(lat) || !Number.isFinite(lon)) return null;

  const existing = stmtGet.get(p.camera_uid);

  if (!existing) {
    const channels = Array.from(new Set([channel, ...(p.discovery_channels || [])].filter(Boolean)));
    stmtInsert.run({
      camera_uid: p.camera_uid,
      name: p.name || 'Unknown camera',
      camera_type: p.camera_type || 'unknown',
      lat,
      lon,
      url: p.url || null,
      thumbnail_url: p.thumbnail_url || null,
      operator: p.operator || null,
      country: p.country || 'JP',
      discovery_channels: JSON.stringify(channels),
      properties: JSON.stringify(p),
    });
    return { kind: 'new', camera: rowToFeature(stmtGet.get(p.camera_uid)) };
  }

  // Merge: union channels, fill missing fields, prefer existing non-null.
  const prevChannels = (() => { try { return JSON.parse(existing.discovery_channels); } catch { return []; } })();
  const nextChannels = Array.from(new Set([...prevChannels, channel, ...(p.discovery_channels || [])].filter(Boolean)));
  const prevProps = (() => { try { return JSON.parse(existing.properties); } catch { return {}; } })();
  const mergedProps = { ...p, ...prevProps }; // existing wins for conflicts (stable identity)

  stmtUpdate.run({
    camera_uid: p.camera_uid,
    name: existing.name || p.name || 'Unknown camera',
    camera_type: existing.camera_type || p.camera_type || 'unknown',
    url: existing.url || p.url || null,
    thumbnail_url: existing.thumbnail_url || p.thumbnail_url || null,
    operator: existing.operator || p.operator || null,
    discovery_channels: JSON.stringify(nextChannels),
    properties: JSON.stringify(mergedProps),
  });
  return { kind: 'updated', camera: rowToFeature(stmtGet.get(p.camera_uid)) };
}

/** Bulk upsert wrapped in a transaction for cheap per-run I/O. */
export const upsertCamerasTx = db.transaction((features, channel) => {
  const results = [];
  for (const f of features) {
    const r = upsertCamera(f, channel);
    if (r) results.push(r);
  }
  return results;
});

export function getAllCameras() {
  const rows = stmtAll.all();
  return {
    type: 'FeatureCollection',
    features: rows.map(rowToFeature),
    _meta: {
      source: 'camera_store',
      recordCount: rows.length,
      fetchedAt: new Date().toISOString(),
    },
  };
}

export function getRecentCameras(limit = 200) {
  return stmtRecent.all(limit).map(rowToFeature);
}

export function cameraStats() {
  const total = stmtTotal.get().c;
  const new24h = stmtNew24h.get().c;
  const byType = stmtCountByType.all();
  return { total, new24h, byType };
}

export default { upsertCamera, upsertCamerasTx, getAllCameras, getRecentCameras, cameraStats };
