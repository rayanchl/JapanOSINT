/**
 * Bridge from existing collector output (FeatureCollection / intel envelope /
 * hybrid) to the polymorphic intel_items master table.
 *
 * Phase A of the pipeline overhaul: every collector continues to emit its
 * existing shape, but we *also* mirror every row into intel_items as a side
 * effect of `respondWithData`. This makes intel_items the unified store —
 * geocoded rows AND ungeocoded rows land here, regardless of whether the
 * collector was historically "spatial" (FC) or "intel-only".
 *
 * Phase B (later) cuts the map read path over to intel_items and retires the
 * typed tables. This file makes the cutover possible without touching ~250
 * collectors.
 */

import crypto from 'node:crypto';
import { upsertItems } from './intelStore.js';

/**
 * Stable uid for a feature. Collectors that already set
 * `feature.properties.uid` / `id` win; otherwise we hash geometry+properties
 * so re-runs upsert in place. Always namespaced by sourceId so two sources
 * can't collide on the same intrinsic id.
 */
// Domain-specific natural-id keys we'll prefer over hash-based uids. Keep in
// the order checked: more-specific first, generic last. Adding `_uid`-keyed
// columns from the typed tables here keeps live-mirrored rows aligned with
// the Phase B backfill (which used the same natural keys), so we never end
// up with two buckets per camera / station / line / post.
const NATIVE_ID_KEYS = [
  'camera_uid', 'station_uid', 'line_uid', 'cluster_uid', 'footprint_id',
  'post_uid', 'event_id', 'earthquake_id', 'buoy_id', 'station_id',
  'spring_id', 'stop_id',
  'uid', 'id', 'uuid',
];

function featureUid(feature, sourceId) {
  const props = feature?.properties || {};
  for (const k of NATIVE_ID_KEYS) {
    const v = props[k];
    if (v != null && v !== '') return `${sourceId}|${String(v)}`;
  }
  if (feature?.id != null) return `${sourceId}|${String(feature.id)}`;
  const fingerprint = JSON.stringify({ g: feature?.geometry || null, p: props });
  const hash = crypto.createHash('sha1').update(fingerprint).digest('hex').slice(0, 16);
  return `${sourceId}|h:${hash}`;
}

/**
 * Centroid of a GeoJSON geometry. For Point we return the point itself; for
 * polygons / lines / multi-shapes we return the bbox centre, which is good
 * enough for "where on the map does this row live" without a full centroid
 * algorithm. Returns [lon, lat] or null if no usable coords.
 */
function geometryCentroid(geom) {
  if (!geom || !geom.type) return null;
  const t = geom.type;
  const c = geom.coordinates;
  if (!c) return null;

  if (t === 'Point') {
    return Array.isArray(c) && Number.isFinite(c[0]) && Number.isFinite(c[1]) ? [c[0], c[1]] : null;
  }

  // Walk all coordinates and compute bbox. Handles Line/Polygon/Multi*.
  let minLon = Infinity, maxLon = -Infinity, minLat = Infinity, maxLat = -Infinity;
  let count = 0;
  const visit = (node) => {
    if (!Array.isArray(node)) return;
    if (typeof node[0] === 'number' && typeof node[1] === 'number') {
      if (Number.isFinite(node[0]) && Number.isFinite(node[1])) {
        if (node[0] < minLon) minLon = node[0];
        if (node[0] > maxLon) maxLon = node[0];
        if (node[1] < minLat) minLat = node[1];
        if (node[1] > maxLat) maxLat = node[1];
        count += 1;
      }
      return;
    }
    for (const child of node) visit(child);
  };
  visit(c);
  if (count === 0) return null;
  return [(minLon + maxLon) / 2, (minLat + maxLat) / 2];
}

/**
 * Map a sourceId to a record_type. Cheap heuristic — the sourceId itself is
 * usually a strong type signal in this codebase ('aed-map' → 'aed', 'cell-towers'
 * → 'cell-tower', etc.). Collectors can override per-row by setting
 * `feature.properties.record_type` or `properties.kind`.
 */
function inferRecordType(sourceId, props) {
  return props?.record_type ?? props?.kind ?? sourceId ?? null;
}

function pickText(props, ...keys) {
  for (const k of keys) {
    const v = props?.[k];
    if (v != null && typeof v === 'string' && v.trim()) return v.trim();
  }
  return null;
}

/**
 * Convert one GeoJSON Feature into a master item. Handles missing / non-Point
 * geometries: the indexed lat/lon hold the centroid, the full geometry is
 * preserved in the `geometry` column for later map rendering.
 */
function featureToMasterItem(feature, sourceId) {
  if (!feature || typeof feature !== 'object') return null;
  const props = feature.properties || {};
  const geom = feature.geometry || null;
  const centroid = geometryCentroid(geom);
  const lat = centroid ? centroid[1] : null;
  const lon = centroid ? centroid[0] : null;
  return {
    uid: featureUid(feature, sourceId),
    recordType: inferRecordType(sourceId, props),
    subSourceId: props.sub_source_id ?? props.channel ?? null,
    lat,
    lon,
    geomSource: (lat != null && lon != null) ? 'native' : null,
    geometry: geom,
    title:        pickText(props, 'title', 'name', 'name_ja', 'label'),
    summary:      pickText(props, 'summary', 'description', 'desc'),
    link:         pickText(props, 'link', 'url', 'href'),
    author:       pickText(props, 'author', 'operator'),
    language:     pickText(props, 'language', 'lang'),
    published_at: pickText(props, 'published_at', 'observed_at', 'time', 'timestamp'),
    tags:         Array.isArray(props.tags) ? props.tags : [],
    properties:   props,
  };
}

/**
 * Mirror collector output into intel_items. Accepts:
 *   - { type: 'FeatureCollection', features, _meta? }
 *   - { kind: 'intel', items, meta? }
 *   - { type: 'FeatureCollection', features, intel: { items } } (hybrid)
 *   - bare array of features (some legacy collectors)
 *
 * Returns { features: { count, geocoded, ungeocoded }, intel: {…} }.
 * Errors are swallowed and logged — mirroring is a side effect; we never
 * want a mirror failure to break the primary map response.
 */
export async function mirrorCollectorOutput(data, sourceId, fetchedAtIso) {
  const result = {
    features: { count: 0, geocoded: 0, ungeocoded: 0 },
    intel:    { count: 0, geocoded: 0, ungeocoded: 0 },
  };
  if (!data || !sourceId) return result;
  const fetchedAt = fetchedAtIso || new Date().toISOString();

  // Extract FC features (if any) and intel items (if any). One collector
  // output can carry both via the hybrid shape.
  const featureArray = Array.isArray(data?.features)
    ? data.features
    : (Array.isArray(data) ? data : []);
  const intelItemsRaw = Array.isArray(data?.items) && data?.kind === 'intel'
    ? data.items
    : (Array.isArray(data?.intel?.items) ? data.intel.items : []);

  // Mirror FC features.
  if (featureArray.length > 0) {
    const items = featureArray
      .map((f) => featureToMasterItem(f, sourceId))
      .filter(Boolean);
    if (items.length > 0) {
      try {
        const r = await upsertItems(items, sourceId, fetchedAt);
        result.features = {
          count: r?.count ?? 0,
          geocoded: r?.geocoded ?? 0,
          ungeocoded: r?.ungeocoded ?? 0,
        };
      } catch (err) {
        console.warn(`[mirror] FC mirror failed for ${sourceId}:`, err?.message);
      }
    }
  }

  // Mirror intel items. The shape from intel collectors is already master-
  // friendly (uid, properties, …) — we just normalise field names.
  if (intelItemsRaw.length > 0) {
    const items = intelItemsRaw.map((it) => normaliseIntelItem(it, sourceId)).filter(Boolean);
    if (items.length > 0) {
      try {
        const r = await upsertItems(items, sourceId, fetchedAt);
        result.intel = {
          count: r?.count ?? 0,
          geocoded: r?.geocoded ?? 0,
          ungeocoded: r?.ungeocoded ?? 0,
        };
      } catch (err) {
        console.warn(`[mirror] intel mirror failed for ${sourceId}:`, err?.message);
      }
    }
  }

  return result;
}

/**
 * Normalise an intel envelope item into the master upsert shape. Existing
 * intel collectors already emit { uid, title, body, ..., properties } so this
 * is mostly identity; we only add the polymorphic fields if present.
 */
function normaliseIntelItem(item, sourceId) {
  if (!item || typeof item !== 'object') return null;
  const uid = item.uid != null ? String(item.uid) : null;
  if (!uid) return null;
  // intel items historically have no native lat/lon; collectors that *do*
  // know location should set item.lat / item.lon and they'll flow through.
  return {
    uid,
    recordType:    item.record_type ?? item.recordType ?? sourceId,
    subSourceId:   item.sub_source_id ?? item.subSourceId ?? null,
    lat:           item.lat ?? null,
    lon:           item.lon ?? null,
    geomSource:    item.geom_source ?? item.geomSource ?? null,
    geometry:      item.geometry ?? null,
    title:         item.title ?? null,
    body:          item.body ?? null,
    summary:       item.summary ?? null,
    link:          item.link ?? null,
    author:        item.author ?? null,
    language:      item.language ?? null,
    published_at:  item.published_at ?? null,
    tags:          item.tags ?? [],
    properties:    item.properties ?? {},
  };
}
