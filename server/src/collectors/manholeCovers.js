/**
 * Decorative / designed manhole covers (manhole-covers).
 *
 * The GKP マンホールカード program has no open bulk geospatial API. The
 * best real, queryable public dataset of physically-located manholes is
 * OpenStreetMap (`man_made=manhole`). We query Overpass for manhole nodes
 * across Japan. Honest empty FeatureCollection on failure — never seeded.
 */

import { fetchOverpassTiled } from './_liveHelpers.js';

export default async function collectManholeCovers() {
  let mapped = null;
  try {
    mapped = await fetchOverpassTiled(
      (bbox) => `node["man_made"="manhole"](${bbox});`,
      (el, _i, coords) => ({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: coords },
        properties: {
          osm_id: `${el.type}/${el.id}`,
          name: el.tags?.name || null,
          manhole_type: el.tags?.manhole || null,
          operator: el.tags?.operator || null,
          country: 'JP',
          source: 'osm_overpass_manhole',
        },
      }),
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
      source: features.length ? 'manhole_covers_osm' : 'manhole_covers_unavailable',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Manhole covers in Japan from OSM (GKP manhole-card program has no open bulk API)',
    },
  };
}
