/**
 * Factory for the `unifiedX` collectors that fuse two-or-more upstream sources.
 *
 * Each unified collector follows the same shape:
 *   1. Run several upstream collectors in parallel (Promise.allSettled).
 *   2. Optionally tag features with a per-upstream `kind` (e.g. 'stop' vs 'terminal').
 *   3. Merge into a flat feature array.
 *   4. Optional filter (e.g. exclude tram_stops in unifiedTrains).
 *   5. Dedupe by ordered key functions (qualified id first, then coord-grid).
 *   6. Optional per-feature post-process (e.g. ensureLineColor).
 *   7. Wrap in the canonical FeatureCollection envelope with upstream counts.
 *
 * The factory bakes steps 1, 2, 3, 5, 7 — callers pass a config object.
 *
 * `unifiedAisShips` does NOT use this factory because its fusion logic
 * (mmsi/imo/name+coord with freshness merge) is too custom — leave it alone.
 */

import {
  mergeFeatureCollections,
  dedupeByKeys,
  countBySource,
} from '../collectors/_dedupe.js';

/**
 * @param {object} opts
 * @param {string} opts.sourceId
 * @param {string} opts.description
 * @param {Array<{name: string, fn: () => Promise<any>, kind?: string}>} opts.upstreams
 *   `name` is the registry id used in `_meta.upstream`. `fn` is the collector.
 *   `kind` (optional) tags every feature in that upstream with `properties.kind`
 *   if not already set.
 * @param {Array<(f: any) => string|null>} [opts.dedupeKeys]  - if absent, no dedupe
 * @param {object} [opts.dedupeOpts]                          - e.g. { coordPrecision: 4 }
 * @param {(f: any) => boolean} [opts.filter]
 * @param {(f: any) => any} [opts.postProcess]
 * @param {() => object} [opts.extraMeta]   - extra fields merged into _meta
 * @returns {() => Promise<object>}
 */
export function createUnifiedCollector({
  sourceId,
  description,
  upstreams,
  dedupeKeys = null,
  dedupeOpts = {},
  filter = null,
  postProcess = null,
  extraMeta = null,
}) {
  if (!sourceId) throw new Error('createUnifiedCollector: sourceId required');
  if (!Array.isArray(upstreams) || upstreams.length === 0) {
    throw new Error('createUnifiedCollector: upstreams[] required');
  }

  return async function collect() {
    const settled = await Promise.allSettled(upstreams.map((u) => u.fn()));

    // Tag each upstream's features with `kind` (only when the upstream specifies one).
    const tagged = settled.map((s, i) => {
      if (s.status !== 'fulfilled') return null;
      const u = upstreams[i];
      if (!u.kind || !s.value || !Array.isArray(s.value.features)) return s.value;
      return {
        ...s.value,
        features: s.value.features.map((f) => ({
          ...f,
          properties: { ...f.properties, kind: f.properties?.kind || u.kind },
        })),
      };
    });

    let raw = mergeFeatureCollections(tagged);
    if (filter) raw = raw.filter(filter);

    let features = dedupeKeys
      ? dedupeByKeys(raw, dedupeKeys, dedupeOpts)
      : raw;

    if (postProcess) features = features.map(postProcess);

    const upstream = {};
    upstreams.forEach((u, i) => {
      const v = settled[i].status === 'fulfilled' ? settled[i].value : null;
      upstream[u.name] = v?.features?.length || 0;
    });

    return {
      type: 'FeatureCollection',
      features,
      _meta: {
        source: sourceId,
        fetchedAt: new Date().toISOString(),
        recordCount: features.length,
        upstream,
        bySource: countBySource(features),
        description,
        ...(extraMeta ? extraMeta() : {}),
      },
    };
  };
}
