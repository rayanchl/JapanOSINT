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

// --------------- One-shot migration: collapse YouTube duplicates ---------------
// Earlier builds stamped camera_uid from the aggregator URL before the YouTube
// upgrade ran, so the same YouTube stream discovered through Skyline,
// webcamera24, worldcams, etc. produced N distinct rows. New inserts now
// canonicalize on yt:<id>, but legacy rows need to be merged. Idempotent: once
// every youtube-id group has a single row, subsequent boots walk away quickly.
(function migrateCollapseYouTubeDuplicates() {
  try {
    const rows = db.prepare(
      "SELECT camera_uid, properties, discovery_channels, first_seen_at, last_seen_at, seen_count FROM cameras"
    ).all();
    const byYtId = new Map();
    for (const r of rows) {
      let p = {};
      try { p = JSON.parse(r.properties); } catch {}
      const ytId = p.youtube_id;
      if (!ytId) continue;
      const canonicalUid = `yt:${ytId}`;
      let g = byYtId.get(ytId);
      if (!g) {
        g = { canonicalUid, members: [] };
        byYtId.set(ytId, g);
      }
      g.members.push(r);
    }

    const groupsNeedingMerge = [];
    for (const g of byYtId.values()) {
      if (g.members.length === 1 && g.members[0].camera_uid === g.canonicalUid) continue;
      groupsNeedingMerge.push(g);
    }
    if (groupsNeedingMerge.length === 0) return;

    console.log(
      `[cameraStore] migrating ${groupsNeedingMerge.length} YouTube-id groups to canonical uids`
    );

    const stmtDelete = db.prepare('DELETE FROM cameras WHERE camera_uid = ?');
    const stmtUpsertCanonical = db.prepare(`
      INSERT INTO cameras (
        camera_uid, name, camera_type, lat, lon, url, thumbnail_url,
        operator, country, discovery_channels, properties,
        first_seen_at, last_seen_at, seen_count
      ) VALUES (
        @camera_uid, @name, @camera_type, @lat, @lon, @url, @thumbnail_url,
        @operator, @country, @discovery_channels, @properties,
        @first_seen_at, @last_seen_at, @seen_count
      )
      ON CONFLICT(camera_uid) DO UPDATE SET
        discovery_channels = @discovery_channels,
        properties         = @properties,
        first_seen_at      = MIN(cameras.first_seen_at, excluded.first_seen_at),
        last_seen_at       = MAX(cameras.last_seen_at, excluded.last_seen_at),
        seen_count         = cameras.seen_count + excluded.seen_count
    `);

    const tx = db.transaction((groups) => {
      for (const g of groups) {
        // Pick the newest member as the identity-carrier (most recent name/url).
        const sorted = g.members.slice().sort(
          (a, b) => (b.last_seen_at || '').localeCompare(a.last_seen_at || '')
        );
        const winner = sorted[0];
        const fullRow = db.prepare('SELECT * FROM cameras WHERE camera_uid = ?').get(winner.camera_uid);
        if (!fullRow) continue;

        const allChannels = new Set();
        let earliest = fullRow.first_seen_at;
        let latest = fullRow.last_seen_at;
        let seenSum = 0;
        let mergedProps = {};
        for (const m of g.members) {
          try {
            for (const c of JSON.parse(m.discovery_channels)) allChannels.add(c);
          } catch {}
          if (m.first_seen_at && (!earliest || m.first_seen_at < earliest)) earliest = m.first_seen_at;
          if (m.last_seen_at && (!latest || m.last_seen_at > latest)) latest = m.last_seen_at;
          seenSum += m.seen_count || 1;
          try { mergedProps = { ...JSON.parse(m.properties), ...mergedProps }; } catch {}
        }
        mergedProps.camera_uid = g.canonicalUid;

        stmtUpsertCanonical.run({
          camera_uid: g.canonicalUid,
          name: fullRow.name,
          camera_type: fullRow.camera_type,
          lat: fullRow.lat,
          lon: fullRow.lon,
          url: fullRow.url,
          thumbnail_url: fullRow.thumbnail_url,
          operator: fullRow.operator,
          country: fullRow.country || 'JP',
          discovery_channels: JSON.stringify(Array.from(allChannels)),
          properties: JSON.stringify(mergedProps),
          first_seen_at: earliest,
          last_seen_at: latest,
          seen_count: seenSum,
        });

        for (const m of g.members) {
          if (m.camera_uid !== g.canonicalUid) stmtDelete.run(m.camera_uid);
        }
      }
    });
    tx(groupsNeedingMerge);
    console.log('[cameraStore] YouTube-id duplicate collapse complete');
  } catch (err) {
    console.error('[cameraStore] migration failed:', err?.message);
  }
})();

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
