/**
 * Cell Tower / Mobile Network Collector
 * Maps mobile network infrastructure across Japan:
 * - NTT Docomo, au/KDDI, SoftBank, Rakuten Mobile
 * - 4G/5G base station locations
 * - Major hub/aggregation sites
 * Uses OpenCellID-style data when available
 */

import { fetchOverpass } from './_liveHelpers.js';

const OPENCELLID_KEY = process.env.OPENCELLID_KEY || '';

// Major urban cell tower density zones
const TOWER_ZONES = [
  // Tokyo high density
  { area: '渋谷', lat: 35.6595, lon: 139.7004, density: 50 },
  { area: '新宿', lat: 35.6938, lon: 139.7036, density: 55 },
  { area: '池袋', lat: 35.7295, lon: 139.7109, density: 45 },
  { area: '品川', lat: 35.6284, lon: 139.7387, density: 40 },
  { area: '東京駅周辺', lat: 35.6812, lon: 139.7671, density: 60 },
  { area: '六本木', lat: 35.6605, lon: 139.7292, density: 35 },
  { area: '上野', lat: 35.7146, lon: 139.7732, density: 35 },
  { area: '秋葉原', lat: 35.6984, lon: 139.7731, density: 40 },
  { area: '銀座', lat: 35.6717, lon: 139.7637, density: 45 },
  { area: '表参道', lat: 35.6654, lon: 139.7121, density: 30 },
  { area: '吉祥寺', lat: 35.7030, lon: 139.5795, density: 25 },
  { area: '町田', lat: 35.5424, lon: 139.4467, density: 25 },
  { area: '立川', lat: 35.6980, lon: 139.4143, density: 25 },
  // Kanagawa
  { area: '横浜', lat: 35.4660, lon: 139.6223, density: 50 },
  { area: '川崎', lat: 35.5309, lon: 139.7030, density: 40 },
  { area: '藤沢', lat: 35.3389, lon: 139.4900, density: 25 },
  { area: '湘南台', lat: 35.4000, lon: 139.4700, density: 20 },
  // Saitama / Chiba
  { area: '大宮', lat: 35.9064, lon: 139.6237, density: 35 },
  { area: '浦和', lat: 35.8617, lon: 139.6455, density: 25 },
  { area: '船橋', lat: 35.6946, lon: 139.9828, density: 30 },
  { area: '柏', lat: 35.8617, lon: 139.9700, density: 25 },
  // Osaka
  { area: '梅田', lat: 34.7055, lon: 135.4983, density: 55 },
  { area: '難波', lat: 34.6627, lon: 135.5010, density: 50 },
  { area: '心斎橋', lat: 34.6748, lon: 135.5012, density: 40 },
  { area: '天王寺', lat: 34.6468, lon: 135.5135, density: 35 },
  { area: '京橋', lat: 34.6960, lon: 135.5340, density: 30 },
  { area: '堺', lat: 34.5733, lon: 135.4832, density: 30 },
  // Kyoto
  { area: '京都駅周辺', lat: 34.9856, lon: 135.7581, density: 35 },
  { area: '河原町', lat: 35.0040, lon: 135.7693, density: 30 },
  // Nagoya
  { area: '名古屋駅', lat: 35.1709, lon: 136.8815, density: 50 },
  { area: '栄', lat: 35.1692, lon: 136.9084, density: 40 },
  { area: '金山', lat: 35.1440, lon: 136.9002, density: 30 },
  // Other cities
  { area: '札幌', lat: 43.0618, lon: 141.3545, density: 45 },
  { area: '仙台', lat: 38.2682, lon: 140.8694, density: 35 },
  { area: '広島', lat: 34.3853, lon: 132.4553, density: 35 },
  { area: '福岡 博多', lat: 33.5920, lon: 130.4080, density: 40 },
  { area: '福岡 天神', lat: 33.5898, lon: 130.3987, density: 40 },
  { area: '神戸 三宮', lat: 34.6951, lon: 135.1979, density: 35 },
  { area: '北九州', lat: 33.8834, lon: 130.8752, density: 25 },
  { area: '熊本', lat: 32.8032, lon: 130.7079, density: 25 },
  { area: '那覇', lat: 26.3344, lon: 127.6809, density: 25 },
  { area: '岡山', lat: 34.6551, lon: 133.9195, density: 25 },
  { area: '金沢', lat: 36.5780, lon: 136.6480, density: 20 },
  { area: '富山', lat: 36.7014, lon: 137.2131, density: 18 },
  { area: '新潟', lat: 37.9161, lon: 139.0364, density: 22 },
  { area: '長野', lat: 36.6433, lon: 138.1886, density: 18 },
  { area: '宇都宮', lat: 36.5594, lon: 139.8981, density: 20 },
  { area: '高松', lat: 34.3401, lon: 134.0434, density: 18 },
  { area: '松山', lat: 33.8395, lon: 132.7657, density: 18 },
  { area: '高知', lat: 33.5667, lon: 133.5436, density: 15 },
  { area: '徳島', lat: 34.0744, lon: 134.5517, density: 15 },
  { area: '大分', lat: 33.2328, lon: 131.6067, density: 18 },
  { area: '宮崎', lat: 31.9164, lon: 131.4272, density: 15 },
  { area: '鹿児島', lat: 31.5966, lon: 130.5571, density: 20 },
  { area: '長崎', lat: 32.7503, lon: 129.8777, density: 20 },
];

const CARRIERS = [
  { name: 'NTT Docomo', mcc: '440', mnc: '10', color: 'red' },
  { name: 'au by KDDI', mcc: '440', mnc: '50', color: 'orange' },
  { name: 'SoftBank', mcc: '440', mnc: '20', color: 'silver' },
  { name: 'Rakuten Mobile', mcc: '440', mnc: '11', color: 'crimson' },
];

const RADIO_TYPES = ['LTE', 'NR-5G', 'LTE+5G', 'LTE-Advanced'];

function seededRandom(seed) {
  let x = Math.sin(seed * 9301 + 49297) * 233280;
  return x - Math.floor(x);
}

function generateSeedData() {
  const features = [];
  let idx = 0;
  const now = new Date();

  for (const zone of TOWER_ZONES) {
    const count = Math.min(15, Math.max(3, Math.round(zone.density / 4)));
    for (let j = 0; j < count; j++) {
      idx++;
      const r1 = seededRandom(idx * 3);
      const r2 = seededRandom(idx * 7);
      const r3 = seededRandom(idx * 11);

      const lat = zone.lat + (r1 - 0.5) * 0.01;
      const lon = zone.lon + (r2 - 0.5) * 0.012;

      const carrier = CARRIERS[Math.floor(r3 * CARRIERS.length)];
      const radioType = RADIO_TYPES[Math.floor(seededRandom(idx * 13) * RADIO_TYPES.length)];
      const cellId = Math.floor(seededRandom(idx * 17) * 999999);
      const lac = Math.floor(seededRandom(idx * 19) * 65535);
      const range = Math.floor(100 + seededRandom(idx * 23) * 2000);
      const samples = Math.floor(seededRandom(idx * 29) * 5000) + 10;

      const bands = radioType.includes('5G')
        ? ['n28 (700MHz)', 'n3 (1800MHz)', 'n78 (3.5GHz)', 'n79 (4.5GHz)', 'n257 (28GHz)']
        : ['B1 (2100)', 'B3 (1800)', 'B8 (900)', 'B11 (1500)', 'B18 (800)', 'B19 (800)', 'B26 (850)', 'B28 (700)'];
      const band = bands[Math.floor(seededRandom(idx * 31) * bands.length)];

      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [lon, lat] },
        properties: {
          tower_id: `CELL_${String(idx).padStart(5, '0')}`,
          carrier: carrier.name,
          mcc: carrier.mcc,
          mnc: carrier.mnc,
          cell_id: cellId,
          lac,
          radio: radioType,
          band,
          area: zone.area,
          range_m: range,
          samples,
          last_seen: new Date(now - Math.floor(seededRandom(idx * 37) * 30) * 86400000).toISOString(),
          source: 'cell_towers',
        },
      });
    }
  }
  return features;
}

async function tryOpenCellID() {
  if (!OPENCELLID_KEY) return null;
  try {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 10000);
    const res = await fetch(
      `https://opencellid.org/cell/getInArea?key=${OPENCELLID_KEY}&BBOX=24,122,46,154&format=json&limit=200&mcc=440`,
      { signal: controller.signal }
    );
    clearTimeout(timeout);
    if (!res.ok) return null;
    const data = await res.json();
    if (!data.cells) return null;
    return data.cells.map((c, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [c.lon, c.lat] },
      properties: {
        tower_id: `OPENCELL_${i}`,
        mcc: c.mcc,
        mnc: c.mnc,
        cell_id: c.cellid,
        lac: c.lac,
        radio: c.radio,
        range_m: c.range,
        samples: c.samples,
        source: 'opencellid_api',
      },
    }));
  } catch {
    return null;
  }
}

async function tryOSMCommTowers() {
  return fetchOverpass(
    'node["tower:type"="communication"](area.jp);way["tower:type"="communication"](area.jp);node["man_made"="mast"]["tower:type"~"communication|cellular"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        tower_id: `OSM_${el.id}`,
        name: el.tags?.name || `Comm tower ${i + 1}`,
        operator: el.tags?.operator || 'unknown',
        carrier: el.tags?.operator || el.tags?.['operator:short'] || 'unknown',
        tower_type: el.tags?.['tower:type'] || 'communication',
        height_m: el.tags?.height ? parseFloat(el.tags.height) : null,
        radio: el.tags?.['communication:mobile_phone'] ? 'mobile' : 'communication',
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

export default async function collectCellTowers() {
  let features = await tryOpenCellID();
  let source = 'opencellid_api';
  if (!features || features.length === 0) {
    features = await tryOSMCommTowers();
    source = 'osm_overpass';
  }
  const live = !!(features && features.length > 0);
  if (!live) {
    features = generateSeedData();
    source = 'cell_towers_seed';
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'cell_towers',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: source,
      description: 'Japan mobile network infrastructure - 4G/5G cell towers (Docomo, au, SoftBank, Rakuten)',
    },
    metadata: {},
  };
}
