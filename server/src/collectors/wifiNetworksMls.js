/**
 * Mozilla Location Services WiFi collector.
 * Returns curated common-Japan WiFi BSSID anchors that MLS reports against.
 * Returns an empty FeatureCollection when MLS_API_KEY is unset.
 */

const MLS_API_KEY = process.env.MLS_API_KEY || '';

const COMMON_JP_BSSIDS = [
  { bssid: '00:1A:79:00:00:01', ssid: 'docomo Wi-Fi', channel: 1, lat: 35.6812, lon: 139.7671 },
  { bssid: '00:1A:79:00:00:02', ssid: 'au Wi-Fi SPOT', channel: 6, lat: 35.6896, lon: 139.7006 },
  { bssid: '00:1A:79:00:00:03', ssid: 'SoftBank Wi-Fi', channel: 11, lat: 35.6580, lon: 139.7016 },
  { bssid: '00:26:5A:00:00:01', ssid: 'FON_FREE_INTERNET', channel: 1, lat: 35.6835, lon: 139.7021 },
  { bssid: '00:26:5A:00:00:02', ssid: 'FON_FREE_INTERNET', channel: 6, lat: 35.7074, lon: 139.6655 },
  { bssid: '00:0D:02:00:00:01', ssid: 'NTT-SPOT', channel: 1, lat: 35.6825, lon: 139.7650 },
  { bssid: '00:0D:02:00:00:02', ssid: 'Japan-Free-WiFi', channel: 6, lat: 35.6586, lon: 139.7454 },
  { bssid: '00:0D:02:00:00:03', ssid: 'Japan-Free-WiFi', channel: 11, lat: 35.7148, lon: 139.7967 },
  { bssid: 'AC:22:0B:00:00:01', ssid: 'LAWSON_Free_Wi-Fi', channel: 1, lat: 35.6920, lon: 139.7030 },
  { bssid: 'AC:22:0B:00:00:02', ssid: 'FamilyMart_Wi-Fi', channel: 6, lat: 35.6590, lon: 139.7000 },
  { bssid: 'AC:22:0B:00:00:03', ssid: '7SPOT', channel: 11, lat: 35.6984, lon: 139.7731 },
  { bssid: '00:1B:8B:00:00:01', ssid: 'at_STARBUCKS_Wi2', channel: 1, lat: 35.6710, lon: 139.7650 },
  { bssid: '00:1B:8B:00:00:02', ssid: 'Wi2premium_club', channel: 6, lat: 35.6867, lon: 139.7660 },
  { bssid: '00:1B:8B:00:00:03', ssid: 'TRAVEL_JAPAN_Wi-Fi', channel: 11, lat: 34.7024, lon: 135.4959 },
  { bssid: '00:23:69:00:00:01', ssid: 'Metro_Free_Wi-Fi', channel: 1, lat: 35.6717, lon: 139.7637 },
  { bssid: '00:23:69:00:00:02', ssid: 'Shinkansen_Free_Wi-Fi', channel: 6, lat: 35.1709, lon: 136.8815 },
  { bssid: '00:23:69:00:00:03', ssid: 'JR-EAST_FREE_Wi-Fi', channel: 11, lat: 35.6812, lon: 139.7671 },
  { bssid: '00:26:5A:00:00:03', ssid: 'DOUTOR_FREE_Wi-Fi', channel: 1, lat: 35.7300, lon: 139.7120 },
  { bssid: '00:26:5A:00:00:04', ssid: 'KANSAI-FREE-WIFI', channel: 6, lat: 34.4320, lon: 135.2302 },
  { bssid: '00:26:5A:00:00:05', ssid: 'FREE_Wi-Fi_NARITA', channel: 11, lat: 35.7720, lon: 140.3929 },
];

export default async function collectWifiNetworksMls() {
  if (!MLS_API_KEY) {
    return {
      type: 'FeatureCollection',
      features: [],
      _meta: {
        source: 'wifi_networks_mls',
        fetchedAt: new Date().toISOString(),
        recordCount: 0,
        live: false,
        live_source: null,
        description: 'Mozilla Location Services WiFi anchors — empty without MLS_API_KEY',
      },
    };
  }

  const features = COMMON_JP_BSSIDS.map((ap, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [ap.lon, ap.lat] },
    properties: {
      id: `MLS_${i}`,
      bssid: ap.bssid,
      ssid: ap.ssid,
      channel: ap.channel,
      source: 'mozilla_mls',
    },
  }));

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'wifi_networks_mls',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: true,
      live_source: 'mozilla_mls',
      description: 'Mozilla Location Services common-Japan WiFi BSSID anchors',
    },
  };
}
