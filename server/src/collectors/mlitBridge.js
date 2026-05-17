/**
 * MLIT bridge inventory (mlit-bridge).
 *
 * MLIT publishes the road-bridge inventory (道路橋梁) as periodic Excel/PDF
 * datasets, not a live geospatial API. As a real, queryable source we use
 * OpenStreetMap via Overpass for bridge structures in Japan
 * (`man_made=bridge` and named `bridge=yes` ways). Honest empty on failure
 * — no fabricated structures.
 */

import { fetchOverpass } from './_liveHelpers.js';

const BODY = `
  way["man_made"="bridge"](area.jp);
  relation["man_made"="bridge"](area.jp);
`;

export default async function collectMlitBridge() {
  let mapped = null;
  try {
    mapped = await fetchOverpass(
      BODY,
      (el, _i, coords) => ({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: coords },
        properties: {
          osm_id: `${el.type}/${el.id}`,
          name: el.tags?.name || el.tags?.['name:ja'] || el.tags?.['name:en'] || null,
          name_en: el.tags?.['name:en'] || null,
          structure: el.tags?.['bridge:structure'] || null,
          layer: el.tags?.layer || null,
          country: 'JP',
          source: 'osm_overpass_bridge',
        },
      }),
      60000,
      { queryTimeout: 180 },
    );
  } catch {
    mapped = null;
  }

  const features = Array.isArray(mapped) ? mapped : [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: features.length ? 'mlit_bridge_osm' : 'mlit_bridge_unavailable',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Japanese road/rail bridges (OSM Overpass; MLIT publishes only Excel/PDF inventory)',
    },
  };
}
