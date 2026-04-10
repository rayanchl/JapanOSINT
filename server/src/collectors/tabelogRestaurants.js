/**
 * Tabelog / HotPepper Restaurant Collector
 * Maps restaurants across Japan via HotPepper Gourmet API.
 * Falls back to a curated seed of famous Tabelog top-rated restaurants.
 */

import { fetchOverpass } from './_liveHelpers.js';

const HOTPEPPER_KEY = process.env.HOTPEPPER_API_KEY || '';
const HOTPEPPER_URL = 'https://webservice.recruit.co.jp/hotpepper/gourmet/v1/';

const SEED_RESTAURANTS = [
  // ── Top-rated Tokyo ────────────────────────────────────────
  { name: '銀座 久兵衛 本店', lat: 35.6722, lon: 139.7647, genre: 'sushi', rating: 4.5, prefecture: '東京都' },
  { name: 'すきやばし次郎 本店', lat: 35.6722, lon: 139.7647, genre: 'sushi', rating: 4.7, prefecture: '東京都' },
  { name: 'かんだやぶそば', lat: 35.6953, lon: 139.7689, genre: 'soba', rating: 3.9, prefecture: '東京都' },
  { name: '一蘭 渋谷店', lat: 35.6595, lon: 139.7008, genre: 'ramen', rating: 3.8, prefecture: '東京都' },
  { name: '麺屋 一燈', lat: 35.7350, lon: 139.8419, genre: 'ramen', rating: 4.0, prefecture: '東京都' },
  { name: 'デンクシフロリレージュ', lat: 35.6664, lon: 139.7117, genre: 'french', rating: 4.4, prefecture: '東京都' },
  { name: 'カンテサンス', lat: 35.6433, lon: 139.7261, genre: 'french', rating: 4.5, prefecture: '東京都' },
  { name: 'ナリサワ', lat: 35.6664, lon: 139.7222, genre: 'french', rating: 4.5, prefecture: '東京都' },
  { name: '銀座 小十', lat: 35.6722, lon: 139.7647, genre: 'kaiseki', rating: 4.4, prefecture: '東京都' },
  { name: '神田鶴八', lat: 35.6953, lon: 139.7689, genre: 'sushi', rating: 4.3, prefecture: '東京都' },
  { name: '焼肉ジャンボ 篠崎本店', lat: 35.6892, lon: 139.9017, genre: 'yakiniku', rating: 4.0, prefecture: '東京都' },
  { name: '俺の割烹 本店', lat: 35.6722, lon: 139.7647, genre: 'kappo', rating: 3.9, prefecture: '東京都' },
  { name: '麻布幸村', lat: 35.6589, lon: 139.7311, genre: 'kaiseki', rating: 4.4, prefecture: '東京都' },
  { name: 'ペーターズ', lat: 35.6589, lon: 139.7311, genre: 'french', rating: 4.3, prefecture: '東京都' },
  { name: 'ロオジエ', lat: 35.6722, lon: 139.7647, genre: 'french', rating: 4.5, prefecture: '東京都' },

  // ── Osaka / Kansai ────────────────────────────────────────
  { name: '蟹道楽 本店', lat: 34.6754, lon: 135.5008, genre: 'crab', rating: 3.8, prefecture: '大阪府' },
  { name: '中村屋 本店', lat: 34.6864, lon: 135.5197, genre: 'okonomiyaki', rating: 3.9, prefecture: '大阪府' },
  { name: '大阪のたこ焼き 道頓堀くくる', lat: 34.6687, lon: 135.5028, genre: 'takoyaki', rating: 3.8, prefecture: '大阪府' },
  { name: '北極星 心斎橋本店', lat: 34.6754, lon: 135.5008, genre: 'omurice', rating: 3.7, prefecture: '大阪府' },
  { name: 'はり重 道頓堀本店', lat: 34.6687, lon: 135.5028, genre: 'sukiyaki', rating: 4.0, prefecture: '大阪府' },
  { name: '老舗 まい泉', lat: 34.6864, lon: 135.5197, genre: 'tonkatsu', rating: 3.8, prefecture: '大阪府' },
  { name: '京都吉兆 嵐山本店', lat: 35.0094, lon: 135.6789, genre: 'kaiseki', rating: 4.6, prefecture: '京都府' },
  { name: '菊乃井 本店', lat: 35.0036, lon: 135.7758, genre: 'kaiseki', rating: 4.5, prefecture: '京都府' },
  { name: 'いづう', lat: 35.0036, lon: 135.7758, genre: 'sushi', rating: 4.0, prefecture: '京都府' },
  { name: '本家尾張屋', lat: 35.0244, lon: 135.7625, genre: 'soba', rating: 3.9, prefecture: '京都府' },

  // ── Other regions ────────────────────────────────────────
  { name: '札幌すみれ ラーメン本店', lat: 43.0628, lon: 141.3478, genre: 'ramen', rating: 4.0, prefecture: '北海道' },
  { name: '札幌螃蟹家本店', lat: 43.0628, lon: 141.3478, genre: 'crab', rating: 4.1, prefecture: '北海道' },
  { name: '勝烈庵 本店', lat: 35.4437, lon: 139.6380, genre: 'tonkatsu', rating: 4.0, prefecture: '神奈川県' },
  { name: '崎陽軒 本店', lat: 35.4658, lon: 139.6224, genre: 'shumai', rating: 3.8, prefecture: '神奈川県' },
  { name: 'ひつまぶし 名古屋備長', lat: 35.1814, lon: 136.9069, genre: 'unagi', rating: 4.1, prefecture: '愛知県' },
  { name: '味噌煮込み 山本屋総本家', lat: 35.1681, lon: 136.9006, genre: 'udon', rating: 3.9, prefecture: '愛知県' },
  { name: '中洲屋台 中洲十番ラーメン', lat: 33.5904, lon: 130.4017, genre: 'ramen', rating: 4.0, prefecture: '福岡県' },
  { name: 'やまや 福岡本店', lat: 33.5904, lon: 130.4017, genre: 'mentaiko', rating: 3.9, prefecture: '福岡県' },
  { name: 'ふぐ料理 春帆楼', lat: 33.9544, lon: 130.9419, genre: 'fugu', rating: 4.4, prefecture: '山口県' },
  { name: '長崎ちゃんぽん 四海樓', lat: 32.7503, lon: 129.8775, genre: 'champon', rating: 4.0, prefecture: '長崎県' },
  { name: '熊本ラーメン 桂花本店', lat: 32.8019, lon: 130.7256, genre: 'ramen', rating: 3.9, prefecture: '熊本県' },
  { name: '沖縄そば すーまぬめぇ', lat: 26.2125, lon: 127.6809, genre: 'okinawan', rating: 4.0, prefecture: '沖縄県' },
];

async function tryHotpepper() {
  if (!HOTPEPPER_KEY) return null;
  try {
    const ctrl = new AbortController();
    const timeout = setTimeout(() => ctrl.abort(), 10000);
    const url = `${HOTPEPPER_URL}?key=${HOTPEPPER_KEY}&format=json&count=100&large_area=Z011`;
    const res = await fetch(url, { signal: ctrl.signal });
    clearTimeout(timeout);
    if (!res.ok) return null;
    const data = await res.json();
    const shops = data.results?.shop || [];
    return shops.map((s, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [parseFloat(s.lng), parseFloat(s.lat)] },
      properties: {
        facility_id: `RESTO_${String(i + 1).padStart(5, '0')}`,
        name: s.name || 'Restaurant',
        genre: s.genre?.name || null,
        budget: s.budget?.name || null,
        access: s.access || null,
        country: 'JP',
        source: 'hotpepper_api',
      },
    }));
  } catch {
    return null;
  }
}

function generateSeedData() {
  const now = new Date();
  return SEED_RESTAURANTS.map((r, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [r.lon, r.lat] },
    properties: {
      facility_id: `RESTO_${String(i + 1).padStart(5, '0')}`,
      name: r.name,
      genre: r.genre,
      rating: r.rating,
      prefecture: r.prefecture,
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'tabelog_seed',
    },
  }));
}

async function tryOSMRestaurants() {
  return fetchOverpass(
    'node["amenity"="restaurant"]["name"](area.jp);node["amenity"="fast_food"]["name"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        facility_id: `OSM_${el.id}`,
        name: el.tags?.name || `Restaurant ${i + 1}`,
        name_en: el.tags?.['name:en'] || null,
        genre: el.tags?.cuisine || 'unknown',
        amenity: el.tags?.amenity,
        address: el.tags?.['addr:full'] || el.tags?.['addr:city'] || null,
        phone: el.tags?.phone || null,
        website: el.tags?.website || null,
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

export default async function collectTabelogRestaurants() {
  let features = await tryHotpepper();
  let liveSource = 'hotpepper_api';
  if (!features || features.length === 0) {
    features = await tryOSMRestaurants();
    liveSource = 'osm_overpass';
  }
  const live = !!(features && features.length > 0);
  if (!live) {
    features = generateSeedData();
    liveSource = 'tabelog_seed';
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'tabelog_restaurants',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: liveSource,
      description: 'Japan restaurants - Tabelog top-rated + HotPepper Gourmet API + OSM Overpass',
    },
    metadata: {},
  };
}
