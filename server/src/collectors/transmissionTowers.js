/**
 * Transmission Towers & Power Poles Collector
 * OSM `power=tower` / `tower:type=transmission` / `power=pole` across Japan.
 * Separate from the aggregate electrical-grid collector so that the raw tower
 * and pole inventory is available as its own layer.
 *
 * Tiled fetch — the nationwide power tower/pole inventory exceeds 100k nodes.
 */

import { fetchOverpassTiled } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpassTiled(
    (bbox) => [
      `node["power"="tower"](${bbox});`,
      `node["tower:type"="transmission"](${bbox});`,
      `node["power"="pole"]["tower:type"!="lighting"](${bbox});`,
      `node["power"="substation"](${bbox});`,
      `way["power"="substation"](${bbox});`,
    ].join(''),
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        id: `PWR_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || null,
        power: el.tags?.power || null,
        tower_type: el.tags?.['tower:type'] || null,
        voltage: el.tags?.voltage || null,
        material: el.tags?.material || null,
        ref: el.tags?.ref || null,
        operator: el.tags?.operator || null,
        source: 'osm_overpass',
      },
    }),
    { queryTimeout: 180, timeoutMs: 90_000 },
  );
}

export default async function collectTransmissionTowers() {
  const features = (await tryLive()) || [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'transmission-towers',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: features.length > 0,
      description: 'Electric transmission towers, poles and substations via tiled OSM',
    },
    metadata: {},
  };
}
