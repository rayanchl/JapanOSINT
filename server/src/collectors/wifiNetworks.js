/**
 * WiFi Networks Collector (Wigle.net style)
 * Maps wireless networks discovered across Japan:
 * - Open/unencrypted networks
 * - WEP networks (weak encryption)
 * - WPA/WPA2/WPA3 networks
 * - Enterprise networks
 * - Free public WiFi hotspots (Japan Free Wi-Fi, etc.)
 * Uses Wigle API when key available, seed data otherwise
 */

import { fetchOverpass } from './_liveHelpers.js';

const WIGLE_API_KEY = process.env.WIGLE_API_KEY || '';

const WIFI_ZONES = [
  // Major transit hubs (lots of WiFi)
  { area: '東京駅', lat: 35.6812, lon: 139.7671, density: 150, type: 'transit' },
  { area: '新宿駅', lat: 35.6896, lon: 139.7006, density: 180, type: 'transit' },
  { area: '渋谷駅', lat: 35.6580, lon: 139.7016, density: 160, type: 'transit' },
  { area: '池袋駅', lat: 35.7295, lon: 139.7109, density: 140, type: 'transit' },
  { area: '品川駅', lat: 35.6284, lon: 139.7387, density: 120, type: 'transit' },
  { area: '大阪駅/梅田', lat: 34.7024, lon: 135.4959, density: 150, type: 'transit' },
  { area: '難波駅', lat: 34.6627, lon: 135.5010, density: 130, type: 'transit' },
  { area: '名古屋駅', lat: 35.1709, lon: 136.8815, density: 110, type: 'transit' },
  { area: '博多駅', lat: 33.5897, lon: 130.4207, density: 100, type: 'transit' },
  { area: '札幌駅', lat: 43.0687, lon: 141.3508, density: 90, type: 'transit' },
  // Commercial/shopping areas
  { area: '秋葉原電気街', lat: 35.6984, lon: 139.7731, density: 200, type: 'commercial' },
  { area: '銀座', lat: 35.6717, lon: 139.7637, density: 130, type: 'commercial' },
  { area: '原宿', lat: 35.6702, lon: 139.7035, density: 120, type: 'commercial' },
  { area: '心斎橋', lat: 34.6748, lon: 135.5012, density: 110, type: 'commercial' },
  { area: '天神', lat: 33.5898, lon: 130.3987, density: 90, type: 'commercial' },
  // Business districts
  { area: '丸の内', lat: 35.6825, lon: 139.7650, density: 140, type: 'business' },
  { area: '六本木', lat: 35.6605, lon: 139.7292, density: 100, type: 'business' },
  { area: '大手町', lat: 35.6867, lon: 139.7660, density: 130, type: 'business' },
  { area: '御堂筋', lat: 34.6900, lon: 135.5000, density: 100, type: 'business' },
  // Residential areas
  { area: '世田谷', lat: 35.6461, lon: 139.6531, density: 80, type: 'residential' },
  { area: '練馬', lat: 35.7356, lon: 139.6518, density: 60, type: 'residential' },
  { area: '吹田', lat: 34.7611, lon: 135.5174, density: 50, type: 'residential' },
  { area: '藤沢', lat: 35.3389, lon: 139.4900, density: 40, type: 'residential' },
  // Tourist areas (free WiFi spots)
  { area: '浅草', lat: 35.7114, lon: 139.7966, density: 70, type: 'tourist' },
  { area: '京都 祇園', lat: 34.9986, lon: 135.7747, density: 60, type: 'tourist' },
  { area: '成田空港', lat: 35.7720, lon: 140.3929, density: 100, type: 'tourist' },
  { area: '関西空港', lat: 34.4320, lon: 135.2302, density: 90, type: 'tourist' },
];

const ENCRYPTION_TYPES = ['open', 'WEP', 'WPA', 'WPA2', 'WPA3', 'WPA2-Enterprise'];
const SSID_PATTERNS = {
  transit: ['JR-EAST_FREE_Wi-Fi', 'Metro_Free_Wi-Fi', 'Shinkansen_Free_Wi-Fi', 'toei_bus_Free_Wi-Fi', 'docomo Wi-Fi', 'au Wi-Fi SPOT'],
  commercial: ['FREE_Wi-Fi_PASSPORT', 'LAWSON_Free_Wi-Fi', '7SPOT', 'FamilyMart_Wi-Fi', 'Starbucks_Wi-Fi', 'at_STARBUCKS_Wi2'],
  business: ['eduroam', 'OFFICE-NET-5G', 'Corp-WiFi', 'NTT-SPOT', 'Wi2premium_club'],
  residential: ['Buffalo-G-XXXX', 'aterm-XXXXX-g', 'IODATA-XXXXX', 'elecom-XXXXX', 'NEC-ATERM'],
  tourist: ['JAPAN-FREE-WIFI', 'TRAVEL_JAPAN_WiFi', 'Visit_Japan_Wi-Fi', 'FREE_Wi-Fi_NARITA', 'KANSAI-FREE-WIFI'],
};

function seededRandom(seed) {
  let x = Math.sin(seed * 9301 + 49297) * 233280;
  return x - Math.floor(x);
}

function generateSeedData() {
  const features = [];
  let idx = 0;
  const now = new Date();

  for (const zone of WIFI_ZONES) {
    const count = Math.min(20, Math.max(5, Math.round(zone.density / 10)));
    const ssidPool = SSID_PATTERNS[zone.type] || SSID_PATTERNS.residential;

    for (let j = 0; j < count; j++) {
      idx++;
      const r1 = seededRandom(idx * 3);
      const r2 = seededRandom(idx * 7);
      const r3 = seededRandom(idx * 11);

      const lat = zone.lat + (r1 - 0.5) * 0.005;
      const lon = zone.lon + (r2 - 0.5) * 0.006;

      const ssid = ssidPool[Math.floor(r3 * ssidPool.length)]
        .replace(/XXXX/g, String(Math.floor(seededRandom(idx * 13) * 9999)).padStart(4, '0'))
        .replace(/XXXXX/g, String(Math.floor(seededRandom(idx * 17) * 99999)).padStart(5, '0'));

      const encIdx = Math.floor(seededRandom(idx * 19) * ENCRYPTION_TYPES.length);
      const encryption = ENCRYPTION_TYPES[encIdx];
      const channel = Math.floor(seededRandom(idx * 23) * 13) + 1;
      const signal = -30 - Math.floor(seededRandom(idx * 29) * 60);
      const is5ghz = seededRandom(idx * 31) > 0.5;

      const mac = Array.from({ length: 6 }, (_, k) =>
        Math.floor(seededRandom(idx * 37 + k) * 256).toString(16).padStart(2, '0')
      ).join(':');

      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [lon, lat] },
        properties: {
          id: `WIFI_${String(idx).padStart(5, '0')}`,
          ssid,
          bssid: mac,
          encryption,
          channel,
          frequency: is5ghz ? '5GHz' : '2.4GHz',
          signal_dbm: signal,
          area: zone.area,
          zone_type: zone.type,
          is_open: encryption === 'open',
          is_free_wifi: ssid.includes('FREE') || ssid.includes('Free') || ssid.includes('SPOT'),
          vendor: ['Buffalo', 'NEC', 'I-O DATA', 'Elecom', 'Cisco', 'Ubiquiti', 'TP-Link'][Math.floor(seededRandom(idx * 41) * 7)],
          last_seen: new Date(now - Math.floor(seededRandom(idx * 43) * 48) * 3600000).toISOString(),
          source: 'wifi_scan',
        },
      });
    }
  }
  return features;
}

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

export default async function collectWifiNetworks() {
  let features = await tryWigleAPI();
  let liveSource = 'wigle_api';
  if (!features || features.length === 0) {
    features = await tryOSMWifiHotspots();
    liveSource = 'osm_overpass';
  }
  const live = !!(features && features.length > 0);
  if (!live) {
    features = generateSeedData();
    liveSource = 'wifi_seed';
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'wifi_scan',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: liveSource,
      description: 'WiFi networks in Japan - open, encrypted, free hotspots, enterprise networks',
    },
    metadata: {},
  };
}
