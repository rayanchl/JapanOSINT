/**
 * Yahoo! Japan local-search collector (yahoo-map-api).
 *
 * Live: Yahoo! Japan Local Search API (landmark POIs across major metros)
 * when YAHOO_APP_ID is set. No key-free fallback; without an app id (or on
 * failure) returns an honest empty result — never fabricated POIs. Missing
 * key surfaces via the apiCredentials map.
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

// Metro centroids to scan; the API returns POIs with real lat/lon.
const SCAN = [
  { lat: 35.6812, lon: 139.7671 }, // Tokyo
  { lat: 34.7024, lon: 135.4959 }, // Osaka
  { lat: 35.1709, lon: 136.8815 }, // Nagoya
  { lat: 33.5897, lon: 130.4207 }, // Fukuoka
  { lat: 43.0687, lon: 141.3508 }, // Sapporo
];

function result(features, source) {
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Yahoo! Japan local-search POIs (requires YAHOO_APP_ID)',
    },
  };
}

export default async function collectYahooMapApi() {
  const appId = envFor('YAHOO_APP_ID');
  if (!appId) return result([], 'yahoo_no_key');

  const features = [];
  for (const c of SCAN) {
    const url = 'https://map.yahooapis.jp/search/local/V1/localSearch'
      + `?appid=${encodeURIComponent(appId)}&output=json&results=50&sort=dist`
      + `&lat=${c.lat}&lon=${c.lon}&dist=10`;
    const data = await fetchJson(url, { timeoutMs: 12000 });
    for (const f of (data?.Feature || [])) {
      const [lon, lat] = String(f.Geometry?.Coordinates || '').split(',').map(Number);
      if (!Number.isFinite(lat) || !Number.isFinite(lon)) continue;
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [lon, lat] },
        properties: {
          poi_id: f.Id || null,
          name: f.Name || null,
          category: f.Property?.Genre?.[0]?.Name || null,
          address: f.Property?.Address || null,
          tel: f.Property?.Tel1 || null,
          source: 'yahoo_api',
        },
      });
    }
  }
  return result(features, features.length ? 'yahoo_api' : 'yahoo_unavailable');
}
