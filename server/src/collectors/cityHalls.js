/**
 * City Halls Collector
 * Tries OSM Overpass `amenity=townhall` (full nationwide via shared helper)
 * then falls back to ~80 major city/ward halls across Japan.
 */

import { fetchOverpass } from './_liveHelpers.js';

const SEED_CITY_HALLS = [
  // Tokyo 23 wards
  { name: '千代田区役所', lat: 35.6940, lon: 139.7536, kind: 'ward', pop: 67000 },
  { name: '中央区役所', lat: 35.6705, lon: 139.7720, kind: 'ward', pop: 170000 },
  { name: '港区役所', lat: 35.6580, lon: 139.7515, kind: 'ward', pop: 260000 },
  { name: '新宿区役所', lat: 35.6939, lon: 139.7036, kind: 'ward', pop: 348000 },
  { name: '文京区役所', lat: 35.7080, lon: 139.7525, kind: 'ward', pop: 240000 },
  { name: '台東区役所', lat: 35.7128, lon: 139.7800, kind: 'ward', pop: 211000 },
  { name: '墨田区役所', lat: 35.7106, lon: 139.8014, kind: 'ward', pop: 273000 },
  { name: '江東区役所', lat: 35.6731, lon: 139.8175, kind: 'ward', pop: 526000 },
  { name: '品川区役所', lat: 35.6092, lon: 139.7300, kind: 'ward', pop: 411000 },
  { name: '目黒区役所', lat: 35.6411, lon: 139.6982, kind: 'ward', pop: 287000 },
  { name: '大田区役所', lat: 35.5614, lon: 139.7161, kind: 'ward', pop: 740000 },
  { name: '世田谷区役所', lat: 35.6464, lon: 139.6533, kind: 'ward', pop: 920000 },
  { name: '渋谷区役所', lat: 35.6614, lon: 139.6975, kind: 'ward', pop: 230000 },
  { name: '中野区役所', lat: 35.7077, lon: 139.6650, kind: 'ward', pop: 335000 },
  { name: '杉並区役所', lat: 35.6996, lon: 139.6363, kind: 'ward', pop: 575000 },
  { name: '豊島区役所', lat: 35.7263, lon: 139.7165, kind: 'ward', pop: 290000 },
  { name: '北区役所', lat: 35.7531, lon: 139.7339, kind: 'ward', pop: 350000 },
  { name: '荒川区役所', lat: 35.7361, lon: 139.7831, kind: 'ward', pop: 217000 },
  { name: '板橋区役所', lat: 35.7511, lon: 139.7092, kind: 'ward', pop: 575000 },
  { name: '練馬区役所', lat: 35.7358, lon: 139.6517, kind: 'ward', pop: 740000 },
  { name: '足立区役所', lat: 35.7758, lon: 139.8047, kind: 'ward', pop: 695000 },
  { name: '葛飾区役所', lat: 35.7434, lon: 139.8475, kind: 'ward', pop: 463000 },
  { name: '江戸川区役所', lat: 35.7064, lon: 139.8683, kind: 'ward', pop: 696000 },
  // Designated cities
  { name: '横浜市役所', lat: 35.4478, lon: 139.6425, kind: 'designated_city', pop: 3770000 },
  { name: '川崎市役所', lat: 35.5308, lon: 139.7028, kind: 'designated_city', pop: 1540000 },
  { name: '相模原市役所', lat: 35.5713, lon: 139.3729, kind: 'designated_city', pop: 720000 },
  { name: '名古屋市役所', lat: 35.1814, lon: 136.9067, kind: 'designated_city', pop: 2330000 },
  { name: '大阪市役所', lat: 34.6937, lon: 135.5023, kind: 'designated_city', pop: 2750000 },
  { name: '堺市役所', lat: 34.5733, lon: 135.4828, kind: 'designated_city', pop: 825000 },
  { name: '京都市役所', lat: 35.0114, lon: 135.7681, kind: 'designated_city', pop: 1465000 },
  { name: '神戸市役所', lat: 34.6913, lon: 135.1830, kind: 'designated_city', pop: 1525000 },
  { name: '札幌市役所', lat: 43.0640, lon: 141.3469, kind: 'designated_city', pop: 1965000 },
  { name: '仙台市役所', lat: 38.2682, lon: 140.8721, kind: 'designated_city', pop: 1090000 },
  { name: 'さいたま市役所', lat: 35.8617, lon: 139.6455, kind: 'designated_city', pop: 1320000 },
  { name: '千葉市役所', lat: 35.6075, lon: 140.1064, kind: 'designated_city', pop: 980000 },
  { name: '新潟市役所', lat: 37.9161, lon: 139.0364, kind: 'designated_city', pop: 800000 },
  { name: '静岡市役所', lat: 34.9756, lon: 138.3828, kind: 'designated_city', pop: 705000 },
  { name: '浜松市役所', lat: 34.7108, lon: 137.7261, kind: 'designated_city', pop: 800000 },
  { name: '岡山市役所', lat: 34.6553, lon: 133.9192, kind: 'designated_city', pop: 720000 },
  { name: '広島市役所', lat: 34.3963, lon: 132.4596, kind: 'designated_city', pop: 1200000 },
  { name: '北九州市役所', lat: 33.8836, lon: 130.8814, kind: 'designated_city', pop: 945000 },
  { name: '福岡市役所', lat: 33.5904, lon: 130.4017, kind: 'designated_city', pop: 1610000 },
  { name: '熊本市役所', lat: 32.8033, lon: 130.7081, kind: 'designated_city', pop: 740000 },
  // Core cities
  { name: '宇都宮市役所', lat: 36.5552, lon: 139.8828, kind: 'core_city', pop: 520000 },
  { name: '前橋市役所', lat: 36.3895, lon: 139.0635, kind: 'core_city', pop: 335000 },
  { name: '高崎市役所', lat: 36.3220, lon: 139.0030, kind: 'core_city', pop: 372000 },
  { name: '水戸市役所', lat: 36.3658, lon: 140.4714, kind: 'core_city', pop: 271000 },
  { name: '長野市役所', lat: 36.6485, lon: 138.1948, kind: 'core_city', pop: 372000 },
  { name: '富山市役所', lat: 36.6953, lon: 137.2113, kind: 'core_city', pop: 410000 },
  { name: '金沢市役所', lat: 36.5613, lon: 136.6562, kind: 'core_city', pop: 460000 },
  { name: '福井市役所', lat: 36.0648, lon: 136.2222, kind: 'core_city', pop: 263000 },
  { name: '岐阜市役所', lat: 35.4233, lon: 136.7606, kind: 'core_city', pop: 405000 },
  { name: '津市役所', lat: 34.7186, lon: 136.5057, kind: 'core_city', pop: 274000 },
  { name: '大津市役所', lat: 35.0048, lon: 135.8686, kind: 'core_city', pop: 343000 },
  { name: '奈良市役所', lat: 34.6852, lon: 135.8050, kind: 'core_city', pop: 354000 },
  { name: '和歌山市役所', lat: 34.2261, lon: 135.1675, kind: 'core_city', pop: 357000 },
  { name: '鳥取市役所', lat: 35.5011, lon: 134.2350, kind: 'core_city', pop: 188000 },
  { name: '松江市役所', lat: 35.4683, lon: 133.0481, kind: 'core_city', pop: 199000 },
  { name: '高松市役所', lat: 34.3431, lon: 134.0467, kind: 'core_city', pop: 422000 },
  { name: '松山市役所', lat: 33.8392, lon: 132.7656, kind: 'core_city', pop: 510000 },
  { name: '高知市役所', lat: 33.5597, lon: 133.5311, kind: 'core_city', pop: 327000 },
  { name: '徳島市役所', lat: 34.0707, lon: 134.5547, kind: 'core_city', pop: 254000 },
  { name: '長崎市役所', lat: 32.7497, lon: 129.8775, kind: 'core_city', pop: 410000 },
  { name: '佐賀市役所', lat: 33.2494, lon: 130.2989, kind: 'core_city', pop: 232000 },
  { name: '大分市役所', lat: 33.2382, lon: 131.6126, kind: 'core_city', pop: 478000 },
  { name: '宮崎市役所', lat: 31.9077, lon: 131.4202, kind: 'core_city', pop: 401000 },
  { name: '鹿児島市役所', lat: 31.5602, lon: 130.5581, kind: 'core_city', pop: 595000 },
  { name: '那覇市役所', lat: 26.2125, lon: 127.6809, kind: 'core_city', pop: 318000 },
  { name: '青森市役所', lat: 40.8222, lon: 140.7475, kind: 'core_city', pop: 280000 },
  { name: '盛岡市役所', lat: 39.7036, lon: 141.1525, kind: 'core_city', pop: 287000 },
  { name: '秋田市役所', lat: 39.7186, lon: 140.1024, kind: 'core_city', pop: 305000 },
  { name: '山形市役所', lat: 38.2406, lon: 140.3631, kind: 'core_city', pop: 245000 },
  { name: '福島市役所', lat: 37.7608, lon: 140.4736, kind: 'core_city', pop: 282000 },
  { name: '郡山市役所', lat: 37.4006, lon: 140.3597, kind: 'core_city', pop: 322000 },
  { name: 'いわき市役所', lat: 36.9447, lon: 140.8881, kind: 'core_city', pop: 327000 },
  { name: '函館市役所', lat: 41.7686, lon: 140.7289, kind: 'core_city', pop: 247000 },
  { name: '旭川市役所', lat: 43.7706, lon: 142.3650, kind: 'core_city', pop: 327000 },
  { name: '甲府市役所', lat: 35.6622, lon: 138.5683, kind: 'core_city', pop: 187000 },
];

async function tryOverpass() {
  return fetchOverpass(
    [
      'node["amenity"="townhall"](area.jp);',
      'way["amenity"="townhall"](area.jp);',
      'node["office"="government"]["government"~"municipal|prefecture|cabinet"](area.jp);',
    ].join(''),
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        hall_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || 'City Hall',
        name_ja: el.tags?.name || null,
        kind: el.tags?.amenity === 'townhall' ? 'townhall' : (el.tags?.government || 'government_office'),
        operator: el.tags?.operator || null,
        addr: el.tags?.['addr:full'] || el.tags?.['addr:city'] || null,
        source: 'osm_overpass',
      },
    }),
    60_000,
    { limit: 0, queryTimeout: 180 },
  );
}

function generateSeedData() {
  return SEED_CITY_HALLS.map((h, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [h.lon, h.lat] },
    properties: {
      hall_id: `HALL_${String(i + 1).padStart(5, '0')}`,
      name: h.name,
      kind: h.kind,
      population: h.pop,
      source: 'city_halls_seed',
    },
  }));
}

export default async function collectCityHalls() {
  let features = await tryOverpass();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'city_halls',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Tokyo wards, designated cities, core cities and prefectural capital halls',
    },
    metadata: {},
  };
}
