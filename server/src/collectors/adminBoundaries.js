/**
 * Administrative Boundaries Collector
 * OSM `boundary=administrative` relations for Japan at admin_level 4
 * (prefectures) and 7 (municipalities/wards). Returns centroid points
 * tagged with administrative metadata; polygons are heavy enough that we
 * rely on GSI/e-Stat for full geometry elsewhere.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpass(
    [
      'relation["boundary"="administrative"]["admin_level"="4"](area.jp);',
      'relation["boundary"="administrative"]["admin_level"="7"](area.jp);',
    ].join(''),
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        id: `ADM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || null,
        name_ja: el.tags?.name || null,
        admin_level: el.tags?.admin_level ? Number(el.tags.admin_level) : null,
        place: el.tags?.place || null,
        iso_code: el.tags?.['ISO3166-2'] || null,
        wikidata: el.tags?.wikidata || null,
        source: 'osm_overpass',
      },
    }),
    30000,
    { limit: 2000, queryTimeout: 90 },
  );
}

export default async function collectAdminBoundaries() {
  const features = (await tryLive()) || [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'admin-boundaries',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: features.length > 0,
      description: 'Japan administrative boundaries (prefecture + municipality) via OSM',
    },
    metadata: {},
  };
}
