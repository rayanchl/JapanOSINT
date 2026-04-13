/**
 * Koban (Police Box) Map Collector
 * Maps Japanese police boxes (交番) and police stations via OSM Overpass API.
 * Falls back to a curated seed of major prefectural HQs and notable koban.
 */

import { fetchOverpassTiled } from './_liveHelpers.js';

const SEED_KOBAN = [
  { name: '警視庁本部', lat: 35.6789, lon: 139.7547, type: 'headquarters', prefecture: '東京都' },
  { name: '丸の内警察署', lat: 35.6792, lon: 139.7639, type: 'station', prefecture: '東京都' },
  { name: '渋谷警察署', lat: 35.6608, lon: 139.7028, type: 'station', prefecture: '東京都' },
  { name: '新宿警察署', lat: 35.6938, lon: 139.6993, type: 'station', prefecture: '東京都' },
  { name: '原宿警察署', lat: 35.6716, lon: 139.7081, type: 'station', prefecture: '東京都' },
  { name: '麻布警察署', lat: 35.6592, lon: 139.7311, type: 'station', prefecture: '東京都' },
  { name: '上野警察署', lat: 35.7110, lon: 139.7780, type: 'station', prefecture: '東京都' },
  { name: '池袋警察署', lat: 35.7295, lon: 139.7125, type: 'station', prefecture: '東京都' },
  { name: '築地警察署', lat: 35.6650, lon: 139.7715, type: 'station', prefecture: '東京都' },
  { name: '渋谷駅前交番', lat: 35.6595, lon: 139.7008, type: 'koban', prefecture: '東京都' },
  { name: '新宿駅前交番', lat: 35.6905, lon: 139.7000, type: 'koban', prefecture: '東京都' },
  { name: '東京駅前交番', lat: 35.6810, lon: 139.7672, type: 'koban', prefecture: '東京都' },
  { name: '神奈川県警察本部', lat: 35.4475, lon: 139.6394, type: 'headquarters', prefecture: '神奈川県' },
  { name: '横浜市中区警察署', lat: 35.4458, lon: 139.6411, type: 'station', prefecture: '神奈川県' },
  { name: '川崎警察署', lat: 35.5311, lon: 139.7036, type: 'station', prefecture: '神奈川県' },
  { name: '大阪府警察本部', lat: 34.6864, lon: 135.5197, type: 'headquarters', prefecture: '大阪府' },
  { name: '大阪市北警察署', lat: 34.7036, lon: 135.4983, type: 'station', prefecture: '大阪府' },
  { name: '大阪市中央警察署', lat: 34.6866, lon: 135.5097, type: 'station', prefecture: '大阪府' },
  { name: '京都府警察本部', lat: 35.0211, lon: 135.7681, type: 'headquarters', prefecture: '京都府' },
  { name: '中京警察署', lat: 35.0094, lon: 135.7639, type: 'station', prefecture: '京都府' },
  { name: '愛知県警察本部', lat: 35.1814, lon: 136.9069, type: 'headquarters', prefecture: '愛知県' },
  { name: '名古屋中警察署', lat: 35.1681, lon: 136.9006, type: 'station', prefecture: '愛知県' },
  { name: '北海道警察本部', lat: 43.0628, lon: 141.3478, type: 'headquarters', prefecture: '北海道' },
  { name: '札幌中央警察署', lat: 43.0556, lon: 141.3522, type: 'station', prefecture: '北海道' },
  { name: '兵庫県警察本部', lat: 34.6919, lon: 135.1831, type: 'headquarters', prefecture: '兵庫県' },
  { name: '宮城県警察本部', lat: 38.2683, lon: 140.8719, type: 'headquarters', prefecture: '宮城県' },
  { name: '福岡県警察本部', lat: 33.6064, lon: 130.4181, type: 'headquarters', prefecture: '福岡県' },
  { name: '広島県警察本部', lat: 34.3964, lon: 132.4594, type: 'headquarters', prefecture: '広島県' },
  { name: '沖縄県警察本部', lat: 26.2125, lon: 127.6809, type: 'headquarters', prefecture: '沖縄県' },
  { name: '千葉県警察本部', lat: 35.6083, lon: 140.1233, type: 'headquarters', prefecture: '千葉県' },
  { name: '埼玉県警察本部', lat: 35.8569, lon: 139.6489, type: 'headquarters', prefecture: '埼玉県' },
];

async function tryOverpass() {
  return fetchOverpassTiled(
    (bbox) => [
      `node["amenity"="police"](${bbox});`,
      `way["amenity"="police"](${bbox});`,
    ].join(''),
    (el, _i, coords) => {
      const name = el.tags?.name || el.tags?.['name:en'] || 'Police';
      const ptype = /交番|koban/i.test(name) ? 'koban' :
                    /本部|headquarters/i.test(name) ? 'headquarters' :
                    /駐在所/.test(name) ? 'chuzaisho' : 'station';
      return {
        type: 'Feature',
        geometry: { type: 'Point', coordinates: coords },
        properties: {
          facility_id: `KOBAN_${el.id}`,
          name,
          police_type: ptype,
          operator: el.tags?.operator || null,
          phone: el.tags?.phone || null,
          opening_hours: el.tags?.opening_hours || '24/7',
          country: 'JP',
          source: 'osm_overpass',
        },
      };
    },
    { queryTimeout: 180, timeoutMs: 90_000 },
  );
}

function generateSeedData() {
  const now = new Date();
  return SEED_KOBAN.map((k, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [k.lon, k.lat] },
    properties: {
      facility_id: `KOBAN_${String(i + 1).padStart(5, '0')}`,
      name: k.name,
      police_type: k.type,
      prefecture: k.prefecture,
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'koban_seed',
    },
  }));
}

export default async function collectKobanMap() {
  let features = await tryOverpass();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'koban_map',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Japan police boxes (koban) and police stations',
    },
    metadata: {},
  };
}
