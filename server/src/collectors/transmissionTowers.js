/**
 * Transmission Towers & Power Poles Collector
 * OSM `power=tower` / `tower:type=transmission` / `power=pole` across Japan.
 * Separate from the aggregate electrical-grid collector so that the raw tower
 * and pole inventory is available as its own layer.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpass(
    [
      'node["power"="tower"](area.jp);',
      'node["tower:type"="transmission"](area.jp);',
      'node["power"="pole"]["tower:type"!="lighting"](area.jp);',
      'node["power"="substation"](area.jp);way["power"="substation"](area.jp);',
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
    25000,
    // Towers/poles are dense; cap is higher and we scope to transmission-class.
    { limit: 2000, queryTimeout: 60 },
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
      description: 'Electric transmission towers, poles and substations via OSM',
    },
    metadata: {},
  };
}
