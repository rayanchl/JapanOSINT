/**
 * HERE Maps Japan collector (here-japan).
 *
 * Live: HERE Browse API (POIs around major metros) when HERE_API_KEY is
 * set. No key-free fallback; without a key (or on failure) returns an
 * honest empty result — never fabricated POIs. Missing key surfaces via
 * the apiCredentials map.
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

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
      description: 'HERE Browse POIs across Japan metros (requires HERE_API_KEY)',
    },
  };
}

export default async function collectHereJapan() {
  const key = envFor('HERE_API_KEY');
  if (!key) return result([], 'here_no_key');

  const features = [];
  for (const c of SCAN) {
    const url = 'https://browse.search.hereapi.com/v1/browse'
      + `?at=${c.lat},${c.lon}&limit=100&apiKey=${encodeURIComponent(key)}`;
    const data = await fetchJson(url, { timeoutMs: 12000 });
    for (const it of (data?.items || [])) {
      const pos = it.position;
      if (!pos || pos.lng == null || pos.lat == null) continue;
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [+pos.lng, +pos.lat] },
        properties: {
          poi_id: it.id || null,
          name: it.title || null,
          category: it.categories?.[0]?.name || null,
          address: it.address?.label || null,
          source: 'here_api',
        },
      });
    }
  }
  return result(features, features.length ? 'here_api' : 'here_unavailable');
}
