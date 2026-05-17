/**
 * Navitime API collector (navitime-api).
 *
 * Live: Navitime RapidAPI transport_node search around major metros when
 * NAVITIME_API_KEY (RapidAPI key) is set. No key-free fallback; without a
 * key (or on failure) returns an honest empty result — never fabricated
 * nodes. Missing key surfaces via the apiCredentials map.
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

const METROS = ['東京', '大阪', '名古屋', '福岡', '札幌'];

function result(features, source) {
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Navitime transport-node search (requires NAVITIME_API_KEY)',
    },
  };
}

export default async function collectNavitimeApi() {
  const key = envFor('NAVITIME_API_KEY');
  if (!key) return result([], 'navitime_no_key');

  const features = [];
  for (const word of METROS) {
    const url = 'https://navitime-transport.p.rapidapi.com/transport_node'
      + `?word=${encodeURIComponent(word)}&datum=wgs84&coord_unit=degree`;
    const data = await fetchJson(url, {
      timeoutMs: 12000,
      headers: {
        'X-RapidAPI-Key': key,
        'X-RapidAPI-Host': 'navitime-transport.p.rapidapi.com',
      },
    });
    for (const it of (data?.items || [])) {
      const c = it.coord;
      if (!c || c.lon == null || c.lat == null) continue;
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [+c.lon, +c.lat] },
        properties: {
          node_id: it.id || null,
          name: it.name || null,
          ruby: it.ruby || null,
          types: it.types || null,
          source: 'navitime_api',
        },
      });
    }
  }
  return result(features, features.length ? 'navitime_api' : 'navitime_unavailable');
}
