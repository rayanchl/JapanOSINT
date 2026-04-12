/**
 * Courts and Prisons Collector
 * MOJ facility list + OSM Overpass amenity=courthouse, amenity=prison.
 * Falls back to seed of supreme/high/district courts and major correctional facilities.
 */

const OVERPASS_URL = 'https://overpass-api.de/api/interpreter';

const SEED_FACILITIES = [
  // High courts (8)
  { name: '東京高等裁判所', lat: 35.6782, lon: 139.7434, type: 'high_court', capacity: 0 },
  { name: '大阪高等裁判所', lat: 34.6912, lon: 135.5208, type: 'high_court', capacity: 0 },
  { name: '名古屋高等裁判所', lat: 35.1814, lon: 136.9067, type: 'high_court', capacity: 0 },
  { name: '広島高等裁判所', lat: 34.3963, lon: 132.4596, type: 'high_court', capacity: 0 },
  { name: '福岡高等裁判所', lat: 33.5904, lon: 130.4017, type: 'high_court', capacity: 0 },
  { name: '仙台高等裁判所', lat: 38.2682, lon: 140.8721, type: 'high_court', capacity: 0 },
  { name: '札幌高等裁判所', lat: 43.0640, lon: 141.3469, type: 'high_court', capacity: 0 },
  { name: '高松高等裁判所', lat: 34.3431, lon: 134.0467, type: 'high_court', capacity: 0 },
  // District courts (50, sample)
  { name: '東京地方裁判所', lat: 35.6782, lon: 139.7430, type: 'district_court', capacity: 0 },
  { name: '横浜地方裁判所', lat: 35.4478, lon: 139.6425, type: 'district_court', capacity: 0 },
  { name: '千葉地方裁判所', lat: 35.6075, lon: 140.1064, type: 'district_court', capacity: 0 },
  { name: 'さいたま地方裁判所', lat: 35.8617, lon: 139.6455, type: 'district_court', capacity: 0 },
  { name: '京都地方裁判所', lat: 35.0114, lon: 135.7681, type: 'district_court', capacity: 0 },
  { name: '神戸地方裁判所', lat: 34.6913, lon: 135.1830, type: 'district_court', capacity: 0 },
  { name: '広島地方裁判所', lat: 34.3963, lon: 132.4596, type: 'district_court', capacity: 0 },
  { name: '岡山地方裁判所', lat: 34.6553, lon: 133.9192, type: 'district_court', capacity: 0 },
  { name: '福岡地方裁判所', lat: 33.5904, lon: 130.4017, type: 'district_court', capacity: 0 },
  { name: '熊本地方裁判所', lat: 32.8033, lon: 130.7081, type: 'district_court', capacity: 0 },
  { name: '長崎地方裁判所', lat: 32.7497, lon: 129.8775, type: 'district_court', capacity: 0 },
  { name: '鹿児島地方裁判所', lat: 31.5602, lon: 130.5581, type: 'district_court', capacity: 0 },
  { name: '那覇地方裁判所', lat: 26.2125, lon: 127.6809, type: 'district_court', capacity: 0 },
  { name: '札幌地方裁判所', lat: 43.0640, lon: 141.3469, type: 'district_court', capacity: 0 },
  { name: '函館地方裁判所', lat: 41.7686, lon: 140.7289, type: 'district_court', capacity: 0 },
  { name: '仙台地方裁判所', lat: 38.2682, lon: 140.8721, type: 'district_court', capacity: 0 },
  { name: '青森地方裁判所', lat: 40.8222, lon: 140.7475, type: 'district_court', capacity: 0 },
  { name: '盛岡地方裁判所', lat: 39.7036, lon: 141.1525, type: 'district_court', capacity: 0 },
  { name: '秋田地方裁判所', lat: 39.7186, lon: 140.1024, type: 'district_court', capacity: 0 },
  // Prisons (~25 major correctional facilities)
  { name: '府中刑務所', lat: 35.6700, lon: 139.4817, type: 'prison', capacity: 2900 },
  { name: '東京拘置所', lat: 35.7572, lon: 139.7889, type: 'detention', capacity: 2950 },
  { name: '横浜刑務所', lat: 35.4517, lon: 139.6178, type: 'prison', capacity: 1700 },
  { name: '千葉刑務所', lat: 35.6500, lon: 140.1167, type: 'prison', capacity: 1500 },
  { name: '川越少年刑務所', lat: 35.9117, lon: 139.4825, type: 'juvenile_prison', capacity: 1400 },
  { name: '大阪刑務所', lat: 34.6433, lon: 135.5733, type: 'prison', capacity: 2700 },
  { name: '京都刑務所', lat: 34.9533, lon: 135.7600, type: 'prison', capacity: 1100 },
  { name: '神戸刑務所', lat: 34.7467, lon: 135.1192, type: 'prison', capacity: 1600 },
  { name: '名古屋刑務所', lat: 35.0497, lon: 137.0192, type: 'prison', capacity: 2700 },
  { name: '岐阜刑務所', lat: 35.4017, lon: 136.7406, type: 'prison', capacity: 800 },
  { name: '岡崎医療刑務所', lat: 34.9542, lon: 137.1731, type: 'medical_prison', capacity: 600 },
  { name: '広島刑務所', lat: 34.3897, lon: 132.4669, type: 'prison', capacity: 1600 },
  { name: '岡山刑務所', lat: 34.6500, lon: 133.9275, type: 'prison', capacity: 700 },
  { name: '高松刑務所', lat: 34.3406, lon: 134.0472, type: 'prison', capacity: 700 },
  { name: '松山刑務所', lat: 33.8275, lon: 132.7681, type: 'prison', capacity: 700 },
  { name: '福岡刑務所', lat: 33.5894, lon: 130.4036, type: 'prison', capacity: 1400 },
  { name: '熊本刑務所', lat: 32.7900, lon: 130.7128, type: 'prison', capacity: 800 },
  { name: '長崎刑務所', lat: 32.7522, lon: 129.8850, type: 'prison', capacity: 600 },
  { name: '鹿児島刑務所', lat: 31.5572, lon: 130.5589, type: 'prison', capacity: 700 },
  { name: '佐賀少年刑務所', lat: 33.2497, lon: 130.3050, type: 'juvenile_prison', capacity: 600 },
  { name: '宮城刑務所', lat: 38.2611, lon: 140.8689, type: 'prison', capacity: 1100 },
  { name: '秋田刑務所', lat: 39.7197, lon: 140.1058, type: 'prison', capacity: 700 },
  { name: '山形刑務所', lat: 38.2425, lon: 140.3617, type: 'prison', capacity: 500 },
  { name: '札幌刑務所', lat: 43.0833, lon: 141.4117, type: 'prison', capacity: 1700 },
  { name: '網走刑務所', lat: 44.0072, lon: 144.2522, type: 'prison', capacity: 1200 },
  { name: '旭川刑務所', lat: 43.7700, lon: 142.3608, type: 'prison', capacity: 700 },
  { name: '帯広刑務所', lat: 42.9233, lon: 143.1958, type: 'prison', capacity: 600 },
  { name: '沖縄刑務所', lat: 26.2125, lon: 127.6817, type: 'prison', capacity: 700 },
];

async function tryOverpass() {
  const query = `[out:json][timeout:25];area["ISO3166-1"="JP"]->.jp;(node["amenity"="courthouse"](area.jp);node["amenity"="prison"](area.jp););out 400;`;
  try {
    const ctrl = new AbortController();
    const timeout = setTimeout(() => ctrl.abort(), 12000);
    const res = await fetch(OVERPASS_URL, {
      method: 'POST',
      signal: ctrl.signal,
      headers: { 'Content-Type': 'text/plain' },
      body: query,
    });
    clearTimeout(timeout);
    if (!res.ok) return null;
    const data = await res.json();
    if (!data?.elements?.length) return null;
    return data.elements
      .map((el) => {
        const lat = el.lat ?? el.center?.lat;
        const lon = el.lon ?? el.center?.lon;
        if (lat == null || lon == null) return null;
        return {
          type: 'Feature',
          geometry: { type: 'Point', coordinates: [lon, lat] },
          properties: {
            facility_id: `OSM_${el.id}`,
            name: el.tags?.name || 'Court / Prison',
            type: el.tags?.amenity === 'prison' ? 'prison' : 'courthouse',
            source: 'osm_overpass',
          },
        };
      })
      .filter(Boolean);
  } catch {
    return null;
  }
}

function generateSeedData() {
  return SEED_FACILITIES.map((f, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [f.lon, f.lat] },
    properties: {
      facility_id: `MOJ_${String(i + 1).padStart(5, '0')}`,
      name: f.name,
      type: f.type,
      capacity: f.capacity,
      source: 'moj_seed',
    },
  }));
}

export default async function collectCourtsPrisons() {
  let features = await tryOverpass();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'courts_prisons',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Japanese high/district courts and major correctional facilities',
    },
    metadata: {},
  };
}
