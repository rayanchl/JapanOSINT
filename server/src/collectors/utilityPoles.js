/**
 * Utility Poles Collector
 * OSM `man_made=utility_pole` across Japan. Complements the transmission
 * tower collector with telecom / low-voltage distribution assets.
 *
 * Tiled fetch — Japan has hundreds of thousands of OSM-mapped poles.
 */

import { fetchOverpassTiled } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpassTiled(
    (bbox) => [
      `node["man_made"="utility_pole"](${bbox});`,
      `node["utility"="power"](${bbox});`,
      `node["utility"="telecom"](${bbox});`,
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
    { queryTimeout: 180, timeoutMs: 90_000 },
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
      description: 'Utility poles (power/telecom distribution) via tiled OSM',
    },
    metadata: {},
  };
}
