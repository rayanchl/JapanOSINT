/**
 * Shodan Japan IoT-device collector (shodan-japan).
 *
 * Live: Shodan host search `country:JP` when SHODAN_API_KEY is set. There
 * is no key-free fallback; without a key (or on failure) the collector
 * returns an honest empty result — never fabricated devices. The
 * missing-key state surfaces via the apiCredentials map.
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

function result(features, source) {
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Internet-exposed devices in Japan via Shodan (requires SHODAN_API_KEY)',
    },
  };
}

export default async function collectShodanJapan() {
  const key = envFor('SHODAN_API_KEY');
  if (!key) return result([], 'shodan_no_key');

  const url = `https://api.shodan.io/shodan/host/search?key=${encodeURIComponent(key)}`
    + `&query=${encodeURIComponent('country:JP')}`;
  const data = await fetchJson(url, { timeoutMs: 20000 });
  const matches = data?.matches;
  if (!Array.isArray(matches)) return result([], 'shodan_unavailable');

  const features = matches
    .filter((m) => m?.location?.longitude != null && m?.location?.latitude != null)
    .map((m) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [+m.location.longitude, +m.location.latitude] },
      properties: {
        id: `${m.ip_str}:${m.port}`,
        ip: m.ip_str,
        port: m.port,
        product: m.product || m._shodan?.module || null,
        transport: m.transport || null,
        org: m.org || null,
        city: m.location?.city || null,
        hostnames: m.hostnames || [],
        source: 'shodan_api',
      },
    }));
  return result(features, 'shodan_api');
}
