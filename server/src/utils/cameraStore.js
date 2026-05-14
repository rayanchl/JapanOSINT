/**
 * Camera storage layer — thin wrapper around intel_items (the polymorphic
 * master) keyed by `camera-discovery|<camera_uid>`. The legacy `cameras`
 * typed table + `cameras_fts` mirror were retired in the Phase B cutover;
 * this module preserves the read/write API its callers expect (cameraRunner,
 * routes/data.js, llmEnricher) so they don't need to change.
 */

import db from './database.js';
import {
  getItemBySourceUid, selectGeoFeatures,
  applyGeocodeToMaster, applyGeocodeFailToMaster,
  upsertItemSync,
} from './intelStore.js';

const CAMERA_SOURCE_ID = 'camera-discovery';

/**
 * Translate a master intel_items record back into the legacy cameras-row
 * shape callers expect. The properties JSON carries the original feature
 * fields (camera_uid, name, camera_type, url, thumbnail_url, …) because the
 * mirror + upsertCamera preserve them verbatim. lat/lon and timestamps come
 * from the indexed master columns.
 */
function masterItemToCameraRow(item) {
  const props = item.properties || {};
  return {
    camera_uid:         props.camera_uid ?? item.uid?.split('|').slice(1).join('|') ?? null,
    name:               props.name ?? item.title ?? null,
    camera_type:        props.camera_type ?? null,
    lat:                item.lat ?? null,
    lon:                item.lon ?? null,
    url:                props.url ?? item.link ?? null,
    thumbnail_url:      props.thumbnail_url ?? null,
    operator:           props.operator ?? null,
    country:            props.country ?? 'JP',
    discovery_channels: typeof props.discovery_channels === 'string'
      ? props.discovery_channels
      : JSON.stringify(props.discovery_channels || []),
    properties:         JSON.stringify(props),
    first_seen_at:      props.first_seen_at ?? null,
    last_seen_at:       props.last_seen_at ?? item.fetched_at ?? null,
    seen_count:         props.seen_count ?? 1,
    llm_place_name:     props.llm_place_name ?? null,
    llm_geocoded_at:    props.llm_geocoded_at ?? item.geom_at ?? null,
  };
}

/**
 * Upsert a single GeoJSON feature produced by a camera discovery channel.
 * Master-only — preserves channel-union + existing-non-null-wins merge
 * semantics by reading the existing intel_items row's properties JSON.
 *
 * @returns {{kind: 'new'|'updated', camera: GeoJSON feature} | null}
 */
export function upsertCamera(feature, channel) {
  const p = feature?.properties || {};
  const coords = feature?.geometry?.coordinates;
  if (!p.camera_uid || !Array.isArray(coords) || coords.length < 2) return null;
  const [lon, lat] = coords;
  if (!Number.isFinite(lat) || !Number.isFinite(lon)) return null;

  const masterUid = `${CAMERA_SOURCE_ID}|${p.camera_uid}`;
  const existing = getItemBySourceUid(CAMERA_SOURCE_ID, p.camera_uid);
  const prevProps = existing?.properties || {};
  const prevChannels = Array.isArray(prevProps.discovery_channels)
    ? prevProps.discovery_channels
    : [];
  const nextChannels = Array.from(new Set([
    ...prevChannels, channel, ...(p.discovery_channels || []),
  ].filter(Boolean)));

  const mergedName     = prevProps.name          || p.name          || 'Unknown camera';
  const mergedType     = prevProps.camera_type   || p.camera_type   || 'unknown';
  const mergedUrl      = prevProps.url           || p.url           || null;
  const mergedThumb    = prevProps.thumbnail_url || p.thumbnail_url || null;
  const mergedOperator = prevProps.operator      || p.operator      || null;
  const mergedCountry  = prevProps.country       || p.country       || 'JP';
  const mergedFirstSeen = prevProps.first_seen_at || new Date().toISOString();
  const seenCount      = (prevProps.seen_count ?? 0) + 1;
  const mergedProps = {
    ...p,
    ...prevProps,
    camera_uid:         p.camera_uid,
    name:               mergedName,
    camera_type:        mergedType,
    url:                mergedUrl,
    thumbnail_url:      mergedThumb,
    operator:           mergedOperator,
    country:            mergedCountry,
    discovery_channels: nextChannels,
    first_seen_at:      mergedFirstSeen,
    last_seen_at:       new Date().toISOString(),
    seen_count:         seenCount,
  };

  const result = upsertItemSync({
    uid:        masterUid,
    title:      mergedName,
    lat,
    lon,
    geomSource: existing?.geom_source === 'llm' ? 'llm' : 'native',
    geometry:   { type: 'Point', coordinates: [lon, lat] },
    recordType: 'camera',
    properties: mergedProps,
  }, CAMERA_SOURCE_ID);

  return {
    kind: result?.kind ?? 'new',
    camera: {
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [lon, lat] },
      properties: mergedProps,
    },
  };
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

/** LLM enricher success path. Master-only write. */
export function applyGeocodeOk({ camera_uid, lat, lon }) {
  return applyGeocodeToMaster({ uid: `${CAMERA_SOURCE_ID}|${camera_uid}`, lat, lon });
}

/** LLM enricher failure path. Increments geom_failed; sentinels at 5. */
export function applyGeocodeFail({ camera_uid }) {
  return applyGeocodeFailToMaster({ uid: `${CAMERA_SOURCE_ID}|${camera_uid}` });
}

/**
 * Look up a single camera by `camera_uid`. Reads from intel_items and shapes
 * the row to look like the legacy cameras-table row so callers (camera
 * popup, snapshot endpoint) keep working unchanged.
 */
export function getCameraByUid(uid) {
  if (!uid) return null;
  const item = getItemBySourceUid(CAMERA_SOURCE_ID, uid);
  return item ? masterItemToCameraRow(item) : null;
}

/** Whole-camera FeatureCollection backed by intel_items. */
export function getAllCameras() {
  const fc = selectGeoFeatures({ sourceId: CAMERA_SOURCE_ID });
  return {
    ...fc,
    _meta: {
      source: 'camera_store',
      recordCount: fc.features.length,
      fetchedAt: new Date().toISOString(),
      served_from: 'intel_items',
    },
  };
}

export function getRecentCameras(limit = 200) {
  const rows = db.prepare(
    `SELECT uid, lat, lon, properties, fetched_at
       FROM intel_items
      WHERE source_id = ?
        AND lat IS NOT NULL
      ORDER BY fetched_at DESC
      LIMIT ?`,
  ).all(CAMERA_SOURCE_ID, limit);
  return rows.map((r) => {
    let properties = {};
    try { properties = JSON.parse(r.properties); } catch { /* ignore */ }
    return {
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [r.lon, r.lat] },
      properties: {
        ...properties,
        camera_uid: properties.camera_uid ?? r.uid?.split('|').slice(1).join('|'),
      },
    };
  });
}

/**
 * Backfill feed for the Camera Discovery thread. Returns rows from
 * intel_items in reverse-chronological order with the shape the WS hook
 * already consumes ({ts, kind, channel, camera, run_id}). Covers both the
 * fused `record_type='camera'` rows written by `upsertCamera` and the
 * per-channel `record_type='camera-discovery'` rows written by the bulk
 * mirror — so every channel that has ever surfaced a camera shows up here
 * even when no live run is in flight.
 *
 * Pagination: pass the previous response's `cursor` to fetch the next page.
 * The cursor is an opaque base64 of `<ts>|<uid>` and uses a (ts, uid) tuple
 * comparison to stay stable when rows share a timestamp.
 */
export function getDiscoveryFeed({ limit = 500, cursor = null, channel = null } = {}) {
  const cap = Math.max(1, Math.min(5000, Number(limit) || 500));
  const where = [
    "source_id = ?",
    "record_type IN ('camera','camera-discovery')",
    "lat IS NOT NULL",
  ];
  const params = [CAMERA_SOURCE_ID];

  // Channel filter — exact match on primary discovery_channel, with a
  // fallback for fused rows that store the channel inside discovery_channels[].
  if (channel) {
    where.push(`(
      json_extract(properties, '$.discovery_channel') = ?
      OR EXISTS (SELECT 1 FROM json_each(json_extract(properties, '$.discovery_channels'))
                 WHERE json_each.value = ?)
    )`);
    params.push(channel, channel);
  }

  // Cursor: rows strictly older than the (ts, uid) anchor. ts orders rows
  // by last activity; uid breaks ties so identical timestamps don't drop
  // entries on subsequent pages.
  let cursorTs = null;
  let cursorUid = null;
  if (cursor) {
    try {
      const decoded = Buffer.from(String(cursor), 'base64').toString('utf8');
      const sep = decoded.indexOf('|');
      if (sep > 0) {
        cursorTs  = decoded.slice(0, sep);
        cursorUid = decoded.slice(sep + 1);
      }
    } catch { /* malformed cursor: ignore, return from the top */ }
  }
  if (cursorTs && cursorUid) {
    where.push(`(
      COALESCE(json_extract(properties, '$.last_seen_at'), fetched_at) < ?
      OR (COALESCE(json_extract(properties, '$.last_seen_at'), fetched_at) = ? AND uid < ?)
    )`);
    params.push(cursorTs, cursorTs, cursorUid);
  }

  // limit + 1 lets us know if there's another page without a second query.
  const rows = db.prepare(`
    SELECT uid, lat, lon, geometry, properties, fetched_at,
           COALESCE(json_extract(properties, '$.last_seen_at'), fetched_at) AS ts
      FROM intel_items
     WHERE ${where.join(' AND ')}
     ORDER BY ts DESC, uid DESC
     LIMIT ?
  `).all(...params, cap + 1);

  const hasMore = rows.length > cap;
  const page = hasMore ? rows.slice(0, cap) : rows;

  const events = page.map((r) => {
    let properties = {};
    try { properties = r.properties ? JSON.parse(r.properties) : {}; } catch { /* keep empty */ }
    let geometry = null;
    if (r.geometry) {
      try { geometry = JSON.parse(r.geometry); } catch { /* fall through */ }
    }
    if (!geometry && r.lat != null && r.lon != null) {
      geometry = { type: 'Point', coordinates: [r.lon, r.lat] };
    }
    return {
      ts: r.ts,
      kind: 'historical',
      channel: properties.discovery_channel
            || (Array.isArray(properties.discovery_channels) ? properties.discovery_channels[0] : null)
            || 'unknown',
      camera: {
        type: 'Feature',
        geometry,
        properties: {
          ...properties,
          camera_uid: properties.camera_uid ?? r.uid?.split('|').slice(1).join('|'),
        },
      },
      run_id: null,
    };
  });

  let nextCursor = null;
  if (hasMore) {
    const last = page[page.length - 1];
    nextCursor = Buffer.from(`${last.ts}|${last.uid}`, 'utf8').toString('base64');
  }
  return { events, cursor: nextCursor };
}

export function cameraStats() {
  const stats = db.prepare(
    `SELECT COUNT(*) AS total,
            SUM(CASE WHEN fetched_at >= datetime('now','-24 hours') THEN 1 ELSE 0 END) AS new24h
       FROM intel_items
      WHERE source_id = ?`,
  ).get(CAMERA_SOURCE_ID);
  const byType = db.prepare(
    `SELECT json_extract(properties, '$.camera_type') AS camera_type,
            COUNT(*) AS c
       FROM intel_items
      WHERE source_id = ?
      GROUP BY json_extract(properties, '$.camera_type')`,
  ).all(CAMERA_SOURCE_ID);
  return {
    total:  stats?.total  ?? 0,
    new24h: stats?.new24h ?? 0,
    byType,
  };
}

export default {
  upsertCamera, upsertCamerasTx,
  getAllCameras, getRecentCameras, cameraStats, getDiscoveryFeed,
  getCameraByUid, applyGeocodeOk, applyGeocodeFail,
};
