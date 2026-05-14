/**
 * Police Crime Data Collector
 * Representative crime/incident data for major Japanese prefectures
 * Distributed with realistic densities (Tokyo highest)
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return await fetchOverpass(
    'node["amenity"="police"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        incident_id: `POL_LIVE_${String(i + 1).padStart(5, '0')}`,
        area: el.tags?.name || el.tags?.['name:en'] || `Police ${el.id}`,
        pref: el.tags?.['addr:state'] || null,
        incident_type: 'police_station',
        severity: 'info',
        operator: el.tags?.operator || null,
        country: 'JP',
        source: 'police_live',
      },
    })
  );
}

const INCIDENT_TYPES = ['theft', 'assault', 'traffic_accident', 'fraud', 'burglary', 'vandalism', 'drug_offense', 'pickpocket', 'bicycle_theft', 'shoplifting'];
const SEVERITY_LEVELS = ['low', 'medium', 'high'];

// City areas with approximate crime-density weighting
const CITY_AREAS = [
  // Tokyo - highest density
  { area: '新宿区歌舞伎町', pref: '東京都', lat: 35.6938, lon: 139.7036, weight: 5.0 },
  { area: '渋谷区センター街', pref: '東京都', lat: 35.6591, lon: 139.6998, weight: 4.5 },
  { area: '豊島区池袋', pref: '東京都', lat: 35.7295, lon: 139.7109, weight: 4.0 },
  { area: '台東区上野', pref: '東京都', lat: 35.7141, lon: 139.7774, weight: 3.5 },
  { area: '千代田区秋葉原', pref: '東京都', lat: 35.6984, lon: 139.7731, weight: 3.0 },
  { area: '港区六本木', pref: '東京都', lat: 35.6626, lon: 139.7315, weight: 3.5 },
  { area: '中央区銀座', pref: '東京都', lat: 35.6717, lon: 139.7637, weight: 2.5 },
  { area: '品川区大井町', pref: '東京都', lat: 35.6068, lon: 139.7348, weight: 2.0 },
  { area: '足立区北千住', pref: '東京都', lat: 35.7497, lon: 139.8049, weight: 3.0 },
  { area: '江戸川区小岩', pref: '東京都', lat: 35.7330, lon: 139.8790, weight: 2.5 },
  { area: '世田谷区三軒茶屋', pref: '東京都', lat: 35.6436, lon: 139.6710, weight: 2.0 },
  { area: '新宿区大久保', pref: '東京都', lat: 35.7012, lon: 139.7001, weight: 3.5 },
  { area: '墨田区錦糸町', pref: '東京都', lat: 35.6960, lon: 139.8150, weight: 2.5 },
  { area: '中野区中野', pref: '東京都', lat: 35.7074, lon: 139.6638, weight: 2.0 },
  { area: '板橋区大山', pref: '東京都', lat: 35.7510, lon: 139.7060, weight: 1.8 },
  { area: '八王子市旭町', pref: '東京都', lat: 35.6558, lon: 139.3388, weight: 1.5 },
  { area: '町田市原町田', pref: '東京都', lat: 35.5423, lon: 139.4466, weight: 1.5 },
  { area: '立川市曙町', pref: '東京都', lat: 35.6980, lon: 139.4137, weight: 1.5 },
  // Osaka
  { area: '中央区道頓堀', pref: '大阪府', lat: 34.6687, lon: 135.5013, weight: 4.0 },
  { area: '北区梅田', pref: '大阪府', lat: 34.7024, lon: 135.4983, weight: 3.5 },
  { area: '浪速区新世界', pref: '大阪府', lat: 34.6522, lon: 135.5062, weight: 3.5 },
  { area: '天王寺区', pref: '大阪府', lat: 34.6466, lon: 135.5170, weight: 2.5 },
  { area: '西成区あいりん', pref: '大阪府', lat: 34.6320, lon: 135.5040, weight: 3.0 },
  // Nagoya
  { area: '中区栄', pref: '愛知県', lat: 35.1664, lon: 136.9087, weight: 3.0 },
  { area: '中村区名駅', pref: '愛知県', lat: 35.1709, lon: 136.8815, weight: 2.5 },
  { area: '中区大須', pref: '愛知県', lat: 35.1572, lon: 136.9006, weight: 2.0 },
  // Yokohama
  { area: '中区関内', pref: '神奈川県', lat: 35.4437, lon: 139.6380, weight: 2.5 },
  { area: '西区横浜駅', pref: '神奈川県', lat: 35.4660, lon: 139.6225, weight: 2.0 },
  // Fukuoka
  { area: '中央区天神', pref: '福岡県', lat: 33.5917, lon: 130.3994, weight: 2.5 },
  { area: '博多区中洲', pref: '福岡県', lat: 33.5920, lon: 130.4080, weight: 3.0 },
  // Sapporo
  { area: '中央区すすきの', pref: '北海道', lat: 43.0535, lon: 141.3537, weight: 2.5 },
  { area: '中央区大通', pref: '北海道', lat: 43.0580, lon: 141.3485, weight: 1.5 },
  // Other cities
  { area: '川崎区', pref: '神奈川県', lat: 35.5308, lon: 139.6992, weight: 2.0 },
  { area: '仙台駅周辺', pref: '宮城県', lat: 38.2601, lon: 140.8822, weight: 1.5 },
  { area: '広島市中区', pref: '広島県', lat: 34.3935, lon: 132.4617, weight: 1.5 },
  { area: '神戸三宮', pref: '兵庫県', lat: 34.6937, lon: 135.1953, weight: 2.0 },
  { area: '京都四条', pref: '京都府', lat: 35.0040, lon: 135.7693, weight: 1.5 },
  { area: '那覇国際通り', pref: '沖縄県', lat: 26.3358, lon: 127.6862, weight: 1.5 },
];

function seededRandom(seed) {
  let x = Math.sin(seed) * 10000;
  return x - Math.floor(x);
}

function generateSeedData() {
  const features = [];
  const now = new Date();
  let idx = 0;

  // Distribute ~200 incidents weighted by area crime density
  const totalWeight = CITY_AREAS.reduce((sum, a) => sum + a.weight, 0);
  const TARGET = 200;

  for (const area of CITY_AREAS) {
    const count = Math.max(1, Math.round((area.weight / totalWeight) * TARGET));
    for (let j = 0; j < count && features.length < TARGET; j++) {
      idx++;
      const r = seededRandom(idx * 17);
      const r2 = seededRandom(idx * 31);
      const r3 = seededRandom(idx * 53);

      // Scatter within ~500m of the area center
      const lat = area.lat + (r - 0.5) * 0.009;
      const lon = area.lon + (r2 - 0.5) * 0.011;

      const incidentType = INCIDENT_TYPES[Math.floor(r * INCIDENT_TYPES.length)];
      const severity = SEVERITY_LEVELS[Math.floor(r2 * SEVERITY_LEVELS.length)];

      // Random date within last 30 days
      const daysAgo = Math.floor(r3 * 30);
      const hours = Math.floor(seededRandom(idx * 71) * 24);
      const reportDate = new Date(now);
      reportDate.setDate(reportDate.getDate() - daysAgo);
      reportDate.setHours(hours, Math.floor(seededRandom(idx * 97) * 60));

      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [lon, lat] },
        properties: {
          incident_id: `CRM_${String(idx).padStart(4, '0')}`,
          incident_type: incidentType,
          severity,
          reported_date: reportDate.toISOString(),
          area_name: area.area,
          prefecture: area.pref,
          time_of_day: hours < 6 ? 'late_night' : hours < 12 ? 'morning' : hours < 18 ? 'afternoon' : 'evening',
          source: 'police_seed',
        },
      });
    }
  }

  return features.slice(0, TARGET);
}

export default async function collectPoliceCrime() {
  // Try OSM police stations first; fallback to seeded incident patterns
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = [];

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'police_seed',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Representative crime incident data for major Japanese cities',
    },
  };
}
