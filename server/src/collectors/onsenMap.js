/**
 * Onsen (hot springs) Collector
 * Japanese hot spring districts and public bath houses.
 * Live: OSM Overpass `amenity=public_bath` + `natural=hot_spring`.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpass(
    'node["amenity"="public_bath"]["bath:type"="onsen"](area.jp);node["natural"="hot_spring"](area.jp);way["natural"="hot_spring"](area.jp);node["amenity"="public_bath"]["name"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        onsen_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || `Onsen ${i + 1}`,
        name_ja: el.tags?.name || null,
        bath_type: el.tags?.['bath:type'] || 'public_bath',
        operator: el.tags?.operator || null,
        website: el.tags?.website || null,
        wikidata: el.tags?.wikidata || null,
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

// Curated: major onsen towns (日本三名泉 + 三古泉 + 100名湯) — representative coordinates
const SEED_ONSEN = [
  // 日本三名泉 (Japan's 3 famous hot springs per Hayashi Razan)
  { name: '草津温泉', lat: 36.6228, lon: 138.5961, type: 'sulfuric', classic: 'sanmeisen', prefecture: '群馬県', est_visitors_yr: 3100000 },
  { name: '有馬温泉', lat: 34.7978, lon: 135.2472, type: 'iron_radium', classic: 'sanmeisen', prefecture: '兵庫県', est_visitors_yr: 1900000 },
  { name: '下呂温泉', lat: 35.8053, lon: 137.2447, type: 'alkaline_simple', classic: 'sanmeisen', prefecture: '岐阜県', est_visitors_yr: 1000000 },

  // 日本三古泉 (3 oldest hot springs)
  { name: '道後温泉', lat: 33.8525, lon: 132.7864, type: 'alkaline_simple', classic: 'sankosen', prefecture: '愛媛県', est_visitors_yr: 1000000 },
  { name: '白浜温泉', lat: 33.6864, lon: 135.3717, type: 'salt', classic: 'sankosen', prefecture: '和歌山県', est_visitors_yr: 2500000 },

  // Top onsen towns nationwide
  { name: '箱根湯本温泉', lat: 35.2333, lon: 139.0250, type: 'alkaline_simple', prefecture: '神奈川県', est_visitors_yr: 7000000 },
  { name: '熱海温泉', lat: 35.0950, lon: 139.0719, type: 'chloride', prefecture: '静岡県', est_visitors_yr: 3000000 },
  { name: '伊東温泉', lat: 34.9667, lon: 139.0950, type: 'chloride', prefecture: '静岡県', est_visitors_yr: 2000000 },
  { name: '修善寺温泉', lat: 34.9722, lon: 138.9239, type: 'simple', prefecture: '静岡県', est_visitors_yr: 800000 },
  { name: '鬼怒川温泉', lat: 36.8142, lon: 139.7064, type: 'alkaline_simple', prefecture: '栃木県', est_visitors_yr: 2200000 },
  { name: '那須温泉', lat: 37.1197, lon: 139.9847, type: 'sulfur', prefecture: '栃木県', est_visitors_yr: 1500000 },
  { name: '塩原温泉', lat: 36.9728, lon: 139.7739, type: 'sulfate', prefecture: '栃木県', est_visitors_yr: 1200000 },
  { name: '伊香保温泉', lat: 36.4950, lon: 138.9150, type: 'iron_sulfate', prefecture: '群馬県', est_visitors_yr: 1500000 },
  { name: '水上温泉', lat: 36.7889, lon: 138.9972, type: 'alkaline_simple', prefecture: '群馬県', est_visitors_yr: 800000 },
  { name: '四万温泉', lat: 36.6375, lon: 138.7447, type: 'chloride', prefecture: '群馬県', est_visitors_yr: 500000 },
  { name: '越後湯沢温泉', lat: 36.9361, lon: 138.8114, type: 'alkaline_simple', prefecture: '新潟県', est_visitors_yr: 2500000 },
  { name: '月岡温泉', lat: 37.9008, lon: 139.3325, type: 'sulfur', prefecture: '新潟県', est_visitors_yr: 600000 },
  { name: '赤倉温泉', lat: 36.8622, lon: 138.1986, type: 'simple', prefecture: '新潟県', est_visitors_yr: 500000 },
  { name: '山代温泉', lat: 36.3033, lon: 136.4244, type: 'sulfate', prefecture: '石川県', est_visitors_yr: 900000 },
  { name: '山中温泉', lat: 36.2397, lon: 136.3789, type: 'sulfate', prefecture: '石川県', est_visitors_yr: 600000 },
  { name: '片山津温泉', lat: 36.3294, lon: 136.3833, type: 'chloride', prefecture: '石川県', est_visitors_yr: 500000 },
  { name: '和倉温泉', lat: 37.0922, lon: 136.9583, type: 'salt', prefecture: '石川県', est_visitors_yr: 800000 },
  { name: '輪島温泉', lat: 37.3892, lon: 136.9028, type: 'salt', prefecture: '石川県', est_visitors_yr: 300000 },
  { name: '芦原温泉', lat: 36.2256, lon: 136.1939, type: 'chloride', prefecture: '福井県', est_visitors_yr: 1000000 },
  { name: '湯田中温泉', lat: 36.7464, lon: 138.4289, type: 'alkaline_simple', prefecture: '長野県', est_visitors_yr: 700000 },
  { name: '渋温泉', lat: 36.7542, lon: 138.4150, type: 'alkaline_simple', prefecture: '長野県', est_visitors_yr: 500000 },
  { name: '野沢温泉', lat: 36.9236, lon: 138.4364, type: 'sulfur', prefecture: '長野県', est_visitors_yr: 700000 },
  { name: '白骨温泉', lat: 36.1169, lon: 137.6533, type: 'sulfate', prefecture: '長野県', est_visitors_yr: 300000 },
  { name: '乗鞍高原温泉', lat: 36.0947, lon: 137.6294, type: 'sulfur', prefecture: '長野県', est_visitors_yr: 400000 },
  { name: '石和温泉', lat: 35.6514, lon: 138.6628, type: 'alkaline_simple', prefecture: '山梨県', est_visitors_yr: 1000000 },
  { name: '湯村温泉 (甲府)', lat: 35.6767, lon: 138.5536, type: 'alkaline_simple', prefecture: '山梨県', est_visitors_yr: 300000 },
  { name: '下部温泉', lat: 35.4675, lon: 138.4981, type: 'alkaline_simple', prefecture: '山梨県', est_visitors_yr: 200000 },
  { name: '温泉津温泉', lat: 35.0967, lon: 132.3322, type: 'chloride', prefecture: '島根県', est_visitors_yr: 200000 },
  { name: '玉造温泉', lat: 35.4303, lon: 133.0100, type: 'sulfate', prefecture: '島根県', est_visitors_yr: 600000 },
  { name: '三朝温泉', lat: 35.3906, lon: 133.8436, type: 'radium', prefecture: '鳥取県', est_visitors_yr: 400000 },
  { name: '皆生温泉', lat: 35.4497, lon: 133.3314, type: 'chloride', prefecture: '鳥取県', est_visitors_yr: 600000 },
  { name: '湯原温泉', lat: 35.1553, lon: 133.6858, type: 'alkaline_simple', prefecture: '岡山県', est_visitors_yr: 300000 },
  { name: '湯郷温泉', lat: 35.0128, lon: 134.2464, type: 'alkaline_simple', prefecture: '岡山県', est_visitors_yr: 400000 },
  { name: '城崎温泉', lat: 35.6236, lon: 134.8158, type: 'chloride', prefecture: '兵庫県', est_visitors_yr: 1000000 },
  { name: '湯村温泉 (但馬)', lat: 35.5772, lon: 134.5000, type: 'alkaline_simple', prefecture: '兵庫県', est_visitors_yr: 250000 },
  { name: '洞川温泉', lat: 34.2908, lon: 135.8717, type: 'simple', prefecture: '奈良県', est_visitors_yr: 200000 },
  { name: '十津川温泉', lat: 33.8442, lon: 135.7744, type: 'sulfate', prefecture: '奈良県', est_visitors_yr: 150000 },
  { name: '湯の峰温泉', lat: 33.8333, lon: 135.7550, type: 'sulfur', prefecture: '和歌山県', est_visitors_yr: 150000 },
  { name: '勝浦温泉', lat: 33.6314, lon: 135.9361, type: 'sulfur', prefecture: '和歌山県', est_visitors_yr: 900000 },
  { name: '龍神温泉', lat: 33.9708, lon: 135.5672, type: 'sulfate', prefecture: '和歌山県', est_visitors_yr: 150000 },
  { name: '別府温泉', lat: 33.2839, lon: 131.4911, type: 'chloride_sulfate', prefecture: '大分県', est_visitors_yr: 8000000 },
  { name: '由布院温泉', lat: 33.2639, lon: 131.3583, type: 'simple', prefecture: '大分県', est_visitors_yr: 4000000 },
  { name: '黒川温泉', lat: 33.0825, lon: 131.1492, type: 'chloride_sulfate', prefecture: '熊本県', est_visitors_yr: 1000000 },
  { name: '杖立温泉', lat: 33.1544, lon: 131.0972, type: 'chloride', prefecture: '熊本県', est_visitors_yr: 400000 },
  { name: '人吉温泉', lat: 32.2092, lon: 130.7617, type: 'alkaline_simple', prefecture: '熊本県', est_visitors_yr: 500000 },
  { name: '嬉野温泉', lat: 33.1008, lon: 130.0486, type: 'bicarbonate', prefecture: '佐賀県', est_visitors_yr: 800000 },
  { name: '武雄温泉', lat: 33.1917, lon: 130.0189, type: 'alkaline_simple', prefecture: '佐賀県', est_visitors_yr: 500000 },
  { name: '雲仙温泉', lat: 32.7375, lon: 130.2569, type: 'sulfur', prefecture: '長崎県', est_visitors_yr: 500000 },
  { name: '小浜温泉', lat: 32.7269, lon: 130.1961, type: 'chloride', prefecture: '長崎県', est_visitors_yr: 300000 },
  { name: '指宿温泉', lat: 31.2456, lon: 130.6442, type: 'chloride_sand', prefecture: '鹿児島県', est_visitors_yr: 800000 },
  { name: '霧島温泉', lat: 31.9333, lon: 130.8417, type: 'sulfur', prefecture: '鹿児島県', est_visitors_yr: 500000 },
  { name: '定山渓温泉', lat: 42.9669, lon: 141.1600, type: 'chloride', prefecture: '北海道', est_visitors_yr: 2500000 },
  { name: '登別温泉', lat: 42.4958, lon: 141.1500, type: 'sulfur_iron', prefecture: '北海道', est_visitors_yr: 2500000 },
  { name: '洞爺湖温泉', lat: 42.5656, lon: 140.8633, type: 'chloride', prefecture: '北海道', est_visitors_yr: 1500000 },
  { name: '層雲峡温泉', lat: 43.7219, lon: 142.9669, type: 'sulfate', prefecture: '北海道', est_visitors_yr: 800000 },
  { name: '湯の川温泉', lat: 41.7783, lon: 140.7894, type: 'chloride', prefecture: '北海道', est_visitors_yr: 900000 },
  { name: 'ニセコ温泉郷', lat: 42.8594, lon: 140.6894, type: 'mixed', prefecture: '北海道', est_visitors_yr: 1500000 },
  { name: '十勝川温泉', lat: 42.9331, lon: 143.3108, type: 'moor', prefecture: '北海道', est_visitors_yr: 500000 },
  { name: '鳴子温泉', lat: 38.7397, lon: 140.7283, type: 'sulfate', prefecture: '宮城県', est_visitors_yr: 1500000 },
  { name: '秋保温泉', lat: 38.2503, lon: 140.7264, type: 'chloride', prefecture: '宮城県', est_visitors_yr: 1500000 },
  { name: '作並温泉', lat: 38.3303, lon: 140.6236, type: 'sulfate', prefecture: '宮城県', est_visitors_yr: 500000 },
  { name: '蔵王温泉', lat: 38.1608, lon: 140.3989, type: 'sulfur', prefecture: '山形県', est_visitors_yr: 1500000 },
  { name: '銀山温泉', lat: 38.5672, lon: 140.5175, type: 'sulfate', prefecture: '山形県', est_visitors_yr: 400000 },
  { name: '天童温泉', lat: 38.3633, lon: 140.3767, type: 'alkaline', prefecture: '山形県', est_visitors_yr: 800000 },
  { name: '飯坂温泉', lat: 37.8261, lon: 140.4642, type: 'alkaline_simple', prefecture: '福島県', est_visitors_yr: 1000000 },
  { name: '東山温泉', lat: 37.4839, lon: 139.9578, type: 'sulfate', prefecture: '福島県', est_visitors_yr: 600000 },
  { name: '浅虫温泉', lat: 40.8969, lon: 140.8594, type: 'alkaline_simple', prefecture: '青森県', est_visitors_yr: 500000 },
  { name: '酸ヶ湯温泉', lat: 40.6564, lon: 140.8503, type: 'sulfur', prefecture: '青森県', est_visitors_yr: 200000 },
  { name: '花巻温泉郷', lat: 39.4519, lon: 141.1319, type: 'simple', prefecture: '岩手県', est_visitors_yr: 1100000 },
];

function generateSeedData() {
  return SEED_ONSEN.map((o, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [o.lon, o.lat] },
    properties: {
      onsen_id: `ONSEN_${String(i + 1).padStart(5, '0')}`,
      name: o.name,
      water_type: o.type,
      classic: o.classic || null,
      prefecture: o.prefecture,
      est_visitors_yr: o.est_visitors_yr,
      country: 'JP',
      source: 'onsen_association_seed',
    },
  }));
}

export default async function collectOnsenMap() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'onsen-map',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'osm_overpass' : 'onsen_association_seed',
      description: 'Japanese onsen hot springs - major towns and public bath facilities',
    },
  };
}
