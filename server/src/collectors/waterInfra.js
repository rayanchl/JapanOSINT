/**
 * Water Infrastructure Collector
 * Maps water infrastructure across Japan:
 * - Major dams and reservoirs
 * - Water treatment plants (浄水場)
 * - Sewage treatment plants (下水処理場)
 * - Major aqueducts
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return await fetchOverpass(
    'node["man_made"="water_works"](area.jp);way["man_made"="water_works"](area.jp);node["man_made"="water_tower"](area.jp);way["landuse"="reservoir"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        facility_id: `WTR_LIVE_${String(i + 1).padStart(4, '0')}`,
        name: el.tags?.name || el.tags?.['name:en'] || `Water facility ${el.id}`,
        operator: el.tags?.operator || 'unknown',
        facility_type: el.tags?.man_made || el.tags?.landuse || 'water',
        country: 'JP',
        updated_at: new Date().toISOString(),
        source: 'water_infra',
      },
    })
  );
}

const WATER_FACILITIES = [
  // Major dams
  { name: '黒部ダム', type: 'dam', operator: '関西電力', lat: 36.5667, lon: 137.6633, height_m: 186, capacity_mcm: 200, prefecture: '富山県' },
  { name: '奥利根ダム (奥只見)', type: 'dam', operator: '電源開発', lat: 37.1700, lon: 139.2200, height_m: 157, capacity_mcm: 601, prefecture: '福島県' },
  { name: '田子倉ダム', type: 'dam', operator: '電源開発', lat: 37.0500, lon: 139.3500, height_m: 145, capacity_mcm: 494, prefecture: '福島県' },
  { name: '宮ヶ瀬ダム', type: 'dam', operator: '国交省', lat: 35.5400, lon: 139.2400, height_m: 156, capacity_mcm: 193, prefecture: '神奈川県' },
  { name: '小河内ダム (奥多摩湖)', type: 'dam', operator: '東京都水道局', lat: 35.7900, lon: 139.0500, height_m: 149, capacity_mcm: 189, prefecture: '東京都' },
  { name: '佐久間ダム', type: 'dam', operator: '電源開発', lat: 35.0900, lon: 137.8100, height_m: 156, capacity_mcm: 326, prefecture: '静岡県' },
  { name: '矢作ダム', type: 'dam', operator: '国交省', lat: 35.2300, lon: 137.4500, height_m: 100, capacity_mcm: 80, prefecture: '愛知県' },
  { name: '徳山ダム', type: 'dam', operator: '水資源機構', lat: 35.6900, lon: 136.5100, height_m: 161, capacity_mcm: 660, prefecture: '岐阜県' },
  { name: '丹生川ダム', type: 'dam', operator: '岐阜県', lat: 36.1100, lon: 137.2400, height_m: 70, capacity_mcm: 27, prefecture: '岐阜県' },
  { name: '池田ダム', type: 'dam', operator: '水資源機構', lat: 34.0200, lon: 133.7900, height_m: 24, capacity_mcm: 13, prefecture: '徳島県' },
  { name: '早明浦ダム', type: 'dam', operator: '水資源機構', lat: 33.7900, lon: 133.5800, height_m: 106, capacity_mcm: 316, prefecture: '高知県' },
  { name: '一庫ダム', type: 'dam', operator: '水資源機構', lat: 34.9200, lon: 135.4400, height_m: 75, capacity_mcm: 33, prefecture: '兵庫県' },
  { name: '布引ダム', type: 'dam', operator: '神戸市', lat: 34.7100, lon: 135.1900, height_m: 33, capacity_mcm: 0.4, prefecture: '兵庫県' },
  { name: '日吉ダム', type: 'dam', operator: '水資源機構', lat: 35.1400, lon: 135.5500, height_m: 67, capacity_mcm: 66, prefecture: '京都府' },
  { name: '天ヶ瀬ダム', type: 'dam', operator: '国交省', lat: 34.8800, lon: 135.8200, height_m: 73, capacity_mcm: 27, prefecture: '京都府' },
  { name: '草木ダム', type: 'dam', operator: '水資源機構', lat: 36.4500, lon: 139.3700, height_m: 140, capacity_mcm: 60, prefecture: '群馬県' },
  { name: '矢木沢ダム', type: 'dam', operator: '水資源機構', lat: 36.8800, lon: 139.0500, height_m: 131, capacity_mcm: 204, prefecture: '群馬県' },
  { name: '藤原ダム', type: 'dam', operator: '国交省', lat: 36.8300, lon: 138.9700, height_m: 95, capacity_mcm: 52, prefecture: '群馬県' },
  { name: '相俣ダム', type: 'dam', operator: '国交省', lat: 36.7700, lon: 138.9200, height_m: 67, capacity_mcm: 25, prefecture: '群馬県' },
  { name: '川治ダム', type: 'dam', operator: '国交省', lat: 36.9100, lon: 139.6700, height_m: 140, capacity_mcm: 83, prefecture: '栃木県' },
  { name: '湯西川ダム', type: 'dam', operator: '国交省', lat: 36.9500, lon: 139.6500, height_m: 119, capacity_mcm: 75, prefecture: '栃木県' },
  { name: '味噌川ダム', type: 'dam', operator: '水資源機構', lat: 35.8700, lon: 137.6700, height_m: 140, capacity_mcm: 61, prefecture: '長野県' },
  { name: '水源池 摺上川ダム', type: 'dam', operator: '国交省', lat: 37.8300, lon: 140.4800, height_m: 105, capacity_mcm: 153, prefecture: '福島県' },
  { name: '玉川ダム', type: 'dam', operator: '国交省', lat: 39.9300, lon: 140.7900, height_m: 100, capacity_mcm: 254, prefecture: '秋田県' },
  { name: '七ヶ宿ダム', type: 'dam', operator: '国交省', lat: 38.0100, lon: 140.4200, height_m: 90, capacity_mcm: 109, prefecture: '宮城県' },
  { name: '夕張シューパロダム', type: 'dam', operator: '国交省', lat: 43.0700, lon: 142.0700, height_m: 110, capacity_mcm: 427, prefecture: '北海道' },
  { name: '十勝ダム', type: 'dam', operator: '国交省', lat: 43.2600, lon: 142.9300, height_m: 84, capacity_mcm: 113, prefecture: '北海道' },
  { name: '苫田ダム', type: 'dam', operator: '国交省', lat: 35.0700, lon: 134.0000, height_m: 74, capacity_mcm: 84, prefecture: '岡山県' },
  { name: '土師ダム', type: 'dam', operator: '水資源機構', lat: 34.6800, lon: 132.8000, height_m: 50, capacity_mcm: 47, prefecture: '広島県' },
  { name: '寺内ダム', type: 'dam', operator: '水資源機構', lat: 33.4200, lon: 130.6500, height_m: 83, capacity_mcm: 18, prefecture: '福岡県' },
  { name: '津屋川ダム', type: 'dam', operator: '岐阜県', lat: 35.4500, lon: 136.4200, height_m: 38, capacity_mcm: 5, prefecture: '岐阜県' },

  // Major water treatment plants
  { name: '朝霞浄水場', type: 'water_treatment', operator: '東京都水道局', lat: 35.7900, lon: 139.5800, capacity_mcm: 1.7, prefecture: '埼玉県' },
  { name: '東村山浄水場', type: 'water_treatment', operator: '東京都水道局', lat: 35.7500, lon: 139.4800, capacity_mcm: 1.3, prefecture: '東京都' },
  { name: '金町浄水場', type: 'water_treatment', operator: '東京都水道局', lat: 35.7800, lon: 139.8800, capacity_mcm: 1.5, prefecture: '東京都' },
  { name: '三郷浄水場', type: 'water_treatment', operator: '東京都水道局', lat: 35.8400, lon: 139.8800, capacity_mcm: 1.1, prefecture: '埼玉県' },
  { name: '柏井浄水場', type: 'water_treatment', operator: '横浜市水道局', lat: 35.5400, lon: 139.5500, capacity_mcm: 0.4, prefecture: '神奈川県' },
  { name: '相模原浄水場', type: 'water_treatment', operator: '神奈川県企業庁', lat: 35.5500, lon: 139.3700, capacity_mcm: 0.6, prefecture: '神奈川県' },
  { name: '庭窪浄水場', type: 'water_treatment', operator: '大阪広域水道企業団', lat: 34.7600, lon: 135.6300, capacity_mcm: 1.3, prefecture: '大阪府' },
  { name: '柴島浄水場', type: 'water_treatment', operator: '大阪市水道局', lat: 34.7300, lon: 135.5200, capacity_mcm: 1.2, prefecture: '大阪府' },
  { name: '豊野浄水場', type: 'water_treatment', operator: '大阪市水道局', lat: 34.6900, lon: 135.4700, capacity_mcm: 0.5, prefecture: '大阪府' },
  { name: '鍋屋上野浄水場', type: 'water_treatment', operator: '名古屋市上下水道局', lat: 35.1900, lon: 136.9300, capacity_mcm: 0.7, prefecture: '愛知県' },
  { name: '蹴上浄水場', type: 'water_treatment', operator: '京都市上下水道局', lat: 35.0100, lon: 135.7900, capacity_mcm: 0.2, prefecture: '京都府' },
  { name: '松ヶ崎浄水場', type: 'water_treatment', operator: '京都市上下水道局', lat: 35.0500, lon: 135.7800, capacity_mcm: 0.2, prefecture: '京都府' },
  { name: '北谷浄水場', type: 'water_treatment', operator: '沖縄県企業局', lat: 26.3300, lon: 127.7600, capacity_mcm: 0.3, prefecture: '沖縄県' },
  { name: '清田配水池 (札幌)', type: 'water_treatment', operator: '札幌市水道局', lat: 43.0200, lon: 141.4400, capacity_mcm: 0.2, prefecture: '北海道' },

  // Major sewage treatment plants
  { name: '芝浦水再生センター', type: 'sewage', operator: '東京都下水道局', lat: 35.6500, lon: 139.7700, capacity_mcm: 0.8, prefecture: '東京都' },
  { name: '森ヶ崎水再生センター', type: 'sewage', operator: '東京都下水道局', lat: 35.5600, lon: 139.7400, capacity_mcm: 1.5, prefecture: '東京都' },
  { name: '葛西水再生センター', type: 'sewage', operator: '東京都下水道局', lat: 35.6500, lon: 139.8800, capacity_mcm: 1.0, prefecture: '東京都' },
  { name: '中川水再生センター', type: 'sewage', operator: '東京都下水道局', lat: 35.7800, lon: 139.8500, capacity_mcm: 0.8, prefecture: '東京都' },
  { name: '南部下水処理場', type: 'sewage', operator: '横浜市環境創造局', lat: 35.4100, lon: 139.6400, capacity_mcm: 0.4, prefecture: '神奈川県' },
  { name: '中央下水処理場', type: 'sewage', operator: '大阪市建設局', lat: 34.6500, lon: 135.4400, capacity_mcm: 0.6, prefecture: '大阪府' },
  { name: '名城下水処理場', type: 'sewage', operator: '名古屋市上下水道局', lat: 35.1800, lon: 136.9000, capacity_mcm: 0.4, prefecture: '愛知県' },
];

function generateSeedData() {
  const now = new Date();
  return WATER_FACILITIES.map((w, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [w.lon, w.lat] },
    properties: {
      facility_id: `WTR_${String(i + 1).padStart(4, '0')}`,
      name: w.name,
      operator: w.operator,
      facility_type: w.type,
      height_m: w.height_m || null,
      capacity_mcm: w.capacity_mcm,
      prefecture: w.prefecture,
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'water_infra',
    },
  }));
}

export default async function collectWaterInfra() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'water_infra',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Japan water infrastructure - dams, water treatment plants, sewage treatment',
    },
    metadata: {},
  };
}
