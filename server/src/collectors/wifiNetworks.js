/**
 * WiFi Networks layer composite.
 * Fans out to the three standalone WiFi-Networks sub-collectors and merges
 * their FeatureCollections into one. Each sub-collector owns exactly one
 * API key (WIGLE / SHODAN / MLS), and is registered as its own source in
 * sourceRegistry.js so the dashboard can show three rows. This composite
 * is invisible to the source UI — it only exists to keep the existing
 * /api/data/wifi-networks layer endpoint and "Refresh collector" button
 * returning a single merged GeoJSON.
 */

import collectWifiNetworksWigle from './wifiNetworksWigle.js';
import collectWifiNetworksShodan from './wifiNetworksShodan.js';
import collectWifiNetworksMls from './wifiNetworksMls.js';

export default async function collectWifiNetworks() {
  const [wigle, shodan, mls] = await Promise.allSettled([
    collectWifiNetworksWigle(),
    collectWifiNetworksShodan(),
    collectWifiNetworksMls(),
  ]);

  const features = [];
  const liveSources = [];
  for (const r of [wigle, shodan, mls]) {
    if (r.status !== 'fulfilled' || !r.value) continue;
    const fc = r.value;
    if (Array.isArray(fc.features)) features.push(...fc.features);
    if (fc._meta?.live_source) liveSources.push(fc._meta.live_source);
  }

  const live = features.length > 0;
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'wifi_networks',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? liveSources.join(',') : null,
      description: 'WiFi networks across Japan — merged WiGLE + Shodan WiFi + MLS sources',
    },
  };
}
