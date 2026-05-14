/**
 * Ski Resorts Collector
 * Japanese ski areas - Hokkaido, Tohoku, Niigata, Nagano, Gifu…
 * Live: OSM Overpass `landuse=winter_sports` + `piste:type=downhill`.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpass(
    'way["landuse"="winter_sports"]["name"](area.jp);relation["landuse"="winter_sports"]["name"](area.jp);node["sport"="skiing"]["name"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        resort_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || `Ski resort ${i + 1}`,
        name_ja: el.tags?.name || null,
        operator: el.tags?.operator || null,
        website: el.tags?.website || null,
        wikidata: el.tags?.wikidata || null,
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

// Major ski resorts by region
const SEED_RESORTS = [
  // ── Hokkaido (powder snow capital) ───────────────────
  { name: 'ニセコアンヌプリ国際', lat: 42.8656, lon: 140.6892, runs: 13, lifts: 6, vertical_m: 1000, prefecture: '北海道', visitors_yr: 500000 },
  { name: 'ニセコビレッジ', lat: 42.8389, lon: 140.6658, runs: 27, lifts: 8, vertical_m: 1170, prefecture: '北海道', visitors_yr: 600000 },
  { name: 'グラン・ヒラフ', lat: 42.8600, lon: 140.7022, runs: 30, lifts: 13, vertical_m: 940, prefecture: '北海道', visitors_yr: 900000 },
  { name: 'ハナゾノリゾート', lat: 42.8778, lon: 140.6933, runs: 10, lifts: 5, vertical_m: 900, prefecture: '北海道', visitors_yr: 350000 },
  { name: 'ルスツリゾート', lat: 42.7450, lon: 140.9075, runs: 37, lifts: 18, vertical_m: 594, prefecture: '北海道', visitors_yr: 1000000 },
  { name: 'キロロスノーワールド', lat: 43.0950, lon: 140.9000, runs: 23, lifts: 9, vertical_m: 610, prefecture: '北海道', visitors_yr: 350000 },
  { name: '札幌国際スキー場', lat: 43.0675, lon: 141.0214, runs: 9, lifts: 5, vertical_m: 640, prefecture: '北海道', visitors_yr: 280000 },
  { name: 'サッポロテイネ', lat: 43.0906, lon: 141.1981, runs: 15, lifts: 11, vertical_m: 683, prefecture: '北海道', visitors_yr: 400000 },
  { name: 'トマム スキーリゾート', lat: 43.0733, lon: 142.7042, runs: 29, lifts: 6, vertical_m: 585, prefecture: '北海道', visitors_yr: 450000 },
  { name: 'サホロリゾート', lat: 43.1344, lon: 142.8750, runs: 21, lifts: 7, vertical_m: 635, prefecture: '北海道', visitors_yr: 250000 },
  { name: '富良野スキー場', lat: 43.3306, lon: 142.3711, runs: 23, lifts: 10, vertical_m: 974, prefecture: '北海道', visitors_yr: 550000 },
  { name: '旭岳ロープウェイ', lat: 43.6944, lon: 142.8417, runs: 5, lifts: 1, vertical_m: 494, prefecture: '北海道', visitors_yr: 130000 },

  // ── Nagano ──────────────────────────────────────────
  { name: '白馬八方尾根スキー場', lat: 36.6950, lon: 137.8194, runs: 13, lifts: 23, vertical_m: 1071, prefecture: '長野県', visitors_yr: 1000000 },
  { name: '白馬岩岳スノーフィールド', lat: 36.7000, lon: 137.8489, runs: 25, lifts: 9, vertical_m: 550, prefecture: '長野県', visitors_yr: 350000 },
  { name: 'エイブル白馬五竜', lat: 36.6639, lon: 137.8350, runs: 16, lifts: 11, vertical_m: 950, prefecture: '長野県', visitors_yr: 500000 },
  { name: '白馬47ウインタースポーツパーク', lat: 36.6556, lon: 137.8433, runs: 8, lifts: 7, vertical_m: 760, prefecture: '長野県', visitors_yr: 400000 },
  { name: '白馬さのさかスキー場', lat: 36.6019, lon: 137.8611, runs: 10, lifts: 5, vertical_m: 390, prefecture: '長野県', visitors_yr: 150000 },
  { name: '野沢温泉スキー場', lat: 36.9294, lon: 138.4394, runs: 44, lifts: 19, vertical_m: 1086, prefecture: '長野県', visitors_yr: 800000 },
  { name: '志賀高原 焼額山', lat: 36.7194, lon: 138.5014, runs: 20, lifts: 11, vertical_m: 617, prefecture: '長野県', visitors_yr: 600000 },
  { name: '志賀高原 中央エリア', lat: 36.7267, lon: 138.4858, runs: 30, lifts: 20, vertical_m: 750, prefecture: '長野県', visitors_yr: 700000 },
  { name: '志賀高原 奥志賀', lat: 36.7417, lon: 138.5244, runs: 12, lifts: 6, vertical_m: 450, prefecture: '長野県', visitors_yr: 300000 },
  { name: '菅平高原スノーリゾート', lat: 36.5283, lon: 138.3439, runs: 60, lifts: 20, vertical_m: 530, prefecture: '長野県', visitors_yr: 700000 },
  { name: '軽井沢プリンスホテルスキー場', lat: 36.3431, lon: 138.6250, runs: 10, lifts: 9, vertical_m: 218, prefecture: '長野県', visitors_yr: 500000 },
  { name: 'ブランシュたかやまスキーリゾート', lat: 36.1875, lon: 138.2297, runs: 15, lifts: 5, vertical_m: 457, prefecture: '長野県', visitors_yr: 200000 },
  { name: '乗鞍高原温泉スキー場', lat: 36.1056, lon: 137.6283, runs: 15, lifts: 8, vertical_m: 600, prefecture: '長野県', visitors_yr: 200000 },
  { name: 'エコーバレースキー場', lat: 36.2153, lon: 138.2531, runs: 14, lifts: 6, vertical_m: 525, prefecture: '長野県', visitors_yr: 150000 },
  { name: '黒姫高原スノーパーク', lat: 36.8233, lon: 138.2267, runs: 14, lifts: 7, vertical_m: 530, prefecture: '長野県', visitors_yr: 150000 },
  { name: 'マウンテンパーク津南', lat: 36.9594, lon: 138.6339, runs: 6, lifts: 3, vertical_m: 380, prefecture: '長野県', visitors_yr: 80000 },
  { name: 'ハンターマウンテン塩原', lat: 37.0228, lon: 139.8117, runs: 12, lifts: 6, vertical_m: 680, prefecture: '栃木県', visitors_yr: 250000 },

  // ── Niigata ─────────────────────────────────────────
  { name: 'ガーラ湯沢スキー場', lat: 36.9489, lon: 138.8058, runs: 16, lifts: 11, vertical_m: 533, prefecture: '新潟県', visitors_yr: 550000 },
  { name: '苗場スキー場', lat: 36.7878, lon: 138.7878, runs: 24, lifts: 24, vertical_m: 889, prefecture: '新潟県', visitors_yr: 850000 },
  { name: 'かぐらスキー場', lat: 36.8369, lon: 138.7706, runs: 23, lifts: 18, vertical_m: 1225, prefecture: '新潟県', visitors_yr: 400000 },
  { name: '神立スノーリゾート', lat: 36.9650, lon: 138.8344, runs: 18, lifts: 6, vertical_m: 656, prefecture: '新潟県', visitors_yr: 250000 },
  { name: '舞子スノーリゾート', lat: 37.0533, lon: 138.8506, runs: 26, lifts: 9, vertical_m: 620, prefecture: '新潟県', visitors_yr: 300000 },
  { name: '石打丸山スキー場', lat: 36.9806, lon: 138.8375, runs: 25, lifts: 11, vertical_m: 694, prefecture: '新潟県', visitors_yr: 350000 },
  { name: '湯沢高原スキー場', lat: 36.9472, lon: 138.8111, runs: 10, lifts: 10, vertical_m: 770, prefecture: '新潟県', visitors_yr: 300000 },
  { name: '妙高杉ノ原スキー場', lat: 36.8333, lon: 138.1147, runs: 17, lifts: 7, vertical_m: 1124, prefecture: '新潟県', visitors_yr: 250000 },
  { name: 'アライリゾート', lat: 36.9375, lon: 138.2036, runs: 17, lifts: 5, vertical_m: 949, prefecture: '新潟県', visitors_yr: 200000 },
  { name: '赤倉温泉スキー場', lat: 36.8636, lon: 138.2000, runs: 17, lifts: 13, vertical_m: 894, prefecture: '新潟県', visitors_yr: 400000 },
  { name: '湯の丸スキー場', lat: 36.4089, lon: 138.4489, runs: 10, lifts: 7, vertical_m: 394, prefecture: '長野県', visitors_yr: 120000 },

  // ── Gifu / Fukushima / Yamagata / Tohoku ─────────────
  { name: '高鷲スノーパーク', lat: 35.9056, lon: 136.9411, runs: 22, lifts: 11, vertical_m: 800, prefecture: '岐阜県', visitors_yr: 450000 },
  { name: 'ダイナランド', lat: 35.9189, lon: 136.9539, runs: 20, lifts: 8, vertical_m: 680, prefecture: '岐阜県', visitors_yr: 400000 },
  { name: 'めいほうスキー場', lat: 35.9361, lon: 137.0236, runs: 14, lifts: 6, vertical_m: 700, prefecture: '岐阜県', visitors_yr: 300000 },
  { name: 'グランデコスノーリゾート', lat: 37.6644, lon: 140.0239, runs: 13, lifts: 5, vertical_m: 580, prefecture: '福島県', visitors_yr: 300000 },
  { name: '星野リゾート アルツ磐梯', lat: 37.5992, lon: 140.0569, runs: 29, lifts: 14, vertical_m: 720, prefecture: '福島県', visitors_yr: 350000 },
  { name: '猪苗代スキー場', lat: 37.5553, lon: 140.1050, runs: 19, lifts: 9, vertical_m: 680, prefecture: '福島県', visitors_yr: 250000 },
  { name: '裏磐梯スキー場', lat: 37.6794, lon: 140.0806, runs: 8, lifts: 4, vertical_m: 300, prefecture: '福島県', visitors_yr: 100000 },
  { name: '蔵王温泉スキー場', lat: 38.1586, lon: 140.3956, runs: 40, lifts: 38, vertical_m: 880, prefecture: '山形県', visitors_yr: 900000 },
  { name: '山形蔵王温泉スキー場 (樹氷原)', lat: 38.1378, lon: 140.4144, runs: 25, lifts: 10, vertical_m: 700, prefecture: '山形県', visitors_yr: 500000 },
  { name: '天元台高原スキー場', lat: 37.8875, lon: 140.1758, runs: 6, lifts: 5, vertical_m: 520, prefecture: '山形県', visitors_yr: 100000 },
  { name: '月山スキー場', lat: 38.5433, lon: 140.0278, runs: 3, lifts: 2, vertical_m: 300, prefecture: '山形県', visitors_yr: 80000 },
  { name: '八甲田スキー場', lat: 40.6597, lon: 140.8778, runs: 5, lifts: 1, vertical_m: 650, prefecture: '青森県', visitors_yr: 120000 },
  { name: '青森スプリング・スキーリゾート', lat: 40.6089, lon: 140.4847, runs: 16, lifts: 5, vertical_m: 504, prefecture: '青森県', visitors_yr: 180000 },
  { name: '安比高原スキー場', lat: 40.0603, lon: 140.9611, runs: 21, lifts: 13, vertical_m: 828, prefecture: '岩手県', visitors_yr: 800000 },
  { name: '雫石スキー場', lat: 39.7611, lon: 140.9692, runs: 14, lifts: 8, vertical_m: 680, prefecture: '岩手県', visitors_yr: 250000 },
  { name: '夏油高原スキー場', lat: 39.2431, lon: 140.8917, runs: 14, lifts: 6, vertical_m: 659, prefecture: '岩手県', visitors_yr: 150000 },
  { name: '田沢湖スキー場', lat: 39.7564, lon: 140.7525, runs: 14, lifts: 4, vertical_m: 545, prefecture: '秋田県', visitors_yr: 120000 },

  // ── Kanto / Chubu close-to-Tokyo ─────────────────────
  { name: '丸沼高原スキー場', lat: 36.8358, lon: 139.3536, runs: 13, lifts: 6, vertical_m: 602, prefecture: '群馬県', visitors_yr: 300000 },
  { name: '尾瀬岩鞍', lat: 36.8056, lon: 139.2478, runs: 17, lifts: 13, vertical_m: 620, prefecture: '群馬県', visitors_yr: 250000 },
  { name: '川場スキー場', lat: 36.6844, lon: 139.1722, runs: 10, lifts: 6, vertical_m: 648, prefecture: '群馬県', visitors_yr: 350000 },
  { name: '谷川岳天神平スキー場', lat: 36.8269, lon: 138.9589, runs: 10, lifts: 4, vertical_m: 533, prefecture: '群馬県', visitors_yr: 120000 },
  { name: '軽井沢スノーパーク', lat: 36.4569, lon: 138.7361, runs: 9, lifts: 4, vertical_m: 170, prefecture: '群馬県', visitors_yr: 150000 },
  { name: '富士見パノラマリゾート', lat: 35.9331, lon: 138.1967, runs: 8, lifts: 6, vertical_m: 730, prefecture: '長野県', visitors_yr: 180000 },
  { name: 'サンメドウズ清里', lat: 35.8903, lon: 138.4197, runs: 8, lifts: 5, vertical_m: 390, prefecture: '山梨県', visitors_yr: 130000 },
  { name: 'ふじてんスノーリゾート', lat: 35.4547, lon: 138.7369, runs: 7, lifts: 5, vertical_m: 259, prefecture: '山梨県', visitors_yr: 160000 },
  { name: 'スノーパーク イエティ', lat: 35.3572, lon: 138.8200, runs: 5, lifts: 3, vertical_m: 120, prefecture: '静岡県', visitors_yr: 100000 },
];

function generateSeedData() {
  return SEED_RESORTS.map((r, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [r.lon, r.lat] },
    properties: {
      resort_id: `SKI_${String(i + 1).padStart(5, '0')}`,
      name: r.name,
      runs: r.runs,
      lifts: r.lifts,
      vertical_m: r.vertical_m,
      visitors_yr: r.visitors_yr,
      prefecture: r.prefecture,
      country: 'JP',
      source: 'ski_resort_seed',
    },
  }));
}

export default async function collectSkiResorts() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'ski-resorts',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'osm_overpass' : 'ski_resort_seed',
      description: 'Japanese ski resorts and snow parks across Hokkaido/Tohoku/Niigata/Nagano/Gifu',
    },
  };
}
