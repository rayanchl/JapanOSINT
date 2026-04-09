/**
 * RESAS Population Collector
 * Fetches population pyramid + future projections from RESAS API.
 * Falls back to a curated seed of major city population stats.
 */

const RESAS_KEY = process.env.RESAS_API_KEY || '';
const RESAS_URL = 'https://opendata.resas-portal.go.jp/api/v1/population/composition/perYear';

const SEED_CITY_POP = [
  { name: '東京23区', lat: 35.6896, lon: 139.6917, total: 9647000, age_0_14: 1090000, age_15_64: 6340000, age_65plus: 2217000, pref_code: 13 },
  { name: '横浜市', lat: 35.4437, lon: 139.6380, total: 3777000, age_0_14: 460000, age_15_64: 2360000, age_65plus: 957000, pref_code: 14 },
  { name: '大阪市', lat: 34.6864, lon: 135.5197, total: 2754000, age_0_14: 290000, age_15_64: 1740000, age_65plus: 724000, pref_code: 27 },
  { name: '名古屋市', lat: 35.1814, lon: 136.9069, total: 2326000, age_0_14: 280000, age_15_64: 1450000, age_65plus: 596000, pref_code: 23 },
  { name: '札幌市', lat: 43.0628, lon: 141.3478, total: 1973000, age_0_14: 220000, age_15_64: 1190000, age_65plus: 563000, pref_code: 1 },
  { name: '福岡市', lat: 33.5904, lon: 130.4017, total: 1612000, age_0_14: 220000, age_15_64: 1040000, age_65plus: 352000, pref_code: 40 },
  { name: '川崎市', lat: 35.5311, lon: 139.7036, total: 1538000, age_0_14: 190000, age_15_64: 1030000, age_65plus: 318000, pref_code: 14 },
  { name: '神戸市', lat: 34.6913, lon: 135.1830, total: 1525000, age_0_14: 180000, age_15_64: 920000, age_65plus: 425000, pref_code: 28 },
  { name: '京都市', lat: 35.0116, lon: 135.7681, total: 1464000, age_0_14: 170000, age_15_64: 880000, age_65plus: 414000, pref_code: 26 },
  { name: 'さいたま市', lat: 35.8617, lon: 139.6455, total: 1324000, age_0_14: 170000, age_15_64: 850000, age_65plus: 304000, pref_code: 11 },
  { name: '広島市', lat: 34.3853, lon: 132.4553, total: 1199000, age_0_14: 160000, age_15_64: 740000, age_65plus: 299000, pref_code: 34 },
  { name: '仙台市', lat: 38.2683, lon: 140.8719, total: 1097000, age_0_14: 140000, age_15_64: 720000, age_65plus: 237000, pref_code: 4 },
  { name: '北九州市', lat: 33.8836, lon: 130.8814, total: 940000, age_0_14: 110000, age_15_64: 540000, age_65plus: 290000, pref_code: 40 },
  { name: '千葉市', lat: 35.6083, lon: 140.1233, total: 977000, age_0_14: 120000, age_15_64: 610000, age_65plus: 247000, pref_code: 12 },
  { name: '堺市', lat: 34.5733, lon: 135.4828, total: 826000, age_0_14: 100000, age_15_64: 500000, age_65plus: 226000, pref_code: 27 },
  { name: '新潟市', lat: 37.9161, lon: 139.0364, total: 789000, age_0_14: 90000, age_15_64: 470000, age_65plus: 229000, pref_code: 15 },
  { name: '浜松市', lat: 34.7108, lon: 137.7261, total: 791000, age_0_14: 100000, age_15_64: 480000, age_65plus: 211000, pref_code: 22 },
  { name: '熊本市', lat: 32.8019, lon: 130.7256, total: 738000, age_0_14: 100000, age_15_64: 450000, age_65plus: 188000, pref_code: 43 },
  { name: '相模原市', lat: 35.5719, lon: 139.3733, total: 725000, age_0_14: 90000, age_15_64: 460000, age_65plus: 175000, pref_code: 14 },
  { name: '岡山市', lat: 34.6628, lon: 133.9197, total: 720000, age_0_14: 90000, age_15_64: 440000, age_65plus: 190000, pref_code: 33 },
  { name: '静岡市', lat: 34.9756, lon: 138.3828, total: 692000, age_0_14: 80000, age_15_64: 400000, age_65plus: 212000, pref_code: 22 },
  { name: '鹿児島市', lat: 31.5963, lon: 130.5571, total: 591000, age_0_14: 80000, age_15_64: 350000, age_65plus: 161000, pref_code: 46 },
  { name: '那覇市', lat: 26.2125, lon: 127.6809, total: 318000, age_0_14: 50000, age_15_64: 200000, age_65plus: 68000, pref_code: 47 },
  { name: '青森市', lat: 40.8244, lon: 140.7400, total: 275000, age_0_14: 30000, age_15_64: 160000, age_65plus: 85000, pref_code: 2 },
  { name: '盛岡市', lat: 39.7036, lon: 141.1525, total: 287000, age_0_14: 35000, age_15_64: 170000, age_65plus: 82000, pref_code: 3 },
  { name: '秋田市', lat: 39.7186, lon: 140.1024, total: 304000, age_0_14: 30000, age_15_64: 170000, age_65plus: 104000, pref_code: 5 },
  { name: '富山市', lat: 36.6953, lon: 137.2113, total: 410000, age_0_14: 50000, age_15_64: 240000, age_65plus: 120000, pref_code: 16 },
  { name: '金沢市', lat: 36.5613, lon: 136.6562, total: 460000, age_0_14: 60000, age_15_64: 280000, age_65plus: 120000, pref_code: 17 },
  { name: '長野市', lat: 36.6489, lon: 138.1944, total: 369000, age_0_14: 40000, age_15_64: 210000, age_65plus: 119000, pref_code: 20 },
  { name: '甲府市', lat: 35.6642, lon: 138.5683, total: 187000, age_0_14: 20000, age_15_64: 110000, age_65plus: 57000, pref_code: 19 },
];

async function tryResas() {
  if (!RESAS_KEY) return null;
  try {
    const ctrl = new AbortController();
    const timeout = setTimeout(() => ctrl.abort(), 10000);
    const url = `${RESAS_URL}?prefCode=13&cityCode=-`;
    const res = await fetch(url, {
      signal: ctrl.signal,
      headers: { 'X-API-KEY': RESAS_KEY },
    });
    clearTimeout(timeout);
    if (!res.ok) return null;
    const data = await res.json();
    const totalPop = data.result?.data?.[0]?.data || [];
    if (totalPop.length === 0) return null;
    return totalPop.slice(0, 30).map((p, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [139.6917, 35.6896] },
      properties: {
        year: p.year,
        population: p.value,
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
  return SEED_CITY_POP.map((c, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [c.lon, c.lat] },
    properties: {
      city_id: `RESAS_${String(i + 1).padStart(5, '0')}`,
      name: c.name,
      total: c.total,
      age_0_14: c.age_0_14,
      age_15_64: c.age_15_64,
      age_65plus: c.age_65plus,
      aging_rate: c.total > 0 ? Math.round((c.age_65plus / c.total) * 1000) / 10 : null,
      pref_code: c.pref_code,
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'resas_population_seed',
    },
  }));
}

export default async function collectResasPopulation() {
  let features = await tryResas();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'resas_population',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'RESAS population composition by city - age distribution and aging rate',
    },
    metadata: {},
  };
}
