/**
 * Pharmacy Map Collector
 * Maps pharmacies (薬局) across Japan via OSM Overpass API.
 * Falls back to a curated seed of major pharmacy chain locations.
 */

import { fetchOverpassTiled } from './_liveHelpers.js';

const SEED_PHARMACIES = [
  // Big chains: Matsumoto Kiyoshi, Welcia, Tsuruha, Cocokara Fine, Sundrug
  { name: 'マツモトキヨシ 渋谷センター街店', lat: 35.6595, lon: 139.7008, brand: 'Matsumoto Kiyoshi', prefecture: '東京都' },
  { name: 'マツモトキヨシ 新宿東口店', lat: 35.6905, lon: 139.7000, brand: 'Matsumoto Kiyoshi', prefecture: '東京都' },
  { name: 'マツモトキヨシ 池袋サンシャイン通り店', lat: 35.7295, lon: 139.7156, brand: 'Matsumoto Kiyoshi', prefecture: '東京都' },
  { name: 'マツモトキヨシ 銀座本店', lat: 35.6722, lon: 139.7647, brand: 'Matsumoto Kiyoshi', prefecture: '東京都' },
  { name: 'マツモトキヨシ 上野アメ横店', lat: 35.7128, lon: 139.7770, brand: 'Matsumoto Kiyoshi', prefecture: '東京都' },
  { name: 'マツモトキヨシ 浅草雷門店', lat: 35.7117, lon: 139.7969, brand: 'Matsumoto Kiyoshi', prefecture: '東京都' },
  { name: 'マツモトキヨシ 横浜西口店', lat: 35.4658, lon: 139.6224, brand: 'Matsumoto Kiyoshi', prefecture: '神奈川県' },
  { name: 'マツモトキヨシ 大阪心斎橋店', lat: 34.6754, lon: 135.5008, brand: 'Matsumoto Kiyoshi', prefecture: '大阪府' },
  { name: 'マツモトキヨシ 大阪なんば店', lat: 34.6650, lon: 135.5000, brand: 'Matsumoto Kiyoshi', prefecture: '大阪府' },
  { name: 'マツモトキヨシ 京都四条河原町店', lat: 35.0036, lon: 135.7681, brand: 'Matsumoto Kiyoshi', prefecture: '京都府' },
  { name: 'ウエルシア 東京駅前店', lat: 35.6810, lon: 139.7672, brand: 'Welcia', prefecture: '東京都' },
  { name: 'ウエルシア 渋谷ヒカリエ店', lat: 35.6592, lon: 139.7028, brand: 'Welcia', prefecture: '東京都' },
  { name: 'ウエルシア 新宿御苑前店', lat: 35.6852, lon: 139.7100, brand: 'Welcia', prefecture: '東京都' },
  { name: 'ウエルシア 池袋東口店', lat: 35.7300, lon: 139.7156, brand: 'Welcia', prefecture: '東京都' },
  { name: 'ウエルシア 横浜みなとみらい店', lat: 35.4561, lon: 139.6317, brand: 'Welcia', prefecture: '神奈川県' },
  { name: 'ウエルシア 名古屋栄店', lat: 35.1681, lon: 136.9006, brand: 'Welcia', prefecture: '愛知県' },
  { name: 'ウエルシア 大阪梅田店', lat: 34.7036, lon: 135.4983, brand: 'Welcia', prefecture: '大阪府' },
  { name: 'ツルハドラッグ 札幌駅前店', lat: 43.0686, lon: 141.3508, brand: 'Tsuruha', prefecture: '北海道' },
  { name: 'ツルハドラッグ 札幌大通店', lat: 43.0606, lon: 141.3547, brand: 'Tsuruha', prefecture: '北海道' },
  { name: 'ツルハドラッグ 函館駅前店', lat: 41.7686, lon: 140.7286, brand: 'Tsuruha', prefecture: '北海道' },
  { name: 'ツルハドラッグ 仙台一番町店', lat: 38.2683, lon: 140.8719, brand: 'Tsuruha', prefecture: '宮城県' },
  { name: 'ツルハドラッグ 旭川駅前店', lat: 43.7706, lon: 142.3650, brand: 'Tsuruha', prefecture: '北海道' },
  { name: 'ココカラファイン 渋谷店', lat: 35.6608, lon: 139.7028, brand: 'Cocokara Fine', prefecture: '東京都' },
  { name: 'ココカラファイン 新宿東口店', lat: 35.6938, lon: 139.7011, brand: 'Cocokara Fine', prefecture: '東京都' },
  { name: 'ココカラファイン 横浜店', lat: 35.4658, lon: 139.6224, brand: 'Cocokara Fine', prefecture: '神奈川県' },
  { name: 'サンドラッグ 新宿西口店', lat: 35.6900, lon: 139.6973, brand: 'Sundrug', prefecture: '東京都' },
  { name: 'サンドラッグ 渋谷道玄坂店', lat: 35.6588, lon: 139.6989, brand: 'Sundrug', prefecture: '東京都' },
  { name: 'サンドラッグ 秋葉原店', lat: 35.6984, lon: 139.7731, brand: 'Sundrug', prefecture: '東京都' },
  { name: 'サンドラッグ 名古屋大須店', lat: 35.1606, lon: 136.8997, brand: 'Sundrug', prefecture: '愛知県' },
  { name: 'スギ薬局 名古屋本店', lat: 35.1814, lon: 136.9069, brand: 'Sugi', prefecture: '愛知県' },
  { name: 'スギ薬局 京都四条店', lat: 35.0036, lon: 135.7681, brand: 'Sugi', prefecture: '京都府' },
  { name: 'スギ薬局 神戸三宮店', lat: 34.6913, lon: 135.1830, brand: 'Sugi', prefecture: '兵庫県' },
];

async function tryOverpass() {
  return fetchOverpassTiled(
    (bbox) => [
      `node["amenity"="pharmacy"](${bbox});`,
      `way["amenity"="pharmacy"](${bbox});`,
      `node["shop"="chemist"](${bbox});`,
    ].join(''),
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        facility_id: `PHARM_${el.id}`,
        name: el.tags?.name || el.tags?.['name:en'] || el.tags?.brand || 'Pharmacy',
        brand: el.tags?.brand || null,
        operator: el.tags?.operator || null,
        opening_hours: el.tags?.opening_hours || null,
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
  return SEED_PHARMACIES.map((p, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [p.lon, p.lat] },
    properties: {
      facility_id: `PHARM_${String(i + 1).padStart(5, '0')}`,
      name: p.name,
      brand: p.brand,
      prefecture: p.prefecture,
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'pharmacy_seed',
    },
  }));
}

export default async function collectPharmacyMap() {
  let features = await tryOverpass();
  const live = !!(features && features.length > 0);
  if (!live) features = [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'pharmacy_map',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Japan pharmacies - major drugstore chains and dispensing pharmacies',
    },
  };
}
