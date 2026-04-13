/**
 * Bosai Shelter Collector
 * Maps disaster evacuation shelters across Japan via OSM Overpass API.
 * Falls back to a curated seed of major designated emergency shelters.
 */

import { fetchOverpassTiled } from './_liveHelpers.js';

const SEED_SHELTERS = [
  { name: '東京体育館', lat: 35.6816, lon: 139.7141, capacity: 5000, type: 'designated', prefecture: '東京都' },
  { name: '日本武道館', lat: 35.6933, lon: 139.7497, capacity: 14000, type: 'designated', prefecture: '東京都' },
  { name: '代々木公園', lat: 35.6717, lon: 139.6950, capacity: 80000, type: 'evacuation_area', prefecture: '東京都' },
  { name: '上野恩賜公園', lat: 35.7148, lon: 139.7740, capacity: 60000, type: 'evacuation_area', prefecture: '東京都' },
  { name: '皇居外苑', lat: 35.6803, lon: 139.7647, capacity: 50000, type: 'evacuation_area', prefecture: '東京都' },
  { name: '新宿御苑', lat: 35.6852, lon: 139.7100, capacity: 40000, type: 'evacuation_area', prefecture: '東京都' },
  { name: '駒沢オリンピック公園', lat: 35.6256, lon: 139.6589, capacity: 30000, type: 'evacuation_area', prefecture: '東京都' },
  { name: '光が丘公園', lat: 35.7669, lon: 139.6406, capacity: 25000, type: 'evacuation_area', prefecture: '東京都' },
  { name: '東京ビッグサイト', lat: 35.6300, lon: 139.7950, capacity: 20000, type: 'designated', prefecture: '東京都' },
  { name: '国立代々木競技場', lat: 35.6678, lon: 139.6989, capacity: 13000, type: 'designated', prefecture: '東京都' },
  { name: '横浜アリーナ', lat: 35.5117, lon: 139.6189, capacity: 17000, type: 'designated', prefecture: '神奈川県' },
  { name: '日産スタジアム', lat: 35.5097, lon: 139.6058, capacity: 72000, type: 'designated', prefecture: '神奈川県' },
  { name: '横浜公園', lat: 35.4453, lon: 139.6411, capacity: 15000, type: 'evacuation_area', prefecture: '神奈川県' },
  { name: '山下公園', lat: 35.4444, lon: 139.6489, capacity: 18000, type: 'evacuation_area', prefecture: '神奈川県' },
  { name: '大阪城公園', lat: 34.6873, lon: 135.5259, capacity: 50000, type: 'evacuation_area', prefecture: '大阪府' },
  { name: '長居公園', lat: 34.6125, lon: 135.5189, capacity: 60000, type: 'evacuation_area', prefecture: '大阪府' },
  { name: '万博記念公園', lat: 34.8067, lon: 135.5286, capacity: 80000, type: 'evacuation_area', prefecture: '大阪府' },
  { name: '京都御苑', lat: 35.0244, lon: 135.7625, capacity: 30000, type: 'evacuation_area', prefecture: '京都府' },
  { name: '名古屋城', lat: 35.1856, lon: 136.8997, capacity: 40000, type: 'evacuation_area', prefecture: '愛知県' },
  { name: '鶴舞公園', lat: 35.1556, lon: 136.9275, capacity: 25000, type: 'evacuation_area', prefecture: '愛知県' },
  { name: '札幌大通公園', lat: 43.0606, lon: 141.3547, capacity: 35000, type: 'evacuation_area', prefecture: '北海道' },
  { name: '北海道神宮', lat: 43.0539, lon: 141.3083, capacity: 20000, type: 'evacuation_area', prefecture: '北海道' },
  { name: '勾当台公園', lat: 38.2683, lon: 140.8719, capacity: 15000, type: 'evacuation_area', prefecture: '宮城県' },
  { name: '仙台城跡', lat: 38.2528, lon: 140.8569, capacity: 30000, type: 'evacuation_area', prefecture: '宮城県' },
  { name: '大濠公園', lat: 33.5867, lon: 130.3786, capacity: 30000, type: 'evacuation_area', prefecture: '福岡県' },
  { name: '舞鶴公園', lat: 33.5867, lon: 130.3833, capacity: 20000, type: 'evacuation_area', prefecture: '福岡県' },
  { name: '広島平和記念公園', lat: 34.3925, lon: 132.4525, capacity: 25000, type: 'evacuation_area', prefecture: '広島県' },
  { name: '熊本城', lat: 32.8064, lon: 130.7058, capacity: 30000, type: 'evacuation_area', prefecture: '熊本県' },
  { name: '鹿児島城山', lat: 31.5969, lon: 130.5500, capacity: 15000, type: 'evacuation_area', prefecture: '鹿児島県' },
  { name: '首里城公園', lat: 26.2169, lon: 127.7194, capacity: 20000, type: 'evacuation_area', prefecture: '沖縄県' },
  { name: '那覇市役所', lat: 26.2125, lon: 127.6809, capacity: 5000, type: 'designated', prefecture: '沖縄県' },
  { name: '兼六園', lat: 36.5622, lon: 136.6625, capacity: 25000, type: 'evacuation_area', prefecture: '石川県' },
  { name: '岡崎公園', lat: 35.0136, lon: 135.7833, capacity: 20000, type: 'evacuation_area', prefecture: '京都府' },
  { name: '奈良公園', lat: 34.6850, lon: 135.8431, capacity: 50000, type: 'evacuation_area', prefecture: '奈良県' },
  { name: '備前公園', lat: 34.6628, lon: 133.9197, capacity: 18000, type: 'evacuation_area', prefecture: '岡山県' },
  { name: '松山城', lat: 33.8458, lon: 132.7656, capacity: 15000, type: 'evacuation_area', prefecture: '愛媛県' },
  { name: '高松栗林公園', lat: 34.3275, lon: 134.0469, capacity: 20000, type: 'evacuation_area', prefecture: '香川県' },
  { name: '津波避難タワー (黒潮町)', lat: 33.0064, lon: 132.9844, capacity: 230, type: 'tsunami_tower', prefecture: '高知県' },
  { name: '津波避難タワー (浜松)', lat: 34.6603, lon: 137.7639, capacity: 150, type: 'tsunami_tower', prefecture: '静岡県' },
  { name: '津波避難ビル (沼津)', lat: 35.0950, lon: 138.8636, capacity: 500, type: 'tsunami_building', prefecture: '静岡県' },
];

async function tryOverpass() {
  return fetchOverpassTiled(
    (bbox) => [
      `node["emergency"="assembly_point"](${bbox});`,
      `node["amenity"="shelter"](${bbox});`,
      `way["amenity"="shelter"](${bbox});`,
      `node["emergency"="evacuation_center"](${bbox});`,
      `way["emergency"="evacuation_center"](${bbox});`,
    ].join(''),
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        facility_id: `SHELTER_${el.id}`,
        name: el.tags?.name || el.tags?.['name:en'] || 'Evacuation Point',
        shelter_type: el.tags?.shelter_type || el.tags?.emergency || 'assembly_point',
        capacity: parseInt(el.tags?.capacity) || null,
        operator: el.tags?.operator || null,
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
    { queryTimeout: 180, timeoutMs: 90_000 },
  );
}

function generateSeedData() {
  const now = new Date();
  return SEED_SHELTERS.map((s, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [s.lon, s.lat] },
    properties: {
      facility_id: `SHELTER_${String(i + 1).padStart(5, '0')}`,
      name: s.name,
      shelter_type: s.type,
      capacity: s.capacity,
      prefecture: s.prefecture,
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'bosai_shelter_seed',
    },
  }));
}

export default async function collectBosaiShelter() {
  let features = await tryOverpass();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'bosai_shelter',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Japan disaster evacuation shelters and assembly points',
    },
    metadata: {},
  };
}
