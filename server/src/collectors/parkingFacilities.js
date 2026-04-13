/**
 * Parking Facilities Collector
 * OSM `amenity=parking` / `amenity=parking_entrance` across Japan.
 *
 * Uses tiled Overpass (12 sub-region bboxes) because the nationwide dataset
 * is far too large for a single Overpass call (>500k elements).
 */

import { fetchOverpassTiled } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpassTiled(
    (bbox) => [
      `node["amenity"="parking"](${bbox});`,
      `way["amenity"="parking"](${bbox});`,
      `node["amenity"="parking_entrance"](${bbox});`,
    ].join(''),
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
    { queryTimeout: 180, timeoutMs: 90_000 },
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
      description: 'Parking facilities in Japan (tiled Overpass nationwide)',
    },
    metadata: {},
  };
}
