/**
 * AED Map Collector
 * Maps AED (Automated External Defibrillator) locations across Japan via OSM Overpass.
 * Falls back to a curated seed of major public AED sites.
 */

const OVERPASS_URL = process.env.OVERPASS_URL || 'https://overpass-api.de/api/interpreter';

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
  { name: '富士山五合目', lat: 35.3950, lon: 138.7300, location: 'tourist', prefecture: '山梨県' },
  { name: '甲子園球場', lat: 34.7211, lon: 135.3617, location: 'stadium', prefecture: '兵庫県' },
  { name: '大阪城', lat: 34.6873, lon: 135.5259, location: 'tourist', prefecture: '大阪府' },
  { name: '清水寺', lat: 34.9949, lon: 135.7851, location: 'tourist', prefecture: '京都府' },
  { name: '伏見稲荷大社', lat: 34.9671, lon: 135.7727, location: 'tourist', prefecture: '京都府' },
  { name: '厳島神社', lat: 34.2960, lon: 132.3199, location: 'tourist', prefecture: '広島県' },
  { name: '原爆ドーム', lat: 34.3955, lon: 132.4536, location: 'tourist', prefecture: '広島県' },
  { name: '札幌時計台', lat: 43.0628, lon: 141.3531, location: 'tourist', prefecture: '北海道' },
  { name: '日産スタジアム', lat: 35.5097, lon: 139.6058, location: 'stadium', prefecture: '神奈川県' },
  { name: '埼玉スタジアム2002', lat: 35.9036, lon: 139.7172, location: 'stadium', prefecture: '埼玉県' },
  { name: '東京体育館', lat: 35.6816, lon: 139.7141, location: 'stadium', prefecture: '東京都' },
];

async function tryOverpass() {
  try {
    const ctrl = new AbortController();
    const timeout = setTimeout(() => ctrl.abort(), 25000);
    const query = `[out:json][timeout:25];
area["ISO3166-1"="JP"][admin_level=2]->.jp;
(node["emergency"="defibrillator"](area.jp);
 node["emergency"="aed"](area.jp););
out 800;`;
    const res = await fetch(OVERPASS_URL, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: 'data=' + encodeURIComponent(query),
      signal: ctrl.signal,
    });
    clearTimeout(timeout);
    if (!res.ok) return null;
    const data = await res.json();
    if (!data.elements || data.elements.length === 0) return null;
    return data.elements
      .map((el, i) => {
        if (el.lat == null || el.lon == null) return null;
        return {
          type: 'Feature',
          geometry: { type: 'Point', coordinates: [el.lon, el.lat] },
          properties: {
            facility_id: `AED_${String(i + 1).padStart(5, '0')}`,
            name: el.tags?.name || el.tags?.['name:en'] || 'AED',
            indoor: el.tags?.indoor || null,
            access: el.tags?.access || 'public',
            opening_hours: el.tags?.opening_hours || null,
            country: 'JP',
            source: 'overpass_api',
          },
        };
      })
      .filter(Boolean);
  } catch {
    return null;
  }
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
  let features = await tryOverpass();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'aed_map',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Japan AED (defibrillator) locations - stations, airports, public facilities',
    },
    metadata: {},
  };
}
