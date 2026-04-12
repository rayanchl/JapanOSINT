/**
 * Water Towers & Treatment Infrastructure Collector
 * OSM `man_made=water_tower` / `man_made=water_works` / `landuse=reservoir`
 * across Japan. Complements the broader water-infra collector with
 * point-level tagged features.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpass(
    [
      'node["man_made"="water_tower"](area.jp);',
      'way["man_made"="water_tower"](area.jp);',
      'node["man_made"="water_works"](area.jp);',
      'way["man_made"="water_works"](area.jp);',
      'node["man_made"="reservoir_covered"](area.jp);',
      'way["man_made"="reservoir_covered"](area.jp);',
    ].join(''),
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        id: `WATER_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || 'Water infrastructure',
        name_ja: el.tags?.name || null,
        kind: el.tags?.man_made || null,
        operator: el.tags?.operator || null,
        height: el.tags?.height || null,
        capacity: el.tags?.capacity || null,
        source: 'osm_overpass',
      },
    }),
    20000,
    { limit: 1000, queryTimeout: 40 },
  );
}

export default async function collectWaterTowers() {
  const features = (await tryLive()) || [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'water-towers',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: features.length > 0,
      description: 'Water towers, waterworks and covered reservoirs via OSM',
    },
    metadata: {},
  };
}
