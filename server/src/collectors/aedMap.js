/**
 * AED Map Collector
 * Maps all AED (Automated External Defibrillator) locations across Japan via
 * OSM Overpass. Uses tiled nationwide fetch + curated SEED list as fallback.
 */

import { fetchOverpassTiled } from './_liveHelpers.js';

const SEED_AEDS = [
  { name: '東京駅 丸の内中央口', lat: 35.6812, lon: 139.7671, location: 'station', prefecture: '東京都' },
  { name: '新宿駅 東口', lat: 35.6896, lon: 139.7006, location: 'station', prefecture: '東京都' },
  { name: '渋谷駅 ハチ公口', lat: 35.6590, lon: 139.7016, location: 'station', prefecture: '東京都' },
  { name: '池袋駅 西口', lat: 35.7295, lon: 139.7109, location: 'station', prefecture: '東京都' },
  { name: '品川駅 港南口', lat: 35.6284, lon: 139.7387, location: 'station', prefecture: '東京都' },
  { name: '上野駅 中央改札', lat: 35.7138, lon: 139.7770, location: 'station', prefecture: '東京都' },
  { name: '東京国際フォーラム', lat: 35.6772, lon: 139.7635, location: 'public', prefecture: '東京都' },
  { name: '東京タワー', lat: 35.6586, lon: 139.7454, location: 'tourist', prefecture: '東京都' },
  { name: '東京スカイツリー', lat: 35.7100, lon: 139.8107, location: 'tourist', prefecture: '東京都' },
  { name: '羽田空港第1ターミナル', lat: 35.5494, lon: 139.7798, location: 'airport', prefecture: '東京都' },
  { name: '羽田空港第2ターミナル', lat: 35.5532, lon: 139.7813, location: 'airport', prefecture: '東京都' },
  { name: '羽田空港第3ターミナル', lat: 35.5494, lon: 139.7858, location: 'airport', prefecture: '東京都' },
  { name: '成田空港第1ターミナル', lat: 35.7720, lon: 140.3929, location: 'airport', prefecture: '千葉県' },
  { name: '成田空港第2ターミナル', lat: 35.7666, lon: 140.3868, location: 'airport', prefecture: '千葉県' },
  { name: '東京ドーム', lat: 35.7056, lon: 139.7519, location: 'stadium', prefecture: '東京都' },
  { name: '国立競技場', lat: 35.6781, lon: 139.7148, location: 'stadium', prefecture: '東京都' },
  { name: '横浜駅', lat: 35.4658, lon: 139.6224, location: 'station', prefecture: '神奈川県' },
  { name: 'みなとみらい21', lat: 35.4561, lon: 139.6317, location: 'public', prefecture: '神奈川県' },
  { name: '大阪駅', lat: 34.7024, lon: 135.4959, location: 'station', prefecture: '大阪府' },
  { name: '京都駅', lat: 34.9858, lon: 135.7588, location: 'station', prefecture: '京都府' },
  { name: '名古屋駅', lat: 35.1709, lon: 136.8815, location: 'station', prefecture: '愛知県' },
  { name: '札幌駅', lat: 43.0686, lon: 141.3508, location: 'station', prefecture: '北海道' },
  { name: '仙台駅', lat: 38.2602, lon: 140.8825, location: 'station', prefecture: '宮城県' },
  { name: '広島駅', lat: 34.3979, lon: 132.4750, location: 'station', prefecture: '広島県' },
  { name: '福岡空港', lat: 33.5856, lon: 130.4506, location: 'airport', prefecture: '福岡県' },
  { name: '関西国際空港', lat: 34.4347, lon: 135.2444, location: 'airport', prefecture: '大阪府' },
  { name: '中部国際空港', lat: 34.8584, lon: 136.8054, location: 'airport', prefecture: '愛知県' },
  { name: '新千歳空港', lat: 42.7752, lon: 141.6920, location: 'airport', prefecture: '北海道' },
  { name: '那覇空港', lat: 26.1958, lon: 127.6458, location: 'airport', prefecture: '沖縄県' },
];

async function tryLive() {
  return fetchOverpassTiled(
    (bbox) => [
      `node["emergency"="defibrillator"](${bbox});`,
      `node["emergency"="aed"](${bbox});`,
    ].join(''),
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        facility_id: `AED_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || 'AED',
        indoor: el.tags?.indoor || null,
        access: el.tags?.access || 'public',
        opening_hours: el.tags?.opening_hours || null,
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
  return SEED_AEDS.map((a, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [a.lon, a.lat] },
    properties: {
      facility_id: `AED_${String(i + 1).padStart(5, '0')}`,
      name: a.name,
      location_type: a.location,
      prefecture: a.prefecture,
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'aed_seed',
    },
  }));
}

export default async function collectAedMap() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'aed_map',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Japan AED (defibrillator) locations - tiled OSM nationwide',
    },
  };
}
