/**
 * WiGLE WiFi Networks collector.
 * Primary: WiGLE search API (requires WIGLE_API_KEY).
 * Silent fallback when WIGLE returns nothing or the key is unset:
 *   - OpenStreetMap Overpass (public hotspots tagged internet_access=wlan)
 * The fallback needs no key and never surfaces in the source UI — it only
 * exists so the layer doesn't go empty when WIGLE is unavailable. The
 * NTT-BP and FREESPOT directory listings have no real coordinates and now
 * live in the intel pipeline (wifiHotspotsJcfw, wifiHotspotsFreespot).
 */

import { fetchOverpass } from './_liveHelpers.js';

const WIGLE_API_KEY = process.env.WIGLE_API_KEY || '';

async function tryWigleAPI() {
  if (!WIGLE_API_KEY) return null;
  try {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 10000);
    const res = await fetch(
      'https://api.wigle.net/api/v2/network/search?country=JP&latrange1=30&latrange2=45&longrange1=129&longrange2=146&resultsPerPage=100',
      {
        headers: { Authorization: `Basic ${WIGLE_API_KEY}` },
        signal: controller.signal,
      }
    );
    clearTimeout(timeout);
    if (!res.ok) return null;
    const data = await res.json();
    if (!data.results) return null;
    return data.results.map((net, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [net.trilong, net.trilat] },
      properties: {
        id: `WIGLE_${i}`,
        ssid: net.ssid,
        bssid: net.netid,
        encryption: net.encryption,
        channel: net.channel,
        last_seen: net.lastupdt,
        source: 'wigle_api',
      },
    }));
  } catch {
    return null;
  }
}

async function tryOSMWifiHotspots() {
  return fetchOverpass(
    'node["internet_access"="wlan"](area.jp);node["amenity"="internet_cafe"](area.jp);node["wifi"="free"](area.jp);node["internet_access"="yes"]["internet_access:fee"="no"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        id: `OSM_${el.id}`,
        ssid: el.tags?.['internet_access:ssid'] || el.tags?.name || `Wi-Fi ${i + 1}`,
        bssid: null,
        encryption: el.tags?.['internet_access:fee'] === 'no' ? 'open' : 'unknown',
        is_open: el.tags?.['internet_access:fee'] === 'no' || el.tags?.wifi === 'free',
        is_free_wifi: el.tags?.['internet_access:fee'] === 'no' || el.tags?.wifi === 'free',
        operator: el.tags?.operator || el.tags?.brand || null,
        venue: el.tags?.amenity || el.tags?.shop || 'unknown',
        zone_type: 'public_hotspot',
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

export default async function collectWifiNetworksWigle() {
  const [wigle, osm] = await Promise.allSettled([
    tryWigleAPI(),
    tryOSMWifiHotspots(),
  ]);

  const features = [];
  const liveSources = [];
  const sourceNames = ['wigle_api', 'osm_overpass'];
  for (const [i, r] of [wigle, osm].entries()) {
    if (r.status === 'fulfilled' && Array.isArray(r.value) && r.value.length > 0) {
      features.push(...r.value);
      liveSources.push(sourceNames[i]);
    }
  }

  const live = features.length > 0;
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'wifi_networks_wigle',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? liveSources.join(',') : null,
      description: 'WiGLE WiFi network search across Japan (OSM Overpass fallback when WIGLE_API_KEY unset)',
    },
  };
}
