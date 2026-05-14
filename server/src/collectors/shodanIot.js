/**
 * Shodan IoT Device Collector
 *
 * Real data only: hits the authenticated Shodan Search API
 * (https://api.shodan.io/shodan/host/search) for Japan-country hosts.
 * Requires SHODAN_API_KEY. Returns an empty FeatureCollection when the
 * key is missing or the API call fails — no seed, no OSM fallback,
 * no InternetDB gateway probing.
 */

import { getEnv } from '../utils/credentials.js';

async function tryShodanAPI() {
  // Read at call-time so iOS-set keys (which mutate process.env via
  // apiKeysStore.setKey) AND tenant BYOK overrides take effect without
  // a server restart.
  const key = getEnv(null, 'SHODAN_API_KEY') || '';
  if (!key) return null;
  try {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 10000);
    const res = await fetch(
      `https://api.shodan.io/shodan/host/search?key=${key}&query=country:JP&facets=port`,
      { signal: controller.signal }
    );
    clearTimeout(timeout);
    if (!res.ok) return null;
    const data = await res.json();
    if (!Array.isArray(data.matches)) return null;
    return data.matches
      .filter((m) => Number.isFinite(m.location?.longitude) && Number.isFinite(m.location?.latitude))
      .map((m, i) => ({
        type: 'Feature',
        geometry: {
          type: 'Point',
          coordinates: [m.location.longitude, m.location.latitude],
        },
        properties: {
          id: `SHODAN_LIVE_${i}`,
          ip: m.ip_str,
          port: m.port,
          product: m.product || 'unknown',
          device_type: m.devicetype || 'unknown',
          os: m.os || 'unknown',
          banner: (m.data || '').substring(0, 200),
          city: m.location?.city || '',
          last_seen: m.timestamp,
          source: 'shodan_api',
        },
      }));
  } catch {
    return null;
  }
}

export default async function collectShodanIot() {
  const features = await tryShodanAPI();
  const list = features || [];
  const live = list.length > 0;

  return {
    type: 'FeatureCollection',
    features: list,
    _meta: {
      source: 'shodan',
      fetchedAt: new Date().toISOString(),
      recordCount: list.length,
      live,
      live_source: live ? 'shodan_api' : null,
      description: 'Shodan IoT device scan — Japan hosts via authenticated Search API (live only)',
    },
  };
}
