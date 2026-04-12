/**
 * RESAS Industry Collector
 * Fetches industry composition (manufacturing/services/agriculture) by city from RESAS.
 * Falls back to a curated seed of major industrial cities.
 */

import { fetchOverpass } from './_liveHelpers.js';

const RESAS_KEY = process.env.RESAS_API_KEY || '';
const RESAS_URL = 'https://opendata.resas-portal.go.jp/api/v1/industry/power/forArea';

const SEED_INDUSTRY = [
  // Manufacturing hubs
  { name: '豊田市 (自動車)', lat: 35.0833, lon: 137.1561, primary: 'manufacturing', sub: 'automotive', employees: 250000, revenue_jpy: 17000000000000, prefecture: '愛知県' },
  { name: '横浜市 (機械/化学/造船)', lat: 35.4437, lon: 139.6380, primary: 'manufacturing', sub: 'mixed', employees: 220000, revenue_jpy: 4500000000000, prefecture: '神奈川県' },
  { name: '川崎市 (重工業/化学)', lat: 35.5311, lon: 139.7036, primary: 'manufacturing', sub: 'heavy_chemical', employees: 165000, revenue_jpy: 4200000000000, prefecture: '神奈川県' },
  { name: '北九州市 (鉄鋼)', lat: 33.8836, lon: 130.8814, primary: 'manufacturing', sub: 'steel', employees: 95000, revenue_jpy: 2300000000000, prefecture: '福岡県' },
  { name: '室蘭市 (鉄鋼)', lat: 42.3158, lon: 140.9742, primary: 'manufacturing', sub: 'steel', employees: 14000, revenue_jpy: 600000000000, prefecture: '北海道' },
  { name: '倉敷市 (石油化学)', lat: 34.5856, lon: 133.7700, primary: 'manufacturing', sub: 'petrochemical', employees: 80000, revenue_jpy: 4000000000000, prefecture: '岡山県' },
  { name: '四日市市 (石油化学)', lat: 34.9650, lon: 136.6244, primary: 'manufacturing', sub: 'petrochemical', employees: 60000, revenue_jpy: 3200000000000, prefecture: '三重県' },
  { name: '市原市 (石油化学)', lat: 35.4983, lon: 140.1153, primary: 'manufacturing', sub: 'petrochemical', employees: 45000, revenue_jpy: 3000000000000, prefecture: '千葉県' },
  { name: '日立市 (電機)', lat: 36.5994, lon: 140.6517, primary: 'manufacturing', sub: 'electronics', employees: 70000, revenue_jpy: 2500000000000, prefecture: '茨城県' },
  { name: '浜松市 (機械/楽器/オートバイ)', lat: 34.7108, lon: 137.7261, primary: 'manufacturing', sub: 'mixed', employees: 150000, revenue_jpy: 4000000000000, prefecture: '静岡県' },
  { name: '四国中央市 (製紙)', lat: 33.9858, lon: 133.5494, primary: 'manufacturing', sub: 'paper', employees: 12000, revenue_jpy: 600000000000, prefecture: '愛媛県' },
  { name: '苫小牧市 (製紙/石油)', lat: 42.6342, lon: 141.6047, primary: 'manufacturing', sub: 'mixed', employees: 30000, revenue_jpy: 1500000000000, prefecture: '北海道' },

  // Services / Finance hubs
  { name: '東京23区 (金融/サービス)', lat: 35.6896, lon: 139.6917, primary: 'services', sub: 'finance', employees: 4500000, revenue_jpy: 50000000000000, prefecture: '東京都' },
  { name: '大阪市 (商業/卸売)', lat: 34.6864, lon: 135.5197, primary: 'services', sub: 'commerce', employees: 1900000, revenue_jpy: 18000000000000, prefecture: '大阪府' },
  { name: '名古屋市 (商業/サービス)', lat: 35.1814, lon: 136.9069, primary: 'services', sub: 'commerce', employees: 1500000, revenue_jpy: 12000000000000, prefecture: '愛知県' },
  { name: '福岡市 (商業/IT)', lat: 33.5904, lon: 130.4017, primary: 'services', sub: 'commerce_it', employees: 1100000, revenue_jpy: 9000000000000, prefecture: '福岡県' },

  // IT hubs
  { name: '渋谷ビットバレー (IT)', lat: 35.6595, lon: 139.7008, primary: 'services', sub: 'it_software', employees: 250000, revenue_jpy: 5500000000000, prefecture: '東京都' },
  { name: '六本木 (IT/メディア)', lat: 35.6604, lon: 139.7292, primary: 'services', sub: 'it_media', employees: 180000, revenue_jpy: 4000000000000, prefecture: '東京都' },

  // Tourism / Resort
  { name: '熱海市 (観光/温泉)', lat: 35.0950, lon: 139.0719, primary: 'services', sub: 'tourism', employees: 25000, revenue_jpy: 200000000000, prefecture: '静岡県' },
  { name: '別府市 (観光/温泉)', lat: 33.2839, lon: 131.4911, primary: 'services', sub: 'tourism', employees: 30000, revenue_jpy: 250000000000, prefecture: '大分県' },
  { name: '京都市 (観光/伝統工芸)', lat: 35.0116, lon: 135.7681, primary: 'services', sub: 'tourism_craft', employees: 720000, revenue_jpy: 6500000000000, prefecture: '京都府' },
  { name: '那覇市 (観光)', lat: 26.2125, lon: 127.6809, primary: 'services', sub: 'tourism', employees: 180000, revenue_jpy: 1300000000000, prefecture: '沖縄県' },

  // Agriculture / Fishery
  { name: '帯広市 (農業)', lat: 42.9239, lon: 143.1953, primary: 'agriculture', sub: 'crop_dairy', employees: 25000, revenue_jpy: 350000000000, prefecture: '北海道' },
  { name: '宮崎市 (畜産/野菜)', lat: 31.9111, lon: 131.4239, primary: 'agriculture', sub: 'livestock', employees: 35000, revenue_jpy: 250000000000, prefecture: '宮崎県' },
  { name: '焼津市 (漁業)', lat: 34.8678, lon: 138.3203, primary: 'fishery', sub: 'tuna', employees: 12000, revenue_jpy: 250000000000, prefecture: '静岡県' },
  { name: '銚子市 (漁業)', lat: 35.7344, lon: 140.8267, primary: 'fishery', sub: 'mixed', employees: 11000, revenue_jpy: 200000000000, prefecture: '千葉県' },
  { name: '気仙沼市 (漁業)', lat: 38.9067, lon: 141.5700, primary: 'fishery', sub: 'tuna_sanma', employees: 8500, revenue_jpy: 180000000000, prefecture: '宮城県' },
  { name: '釧路市 (漁業)', lat: 42.9849, lon: 144.3819, primary: 'fishery', sub: 'mixed', employees: 9000, revenue_jpy: 150000000000, prefecture: '北海道' },
  { name: '長崎市 (造船/漁業)', lat: 32.7503, lon: 129.8775, primary: 'manufacturing', sub: 'shipbuilding_fishery', employees: 60000, revenue_jpy: 1100000000000, prefecture: '長崎県' },
];

async function tryResas() {
  if (!RESAS_KEY) return null;
  try {
    const ctrl = new AbortController();
    const timeout = setTimeout(() => ctrl.abort(), 10000);
    const url = `${RESAS_URL}?year=2014&prefCode=13&sicCode=A`;
    const res = await fetch(url, {
      signal: ctrl.signal,
      headers: { 'X-API-KEY': RESAS_KEY },
    });
    clearTimeout(timeout);
    if (!res.ok) return null;
    const data = await res.json();
    const items = data.result?.data || [];
    if (items.length === 0) return null;
    return items.slice(0, 50).map((it, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [139.6917, 35.6896] },
      properties: {
        sic_code: it.sicCode || null,
        sic_name: it.sicName || null,
        value: it.value || null,
        country: 'JP',
        source: 'resas_api',
      },
    }));
  } catch {
    return null;
  }
}

function generateSeedData() {
  const now = new Date();
  return SEED_INDUSTRY.map((s, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [s.lon, s.lat] },
    properties: {
      city_id: `IND_${String(i + 1).padStart(5, '0')}`,
      name: s.name,
      primary_sector: s.primary,
      sub_sector: s.sub,
      employees: s.employees,
      revenue_jpy: s.revenue_jpy,
      prefecture: s.prefecture,
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'resas_industry_seed',
    },
  }));
}

async function tryOSMIndustrial() {
  return fetchOverpass(
    'way["landuse"="industrial"]["name"](area.jp);node["industrial"]["name"](area.jp);way["industrial"]["name"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        city_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || `Industrial zone ${i + 1}`,
        name_ja: el.tags?.name || null,
        primary_sector: 'manufacturing',
        sub_sector: el.tags?.industrial || 'mixed',
        operator: el.tags?.operator || null,
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

export default async function collectResasIndustry() {
  let features = await tryResas();
  let liveSource = 'resas_api';
  if (!features || features.length === 0) {
    features = await tryOSMIndustrial();
    liveSource = 'osm_overpass';
  }
  const live = !!(features && features.length > 0);
  if (!live) {
    features = generateSeedData();
    liveSource = 'resas_industry_seed';
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'resas_industry',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: liveSource,
      description: 'Japanese industrial composition - RESAS + OSM landuse=industrial zones',
    },
    metadata: {},
  };
}
