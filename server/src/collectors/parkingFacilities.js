/**
 * Parking Facilities Collector
 * OSM `amenity=parking` / `amenity=parking_entrance` across Japan.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpass(
    'node["amenity"="parking"](area.jp);way["amenity"="parking"](area.jp);node["amenity"="parking_entrance"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        id: `PARK_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || 'Parking',
        name_ja: el.tags?.name || null,
        parking_type: el.tags?.parking || null,
        access: el.tags?.access || 'public',
        fee: el.tags?.fee || null,
        capacity: el.tags?.capacity ? Number(el.tags.capacity) : null,
        operator: el.tags?.operator || null,
        supervised: el.tags?.supervised || null,
        source: 'osm_overpass',
      },
    }),
    20000,
    { limit: 1500, queryTimeout: 50 },
  );
}

export default async function collectParkingFacilities() {
  const features = (await tryLive()) || [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'parking-facilities',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: features.length > 0,
      description: 'Parking facilities in Japan via OSM amenity=parking',
    },
    metadata: {},
  };
}
