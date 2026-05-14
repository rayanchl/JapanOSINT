/**
 * Fire Station Map Collector
 * Maps Japanese fire stations (消防署) via OSM Overpass API.
 * Falls back to a curated seed of major prefectural fire HQs.
 */

import { fetchOverpassTiled } from './_liveHelpers.js';

const SEED_FIRE_STATIONS = [
  { name: '東京消防庁本庁', lat: 35.6919, lon: 139.7547, type: 'headquarters', prefecture: '東京都' },
  { name: '丸の内消防署', lat: 35.6803, lon: 139.7647, type: 'station', prefecture: '東京都' },
  { name: '麹町消防署', lat: 35.6850, lon: 139.7397, type: 'station', prefecture: '東京都' },
  { name: '神田消防署', lat: 35.6925, lon: 139.7717, type: 'station', prefecture: '東京都' },
  { name: '本郷消防署', lat: 35.7067, lon: 139.7611, type: 'station', prefecture: '東京都' },
  { name: '渋谷消防署', lat: 35.6647, lon: 139.7053, type: 'station', prefecture: '東京都' },
  { name: '新宿消防署', lat: 35.6928, lon: 139.7039, type: 'station', prefecture: '東京都' },
  { name: '池袋消防署', lat: 35.7314, lon: 139.7156, type: 'station', prefecture: '東京都' },
  { name: '上野消防署', lat: 35.7128, lon: 139.7780, type: 'station', prefecture: '東京都' },
  { name: '麻布消防署', lat: 35.6592, lon: 139.7311, type: 'station', prefecture: '東京都' },
  { name: '芝消防署', lat: 35.6586, lon: 139.7494, type: 'station', prefecture: '東京都' },
  { name: '横浜市消防局', lat: 35.4486, lon: 139.6431, type: 'headquarters', prefecture: '神奈川県' },
  { name: '川崎市消防局', lat: 35.5311, lon: 139.7036, type: 'headquarters', prefecture: '神奈川県' },
  { name: '大阪市消防局', lat: 34.6864, lon: 135.5197, type: 'headquarters', prefecture: '大阪府' },
  { name: '京都市消防局', lat: 35.0094, lon: 135.7639, type: 'headquarters', prefecture: '京都府' },
  { name: '名古屋市消防局', lat: 35.1814, lon: 136.9069, type: 'headquarters', prefecture: '愛知県' },
  { name: '札幌市消防局', lat: 43.0628, lon: 141.3478, type: 'headquarters', prefecture: '北海道' },
  { name: '神戸市消防局', lat: 34.6919, lon: 135.1831, type: 'headquarters', prefecture: '兵庫県' },
  { name: '仙台市消防局', lat: 38.2683, lon: 140.8719, type: 'headquarters', prefecture: '宮城県' },
  { name: '福岡市消防局', lat: 33.5897, lon: 130.4017, type: 'headquarters', prefecture: '福岡県' },
  { name: '広島市消防局', lat: 34.3964, lon: 132.4594, type: 'headquarters', prefecture: '広島県' },
  { name: '北九州市消防局', lat: 33.8836, lon: 130.8814, type: 'headquarters', prefecture: '福岡県' },
  { name: '千葉市消防局', lat: 35.6083, lon: 140.1233, type: 'headquarters', prefecture: '千葉県' },
  { name: 'さいたま市消防局', lat: 35.8569, lon: 139.6489, type: 'headquarters', prefecture: '埼玉県' },
  { name: '静岡市消防局', lat: 34.9756, lon: 138.3828, type: 'headquarters', prefecture: '静岡県' },
  { name: '岡山市消防局', lat: 34.6628, lon: 133.9197, type: 'headquarters', prefecture: '岡山県' },
  { name: '熊本市消防局', lat: 32.8019, lon: 130.7256, type: 'headquarters', prefecture: '熊本県' },
  { name: '鹿児島市消防局', lat: 31.5963, lon: 130.5571, type: 'headquarters', prefecture: '鹿児島県' },
  { name: '那覇市消防局', lat: 26.2150, lon: 127.6792, type: 'headquarters', prefecture: '沖縄県' },
  { name: '長崎市消防局', lat: 32.7503, lon: 129.8775, type: 'headquarters', prefecture: '長崎県' },
];

async function tryOverpass() {
  return fetchOverpassTiled(
    (bbox) => [
      `node["amenity"="fire_station"](${bbox});`,
      `way["amenity"="fire_station"](${bbox});`,
    ].join(''),
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        facility_id: `FIRE_${el.id}`,
        name: el.tags?.name || el.tags?.['name:en'] || 'Fire Station',
        operator: el.tags?.operator || null,
        phone: el.tags?.phone || null,
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
    { queryTimeout: 180, timeoutMs: 90_000 },
  );
}

function generateSeedData() {
  const now = new Date();
  return SEED_FIRE_STATIONS.map((f, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [f.lon, f.lat] },
    properties: {
      facility_id: `FIRE_${String(i + 1).padStart(5, '0')}`,
      name: f.name,
      station_type: f.type,
      prefecture: f.prefecture,
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'fire_station_seed',
    },
  }));
}

export default async function collectFireStationMap() {
  let features = await tryOverpass();
  const live = !!(features && features.length > 0);
  if (!live) features = [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'fire_station_map',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Japan fire stations and fire department headquarters',
    },
  };
}
