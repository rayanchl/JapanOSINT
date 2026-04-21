/**
 * Government Buildings Collector
 * Tries OSM Overpass `office=government` then falls back to seed of major
 * Japanese government buildings (cabinet, Diet, ministries).
 */

import { fetchOverpass } from './_liveHelpers.js';

const SEED_GOV_BUILDINGS = [
  // Tokyo: Cabinet & Diet & Imperial
  { name: '首相官邸', name_en: 'Prime Ministers Office (Kantei)', lat: 35.6735, lon: 139.7437, kind: 'cabinet' },
  { name: '国会議事堂', name_en: 'National Diet Building', lat: 35.6759, lon: 139.7448, kind: 'parliament' },
  { name: '皇居', name_en: 'Imperial Palace', lat: 35.6852, lon: 139.7528, kind: 'imperial' },
  { name: '最高裁判所', name_en: 'Supreme Court of Japan', lat: 35.6782, lon: 139.7434, kind: 'judiciary' },
  // Ministries (Kasumigaseki)
  { name: '外務省', name_en: 'Ministry of Foreign Affairs', lat: 35.6748, lon: 139.7517, kind: 'ministry' },
  { name: '財務省', name_en: 'Ministry of Finance', lat: 35.6748, lon: 139.7515, kind: 'ministry' },
  { name: '経済産業省', name_en: 'Ministry of Economy Trade Industry', lat: 35.6727, lon: 139.7536, kind: 'ministry' },
  { name: '総務省', name_en: 'Ministry of Internal Affairs Communications', lat: 35.6745, lon: 139.7522, kind: 'ministry' },
  { name: '法務省', name_en: 'Ministry of Justice', lat: 35.6786, lon: 139.7536, kind: 'ministry' },
  { name: '文部科学省', name_en: 'Ministry of Education MEXT', lat: 35.6750, lon: 139.7531, kind: 'ministry' },
  { name: '厚生労働省', name_en: 'Ministry of Health Labour Welfare', lat: 35.6738, lon: 139.7531, kind: 'ministry' },
  { name: '農林水産省', name_en: 'Ministry of Agriculture MAFF', lat: 35.6742, lon: 139.7531, kind: 'ministry' },
  { name: '国土交通省', name_en: 'Ministry of Land Infrastructure MLIT', lat: 35.6736, lon: 139.7531, kind: 'ministry' },
  { name: '環境省', name_en: 'Ministry of Environment', lat: 35.6736, lon: 139.7536, kind: 'ministry' },
  { name: '防衛省', name_en: 'Ministry of Defense (Ichigaya)', lat: 35.6926, lon: 139.7270, kind: 'ministry' },
  { name: 'デジタル庁', name_en: 'Digital Agency', lat: 35.6750, lon: 139.7595, kind: 'ministry' },
  // Agencies
  { name: '警察庁', name_en: 'National Police Agency', lat: 35.6745, lon: 139.7531, kind: 'agency' },
  { name: '国税庁', name_en: 'National Tax Agency', lat: 35.6748, lon: 139.7515, kind: 'agency' },
  { name: '気象庁', name_en: 'Japan Meteorological Agency', lat: 35.6906, lon: 139.7553, kind: 'agency' },
  { name: '海上保安庁', name_en: 'Japan Coast Guard HQ', lat: 35.6736, lon: 139.7531, kind: 'agency' },
  { name: '消防庁', name_en: 'Fire Defence Agency', lat: 35.6745, lon: 139.7522, kind: 'agency' },
  { name: '特許庁', name_en: 'Japan Patent Office', lat: 35.6727, lon: 139.7536, kind: 'agency' },
  { name: '林野庁', name_en: 'Forestry Agency', lat: 35.6742, lon: 139.7531, kind: 'agency' },
  { name: '水産庁', name_en: 'Fisheries Agency', lat: 35.6742, lon: 139.7531, kind: 'agency' },
  { name: '観光庁', name_en: 'Japan Tourism Agency', lat: 35.6736, lon: 139.7531, kind: 'agency' },
  { name: '原子力規制委員会', name_en: 'Nuclear Regulation Authority', lat: 35.6850, lon: 139.7280, kind: 'agency' },
  { name: '宇宙航空研究開発機構 JAXA本社', name_en: 'JAXA HQ', lat: 35.7100, lon: 139.4820, kind: 'agency' },
  // Prefectural government HQ (selected)
  { name: '東京都庁', name_en: 'Tokyo Metropolitan Govt', lat: 35.6896, lon: 139.6917, kind: 'prefectural' },
  { name: '大阪府庁', name_en: 'Osaka Prefectural Govt', lat: 34.6863, lon: 135.5200, kind: 'prefectural' },
  { name: '愛知県庁', name_en: 'Aichi Prefectural Govt', lat: 35.1814, lon: 136.9067, kind: 'prefectural' },
  { name: '神奈川県庁', name_en: 'Kanagawa Prefectural Govt', lat: 35.4478, lon: 139.6425, kind: 'prefectural' },
  { name: '北海道庁', name_en: 'Hokkaido Prefectural Govt', lat: 43.0640, lon: 141.3469, kind: 'prefectural' },
  { name: '福岡県庁', name_en: 'Fukuoka Prefectural Govt', lat: 33.6064, lon: 130.4181, kind: 'prefectural' },
  { name: '京都府庁', name_en: 'Kyoto Prefectural Govt', lat: 35.0214, lon: 135.7556, kind: 'prefectural' },
  { name: '兵庫県庁', name_en: 'Hyogo Prefectural Govt', lat: 34.6913, lon: 135.1830, kind: 'prefectural' },
  { name: '宮城県庁', name_en: 'Miyagi Prefectural Govt', lat: 38.2682, lon: 140.8721, kind: 'prefectural' },
  { name: '広島県庁', name_en: 'Hiroshima Prefectural Govt', lat: 34.3963, lon: 132.4596, kind: 'prefectural' },
  { name: '沖縄県庁', name_en: 'Okinawa Prefectural Govt', lat: 26.2124, lon: 127.6809, kind: 'prefectural' },
  // Bank of Japan
  { name: '日本銀行 本店', name_en: 'Bank of Japan HQ', lat: 35.6863, lon: 139.7728, kind: 'central_bank' },
  { name: '国立印刷局', name_en: 'National Printing Bureau', lat: 35.6916, lon: 139.7461, kind: 'agency' },
  { name: '造幣局 大阪', name_en: 'Mint Bureau Osaka', lat: 34.6981, lon: 135.5147, kind: 'agency' },
];

async function tryOverpass() {
  return fetchOverpass(
    'node["office"="government"](area.jp);way["office"="government"](area.jp);',
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        building_id: `OSM_GOV_${el.id}`,
        name: el.tags?.name || el.tags?.['name:en'] || 'Government Office',
        kind: el.tags?.government || 'government',
        source: 'osm_overpass',
      },
    }),
  );
}

function generateSeedData() {
  return SEED_GOV_BUILDINGS.map((b, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [b.lon, b.lat] },
    properties: {
      building_id: `GOV_${String(i + 1).padStart(5, '0')}`,
      name: b.name,
      name_en: b.name_en,
      kind: b.kind,
      country: 'JP',
      source: 'gov_buildings_seed',
    },
  }));
}

export default async function collectGovernmentBuildings() {
  let features = await tryOverpass();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'government_buildings',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Japanese government buildings: cabinet, Diet, ministries, agencies, prefectural HQ',
    },
    metadata: {},
  };
}
