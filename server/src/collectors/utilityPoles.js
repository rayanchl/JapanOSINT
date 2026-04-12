/**
 * Utility Poles Collector
 * OSM `man_made=utility_pole` across Japan. Complements the transmission
 * tower collector with telecom / low-voltage distribution assets.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpass(
    [
      'node["man_made"="utility_pole"](area.jp);',
      'node["utility"="power"](area.jp);',
      'node["utility"="telecom"](area.jp);',
    ].join(''),
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        id: `UPOLE_${el.id}`,
        name: el.tags?.name || null,
        utility: el.tags?.utility || null,
        material: el.tags?.material || null,
        height: el.tags?.height || null,
        ref: el.tags?.ref || null,
        operator: el.tags?.operator || null,
        source: 'osm_overpass',
      },
    }),
    25000,
    { limit: 2000, queryTimeout: 60 },
  );
}

export default async function collectUtilityPoles() {
  const features = (await tryLive()) || [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'utility-poles',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: features.length > 0,
      description: 'Utility poles (power/telecom distribution) via OSM',
    },
    metadata: {},
  };
}
