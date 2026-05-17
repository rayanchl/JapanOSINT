/**
 * OpenStreetMap Japan — general points of interest layer.
 *
 * Real data via Overpass. There is no single "OSM-JP" dataset, so we query a
 * meaningful, bounded nationwide POI set: town halls, hospitals, fire stations,
 * police stations, post offices and railway stations tagged in OSM within
 * Japan. This is real OSM data (no fabrication); on Overpass failure we return
 * an honest empty FeatureCollection.
 */

import { fetchOverpass } from './_liveHelpers.js';

const OVERPASS_BODY = [
  'node["amenity"="townhall"](area.jp);',
  'node["amenity"="hospital"](area.jp);',
  'node["amenity"="fire_station"](area.jp);',
  'node["amenity"="police"](area.jp);',
  'node["amenity"="post_office"](area.jp);',
  'node["railway"="station"](area.jp);',
].join('');

function emptyResult(source) {
  return {
    type: 'FeatureCollection',
    features: [],
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: 0,
      description: 'OpenStreetMap Japan civic / transport points of interest (Overpass)',
    },
  };
}

export default async function collectOpenstreetmapJp() {
  let features = null;
  try {
    features = await fetchOverpass(
      OVERPASS_BODY,
      (el, i, coords) => ({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: coords },
        properties: {
          osm_id: el.type && el.id != null ? `${el.type}/${el.id}` : null,
          name: el.tags?.name ?? el.tags?.['name:en'] ?? null,
          name_en: el.tags?.['name:en'] ?? null,
          category: el.tags?.amenity ?? el.tags?.railway ?? null,
          operator: el.tags?.operator ?? null,
          address: el.tags?.['addr:full'] ?? el.tags?.['addr:city'] ?? null,
          source: 'openstreetmap',
        },
      }),
      60_000,
      { queryTimeout: 240 },
    );
  } catch {
    features = null;
  }

  if (!Array.isArray(features) || features.length === 0) {
    return emptyResult('openstreetmap_jp_unavailable');
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'openstreetmap',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'OpenStreetMap Japan civic / transport points of interest (Overpass)',
    },
  };
}
