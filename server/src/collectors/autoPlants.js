/**
 * Automotive Plants Collector
 * OSM Overpass `industrial=automobile` + curated seed of major Japanese
 * automaker assembly plants.
 */

import { fetchOverpass } from './_liveHelpers.js';

const SEED_AUTO_PLANTS = [
  // Toyota (HQ + assembly)
  { name: 'トヨタ自動車 本社工場', lat: 35.0833, lon: 137.1561, brand: 'Toyota', kind: 'assembly', employees: 9000 },
  { name: 'トヨタ自動車 元町工場', lat: 35.0925, lon: 137.1417, brand: 'Toyota', kind: 'assembly', employees: 6500 },
  { name: 'トヨタ自動車 高岡工場', lat: 35.0681, lon: 137.1361, brand: 'Toyota', kind: 'assembly', employees: 5000 },
  { name: 'トヨタ自動車 堤工場', lat: 35.0589, lon: 137.1494, brand: 'Toyota', kind: 'assembly', employees: 4500 },
  { name: 'トヨタ自動車 田原工場', lat: 34.6647, lon: 137.2531, brand: 'Toyota', kind: 'assembly', employees: 7500 },
  { name: 'トヨタ自動車 上郷工場', lat: 35.0306, lon: 137.0850, brand: 'Toyota', kind: 'engine', employees: 3500 },
  { name: 'トヨタ自動車 衣浦工場', lat: 34.8561, lon: 136.9886, brand: 'Toyota', kind: 'transmission', employees: 2800 },
  { name: 'トヨタ自動車 三好工場', lat: 35.0922, lon: 137.0817, brand: 'Toyota', kind: 'parts', employees: 2400 },
  { name: 'トヨタ自動車 九州 宮田工場', lat: 33.6489, lon: 130.6353, brand: 'Toyota', kind: 'assembly', employees: 6800 },
  { name: 'トヨタ自動車 東日本 岩手工場', lat: 39.2233, lon: 141.0900, brand: 'Toyota', kind: 'assembly', employees: 4500 },
  { name: 'トヨタ自動車 東日本 宮城大衡工場', lat: 38.4789, lon: 140.8625, brand: 'Toyota', kind: 'assembly', employees: 3500 },
  // Lexus
  { name: 'レクサス 田原工場', lat: 34.6650, lon: 137.2533, brand: 'Lexus', kind: 'assembly', employees: 0 },
  { name: 'レクサス 元町工場', lat: 35.0925, lon: 137.1417, brand: 'Lexus', kind: 'assembly', employees: 0 },
  // Nissan
  { name: '日産自動車 横浜工場', lat: 35.4636, lon: 139.6522, brand: 'Nissan', kind: 'engine', employees: 6500 },
  { name: '日産自動車 追浜工場', lat: 35.3144, lon: 139.6347, brand: 'Nissan', kind: 'assembly', employees: 3500 },
  { name: '日産自動車 栃木工場', lat: 36.5683, lon: 140.0344, brand: 'Nissan', kind: 'assembly', employees: 4800 },
  { name: '日産自動車 九州工場', lat: 33.7281, lon: 130.7553, brand: 'Nissan', kind: 'assembly', employees: 5500 },
  { name: '日産自動車 いわき工場', lat: 37.0533, lon: 140.8983, brand: 'Nissan', kind: 'engine', employees: 1800 },
  // Honda
  { name: 'ホンダ 鈴鹿製作所', lat: 34.8581, lon: 136.5928, brand: 'Honda', kind: 'assembly', employees: 7000 },
  { name: 'ホンダ 寄居工場', lat: 36.1233, lon: 139.2014, brand: 'Honda', kind: 'assembly', employees: 4000 },
  { name: 'ホンダ 小川工場', lat: 36.0247, lon: 139.2522, brand: 'Honda', kind: 'engine', employees: 2200 },
  { name: 'ホンダ 熊本製作所', lat: 32.9614, lon: 130.7844, brand: 'Honda', kind: 'motorcycle', employees: 3200 },
  { name: 'ホンダ 浜松製作所', lat: 34.7228, lon: 137.7164, brand: 'Honda', kind: 'engine', employees: 2700 },
  // Mazda
  { name: 'マツダ 本社工場 (広島)', lat: 34.3683, lon: 132.4839, brand: 'Mazda', kind: 'assembly', employees: 13000 },
  { name: 'マツダ 防府工場', lat: 34.0750, lon: 131.5958, brand: 'Mazda', kind: 'assembly', employees: 5500 },
  { name: 'マツダ 三次事業所', lat: 34.7906, lon: 132.8536, brand: 'Mazda', kind: 'parts', employees: 1800 },
  // Subaru
  { name: 'スバル 群馬製作所 本工場', lat: 36.3306, lon: 139.3781, brand: 'Subaru', kind: 'assembly', employees: 9000 },
  { name: 'スバル 群馬製作所 矢島工場', lat: 36.3000, lon: 139.4197, brand: 'Subaru', kind: 'assembly', employees: 5500 },
  { name: 'スバル 群馬製作所 大泉工場', lat: 36.2475, lon: 139.4133, brand: 'Subaru', kind: 'engine', employees: 2800 },
  // Mitsubishi
  { name: '三菱自動車 岡崎工場', lat: 34.9700, lon: 137.1839, brand: 'Mitsubishi', kind: 'assembly', employees: 4000 },
  { name: '三菱自動車 水島製作所', lat: 34.5108, lon: 133.7350, brand: 'Mitsubishi', kind: 'assembly', employees: 4500 },
  { name: '三菱自動車 京都工場', lat: 34.9233, lon: 135.7100, brand: 'Mitsubishi', kind: 'engine', employees: 1900 },
  // Suzuki
  { name: 'スズキ 湖西工場', lat: 34.7314, lon: 137.5022, brand: 'Suzuki', kind: 'assembly', employees: 5500 },
  { name: 'スズキ 相良工場', lat: 34.6661, lon: 138.1947, brand: 'Suzuki', kind: 'engine', employees: 1800 },
  { name: 'スズキ 磐田工場', lat: 34.7261, lon: 137.8889, brand: 'Suzuki', kind: 'motorcycle', employees: 2400 },
  // Daihatsu
  { name: 'ダイハツ 本社・池田工場', lat: 34.8408, lon: 135.4167, brand: 'Daihatsu', kind: 'assembly', employees: 6500 },
  { name: 'ダイハツ 京都工場', lat: 34.9272, lon: 135.7144, brand: 'Daihatsu', kind: 'assembly', employees: 4200 },
  { name: 'ダイハツ 九州 大分工場', lat: 33.5717, lon: 131.5783, brand: 'Daihatsu', kind: 'assembly', employees: 3800 },
  // Isuzu / Hino
  { name: 'いすゞ 藤沢工場', lat: 35.3478, lon: 139.4444, brand: 'Isuzu', kind: 'assembly', employees: 5500 },
  { name: 'いすゞ 栃木工場', lat: 36.6739, lon: 139.9239, brand: 'Isuzu', kind: 'engine', employees: 3000 },
  { name: '日野自動車 羽村工場', lat: 35.7544, lon: 139.3214, brand: 'Hino', kind: 'assembly', employees: 6800 },
  { name: '日野自動車 古河工場', lat: 36.1700, lon: 139.7250, brand: 'Hino', kind: 'assembly', employees: 4500 },
  // Kawasaki / Yamaha
  { name: '川崎重工 明石工場 (二輪)', lat: 34.6450, lon: 134.9486, brand: 'Kawasaki', kind: 'motorcycle', employees: 4200 },
  { name: 'ヤマハ発動機 本社工場', lat: 34.7028, lon: 137.8669, brand: 'Yamaha', kind: 'motorcycle', employees: 5800 },
  { name: 'ヤマハ発動機 磐田南工場', lat: 34.7042, lon: 137.8633, brand: 'Yamaha', kind: 'motorcycle', employees: 2400 },
];

async function tryOverpass() {
  return fetchOverpass(
    'way["industrial"="automobile"](area.jp);way["industrial"="auto_parts"](area.jp);',
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        plant_id: `OSM_${el.id}`,
        name: el.tags?.name || 'Auto Plant',
        brand: el.tags?.operator || el.tags?.brand || 'unknown',
        source: 'osm_overpass',
      },
    }),
  );
}

function generateSeedData() {
  return SEED_AUTO_PLANTS.map((p, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [p.lon, p.lat] },
    properties: {
      plant_id: `AUTO_${String(i + 1).padStart(5, '0')}`,
      name: p.name,
      brand: p.brand,
      kind: p.kind,
      employees: p.employees,
      country: 'JP',
      source: 'auto_plants_seed',
    },
  }));
}

export default async function collectAutoPlants() {
  let features = await tryOverpass();
  const live = !!(features && features.length > 0);
  if (!live) features = [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'auto_plants',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Japanese automotive assembly plants: Toyota, Nissan, Honda, Mazda, Subaru, Mitsubishi, Suzuki, Daihatsu, Isuzu, Hino, Kawasaki, Yamaha',
    },
  };
}
