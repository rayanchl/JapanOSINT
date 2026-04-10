/**
 * Pachinko Density Collector
 * Pachinko parlors density across Japan - adult gaming centers.
 * Live: OSM Overpass `leisure=adult_gaming_centre`.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpass(
    'node["leisure"="adult_gaming_centre"](area.jp);way["leisure"="adult_gaming_centre"](area.jp);node["shop"="games"]["name"~"パチンコ"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        parlor_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || `Pachinko ${i + 1}`,
        name_ja: el.tags?.name || null,
        operator: el.tags?.operator || null,
        machines: el.tags?.capacity || null,
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

// City-level pachinko density (NPA regulatory data - 遊技場営業許可数)
const SEED_DENSITY = [
  { city: '東京都 新宿区', lat: 35.6896, lon: 139.6917, parlors: 55, machines_est: 45000, prefecture: '東京都' },
  { city: '東京都 渋谷区', lat: 35.6580, lon: 139.7016, parlors: 32, machines_est: 25000, prefecture: '東京都' },
  { city: '東京都 池袋', lat: 35.7295, lon: 139.7109, parlors: 40, machines_est: 32000, prefecture: '東京都' },
  { city: '東京都 台東区 上野', lat: 35.7141, lon: 139.7774, parlors: 28, machines_est: 22000, prefecture: '東京都' },
  { city: '東京都 千代田区 秋葉原', lat: 35.6984, lon: 139.7731, parlors: 15, machines_est: 10000, prefecture: '東京都' },
  { city: '横浜市 中区', lat: 35.4437, lon: 139.6380, parlors: 48, machines_est: 40000, prefecture: '神奈川県' },
  { city: '川崎市 川崎区', lat: 35.5311, lon: 139.7036, parlors: 35, machines_est: 28000, prefecture: '神奈川県' },
  { city: '大阪市 中央区 難波', lat: 34.6650, lon: 135.5036, parlors: 52, machines_est: 42000, prefecture: '大阪府' },
  { city: '大阪市 北区 梅田', lat: 34.7022, lon: 135.4950, parlors: 44, machines_est: 35000, prefecture: '大阪府' },
  { city: '大阪市 西成区', lat: 34.6469, lon: 135.5058, parlors: 45, machines_est: 32000, prefecture: '大阪府' },
  { city: '堺市 堺区', lat: 34.5733, lon: 135.4828, parlors: 28, machines_est: 22000, prefecture: '大阪府' },
  { city: '名古屋市 中区', lat: 35.1708, lon: 136.9050, parlors: 42, machines_est: 35000, prefecture: '愛知県' },
  { city: '名古屋市 中村区', lat: 35.1706, lon: 136.8803, parlors: 35, machines_est: 28000, prefecture: '愛知県' },
  { city: '京都市 下京区', lat: 34.9847, lon: 135.7636, parlors: 30, machines_est: 23000, prefecture: '京都府' },
  { city: '神戸市 中央区', lat: 34.6913, lon: 135.1830, parlors: 30, machines_est: 25000, prefecture: '兵庫県' },
  { city: '札幌市 中央区 すすきの', lat: 43.0555, lon: 141.3522, parlors: 48, machines_est: 38000, prefecture: '北海道' },
  { city: '仙台市 青葉区', lat: 38.2683, lon: 140.8719, parlors: 35, machines_est: 28000, prefecture: '宮城県' },
  { city: '福岡市 博多区 中洲', lat: 33.5931, lon: 130.4044, parlors: 40, machines_est: 32000, prefecture: '福岡県' },
  { city: '北九州市 小倉北区', lat: 33.8864, lon: 130.8792, parlors: 28, machines_est: 22000, prefecture: '福岡県' },
  { city: '広島市 中区', lat: 34.3853, lon: 132.4553, parlors: 30, machines_est: 25000, prefecture: '広島県' },
  { city: '岡山市 北区', lat: 34.6628, lon: 133.9197, parlors: 25, machines_est: 20000, prefecture: '岡山県' },
  { city: '松山市', lat: 33.8392, lon: 132.7656, parlors: 22, machines_est: 17000, prefecture: '愛媛県' },
  { city: '高松市', lat: 34.3401, lon: 134.0434, parlors: 20, machines_est: 15000, prefecture: '香川県' },
  { city: '金沢市', lat: 36.5613, lon: 136.6562, parlors: 22, machines_est: 17000, prefecture: '石川県' },
  { city: '新潟市 中央区', lat: 37.9161, lon: 139.0364, parlors: 30, machines_est: 24000, prefecture: '新潟県' },
  { city: '静岡市 葵区', lat: 34.9756, lon: 138.3828, parlors: 25, machines_est: 20000, prefecture: '静岡県' },
  { city: '浜松市 中区', lat: 34.7108, lon: 137.7261, parlors: 32, machines_est: 25000, prefecture: '静岡県' },
  { city: '鹿児島市', lat: 31.5963, lon: 130.5571, parlors: 25, machines_est: 19000, prefecture: '鹿児島県' },
  { city: '那覇市', lat: 26.2125, lon: 127.6809, parlors: 22, machines_est: 16000, prefecture: '沖縄県' },
  { city: '熊本市 中央区', lat: 32.8019, lon: 130.7256, parlors: 28, machines_est: 22000, prefecture: '熊本県' },
  { city: '長崎市', lat: 32.7503, lon: 129.8775, parlors: 22, machines_est: 17000, prefecture: '長崎県' },
  { city: '宇都宮市', lat: 36.5658, lon: 139.8836, parlors: 28, machines_est: 22000, prefecture: '栃木県' },
  { city: '水戸市', lat: 36.3658, lon: 140.4711, parlors: 25, machines_est: 19000, prefecture: '茨城県' },
  { city: '前橋市', lat: 36.3911, lon: 139.0608, parlors: 22, machines_est: 17000, prefecture: '群馬県' },
  { city: 'さいたま市 大宮区', lat: 35.9081, lon: 139.6297, parlors: 35, machines_est: 28000, prefecture: '埼玉県' },
  { city: '千葉市 中央区', lat: 35.6083, lon: 140.1233, parlors: 30, machines_est: 24000, prefecture: '千葉県' },
];

function generateSeedData() {
  return SEED_DENSITY.map((d, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [d.lon, d.lat] },
    properties: {
      area_id: `PACHI_${String(i + 1).padStart(4, '0')}`,
      name: d.city,
      parlor_count: d.parlors,
      machines_est: d.machines_est,
      prefecture: d.prefecture,
      country: 'JP',
      source: 'pachinko_density_seed',
    },
  }));
}

export default async function collectPachinkoDensity() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'pachinko-density',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'osm_overpass' : 'pachinko_density_seed',
      description: 'Pachinko parlor density - adult gaming centers across Japan',
    },
    metadata: {},
  };
}
