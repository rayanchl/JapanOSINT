/**
 * Shared deduplication helpers for fused transit + AIS collectors.
 *
 * dedupeByKeys(features, keyFns) — first non-null key value wins the slot.
 * dedupeByCoordGrid(features, precision) — bucket features by rounded lat/lon.
 * mergeFeatureCollections(results) — flatten a list of GeoJSON collections
 *                                    into a single feature array.
 */

export function mergeFeatureCollections(results) {
  const out = [];
  for (const r of results) {
    if (!r) continue;
    const feats = Array.isArray(r) ? r
      : Array.isArray(r.features) ? r.features
      : [];
    for (const f of feats) {
      if (f && f.geometry && f.properties) out.push(f);
    }
  }
  return out;
}

/** Normalise a Japanese station/stop name for fuzzy matching. */
export function normName(raw) {
  if (!raw) return '';
  return String(raw)
    .toLowerCase()
    .replace(/駅|station|停留所|バス停|stop/gi, '')
    .replace(/[\s・\-()（）、,.]/g, '')
    .trim();
}

/**
 * Dedupe using ordered key functions. The first keyFn that returns a
 * non-empty string becomes the feature's dedup key. Features with no
 * valid key fall through to a coord-grid bucket.
 */
export function dedupeByKeys(features, keyFns, { coordPrecision = 4 } = {}) {
  const seen = new Map(); // key -> feature
  const kept = [];

  for (const f of features) {
    let key = null;
    for (const fn of keyFns) {
      const k = fn(f);
      if (k) { key = k; break; }
    }
    if (!key) {
      const [lon, lat] = f.geometry.coordinates || [];
      if (Number.isFinite(lon) && Number.isFinite(lat)) {
        key = `_c:${lon.toFixed(coordPrecision)},${lat.toFixed(coordPrecision)}:${normName(f.properties?.name)}`;
      } else {
        continue;
      }
    }
    if (seen.has(key)) {
      // Merge properties: fill in nulls from the newer record
      const existing = seen.get(key);
      const merged = { ...existing.properties };
      for (const [k, v] of Object.entries(f.properties || {})) {
        if (merged[k] == null && v != null) merged[k] = v;
      }
      // Track which upstream sources contributed
      const sources = new Set();
      if (existing.properties?.source) sources.add(existing.properties.source);
      if (f.properties?.source) sources.add(f.properties.source);
      merged.sources = Array.from(sources);
      existing.properties = merged;
    } else {
      const copy = {
        type: 'Feature',
        geometry: f.geometry,
        properties: { ...f.properties, sources: [f.properties?.source].filter(Boolean) },
      };
      seen.set(key, copy);
      kept.push(copy);
    }
  }
  return kept;
}

/** Count features by upstream source (uses `sources[]` or falls back to `source`). */
export function countBySource(features) {
  const counts = {};
  for (const f of features) {
    const list = f.properties?.sources
      || (f.properties?.source ? [f.properties.source] : ['unknown']);
    for (const s of list) counts[s] = (counts[s] || 0) + 1;
  }
  return counts;
}
