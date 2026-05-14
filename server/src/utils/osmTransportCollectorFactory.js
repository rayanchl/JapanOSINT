/**
 * Factory for the OSM transport collectors.
 *
 * The five `osmTransport*` files share an identical shape:
 *   1. Run a tiled Overpass query (point or way geometry).
 *   2. Map each OSM element to a GeoJSON feature.
 *   3. Wrap the result in the canonical FeatureCollection envelope.
 *
 * The factory handles steps 1 and 3. Callers provide the QL body builder, the
 * mapper, and the static envelope metadata.
 *
 * `geometry: 'point'` (default) → `fetchOverpassTiled` (out center).
 * `geometry: 'way'`               → `fetchOverpassWaysTiled` (out geom).
 */

import { fetchOverpassTiled, fetchOverpassWaysTiled } from '../collectors/_liveHelpers.js';

/**
 * @param {object} opts
 * @param {string} opts.sourceId
 * @param {string} opts.description
 * @param {(bbox: string) => string} opts.body  - returns the Overpass body for a bbox
 * @param {(el: any, i: number, coords: any) => any} opts.feature - element → Feature mapper
 * @param {'point'|'way'} [opts.geometry='point']
 * @param {{ queryTimeout?: number, timeoutMs?: number }} [opts.overpassOpts]
 */
export function createOsmTransportCollector({
  sourceId,
  description,
  body,
  feature,
  geometry = 'point',
  overpassOpts = {},
}) {
  if (!sourceId) throw new Error('createOsmTransportCollector: sourceId required');
  if (typeof body !== 'function') throw new Error('createOsmTransportCollector: body must be a function');
  if (typeof feature !== 'function') throw new Error('createOsmTransportCollector: feature must be a function');

  const fetcher = geometry === 'way' ? fetchOverpassWaysTiled : fetchOverpassTiled;
  const defaults = geometry === 'way'
    ? { queryTimeout: 180, timeoutMs: 180_000 }
    : { queryTimeout: 180, timeoutMs: 120_000 };

  return async function collect() {
    const features = await fetcher(body, feature, { ...defaults, ...overpassOpts });
    const list = (features || []).filter(Boolean);
    return {
      type: 'FeatureCollection',
      features: list,
      _meta: {
        source: sourceId,
        fetchedAt: new Date().toISOString(),
        recordCount: list.length,
        live: list.length > 0,
        description,
      },
    };
  };
}
