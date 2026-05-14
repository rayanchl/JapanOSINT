/**
 * Shodan WiFi Devices collector.
 * Targets Shodan hosts in Japan whose banner mentions wifi/wireless on
 * common admin ports. Distinct from the general `shodan-iot` collector
 * (which queries all of country:JP without a WiFi keyword).
 * Returns an empty FeatureCollection when SHODAN_API_KEY is unset.
 */

import { fetchJson } from './_liveHelpers.js';

async function tryShodanWifiAPs() {
  // Read at call-time so iOS-set keys (which mutate process.env via
  // apiKeysStore.setKey) take effect without a server restart.
  const key = process.env.SHODAN_API_KEY || '';
  if (!key) return [];
  try {
    const query = encodeURIComponent('country:JP wifi OR "wireless" port:80,8080');
    const data = await fetchJson(
      `https://api.shodan.io/shodan/host/search?key=${key}&query=${query}`
    );
    if (!data || !data.matches) return [];
    return data.matches.slice(0, 50).map((m, i) => ({
      type: 'Feature',
      geometry: {
        type: 'Point',
        coordinates: [
          m.location?.longitude || 139.7671,
          m.location?.latitude || 35.6812,
        ],
      },
      properties: {
        id: `SHODAN_${i}`,
        ip: m.ip_str,
        port: m.port,
        org: m.org,
        isp: m.isp,
        product: m.product,
        city: m.location?.city,
        source: 'shodan_wifi',
      },
    }));
  } catch {
    return [];
  }
}

export default async function collectWifiNetworksShodan() {
  const features = await tryShodanWifiAPs();
  const live = features.length > 0;
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'wifi_networks_shodan',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'shodan_wifi' : null,
      description: 'Shodan country:JP wifi/wireless device hits (live only — empty without SHODAN_API_KEY)',
    },
  };
}
