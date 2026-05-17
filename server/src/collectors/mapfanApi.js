/**
 * MapFan API collector (mapfan-api).
 *
 * MapFan API (ジオテクノロジーズ / インクリメントP) is a paid, key-gated
 * geocoding / POI / map platform with no public unauthenticated data feed.
 * No key-free fallback exists, so without an API key we return an honest
 * empty FeatureCollection (never fabricated POIs). The missing-key state
 * surfaces via the apiCredentials map ("needs API key" badge in the Sources
 * tab).
 *
 * When MAPFAN_API_KEY is present the documented POI-search endpoint is
 * attempted over a Japan bbox; its exact path/params are plan-specific and
 * unverified, so any failure yields honest empty ('mapfan-api_unavailable').
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

// minLon, minLat, maxLon, maxLat
const JP_BBOX = [122.0, 24.0, 146.0, 46.0];

function result(features, source) {
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'MapFan API POI/place data over Japan (requires MAPFAN_API_KEY — paid API)',
    },
  };
}

export default async function collectMapfanApi() {
  const key = envFor('MAPFAN_API_KEY');
  if (!key) return result([], 'mapfan-api_no_key');

  const [w, s, e, n] = JP_BBOX;
  // Key-gated POI search — endpoint/params are plan-specific and unverified.
  // Real fetch attempted with the key; honest empty otherwise.
  const url = 'https://api.mapfan.com/v1/poi/search'
    + `?bbox=${w},${s},${e},${n}&limit=200&key=${encodeURIComponent(key)}`;
  const data = await fetchJson(url, { timeoutMs: 20000 }).catch(() => null);

  const rows = Array.isArray(data?.features) ? data.features
    : Array.isArray(data?.results) ? data.results
      : Array.isArray(data?.pois) ? data.pois : null;
  if (!rows || rows.length === 0) return result([], 'mapfan-api_unavailable');

  const features = rows
    .filter((r) => {
      const lon = r.lon ?? r.longitude ?? r.geometry?.coordinates?.[0];
      const lat = r.lat ?? r.latitude ?? r.geometry?.coordinates?.[1];
      return lon != null && lat != null;
    })
    .map((r) => {
      const lon = r.lon ?? r.longitude ?? r.geometry?.coordinates?.[0];
      const lat = r.lat ?? r.latitude ?? r.geometry?.coordinates?.[1];
      return {
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [+lon, +lat] },
        properties: {
          poi_id: r.id ?? r.poi_id ?? null,
          name: r.name ?? r.title ?? null,
          category: r.category ?? r.genre ?? null,
          address: r.address ?? null,
          source: 'mapfan_api',
        },
      };
    });
  return result(features, 'mapfan-api');
}
