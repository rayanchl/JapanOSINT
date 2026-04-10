/**
 * Manga/Net Cafes Collector
 * Japanese 漫画喫茶 / ネットカフェ — includes 24-hr cafes used as
 * informal overnight accommodation (net-cafe refugees / ネットカフェ難民).
 * Live: OSM Overpass `shop=internet_cafe` + `amenity=internet_cafe`.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpass(
    'node["shop"="internet_cafe"](area.jp);way["shop"="internet_cafe"](area.jp);node["amenity"="internet_cafe"](area.jp);way["amenity"="internet_cafe"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        cafe_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || `Net Cafe ${i + 1}`,
        name_ja: el.tags?.name || null,
        brand: el.tags?.brand || null,
        operator: el.tags?.operator || null,
        opening_hours: el.tags?.opening_hours || '24/7',
        shower: el.tags?.shower === 'yes',
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

const SEED_NET_CAFES = [
  // Media Cafe Popeye (Runsystem) — 24-hr
  { name: 'Media Cafe Popeye Shinjuku Kabukicho', lat: 35.6954, lon: 139.7022, prefecture: '東京都', city: '新宿区', brand: 'Media Cafe Popeye', seats: 180, kind: '24h' },
  { name: 'Media Cafe Popeye Ikebukuro West', lat: 35.7306, lon: 139.7092, prefecture: '東京都', city: '豊島区', brand: 'Media Cafe Popeye', seats: 150, kind: '24h' },
  { name: 'Media Cafe Popeye Shibuya Center-gai', lat: 35.6590, lon: 139.6989, prefecture: '東京都', city: '渋谷区', brand: 'Media Cafe Popeye', seats: 160, kind: '24h' },
  { name: 'Media Cafe Popeye Ueno', lat: 35.7119, lon: 139.7767, prefecture: '東京都', city: '台東区', brand: 'Media Cafe Popeye', seats: 130, kind: '24h' },
  { name: 'Media Cafe Popeye Akihabara', lat: 35.6988, lon: 139.7728, prefecture: '東京都', city: '千代田区', brand: 'Media Cafe Popeye', seats: 140, kind: '24h' },
  { name: 'Media Cafe Popeye Nipponbashi Osaka', lat: 34.6658, lon: 135.5033, prefecture: '大阪府', city: '大阪市中央区', brand: 'Media Cafe Popeye', seats: 140, kind: '24h' },

  // Jiyu-kukan (自遊空間) — Runsystem, one of the largest chains ~180 stores
  { name: 'Jiyu-kukan Shinjuku Nishiguchi', lat: 35.6919, lon: 139.6975, prefecture: '東京都', city: '新宿区', brand: 'Jiyu-kukan', seats: 200, kind: '24h' },
  { name: 'Jiyu-kukan Ikebukuro East', lat: 35.7295, lon: 139.7111, prefecture: '東京都', city: '豊島区', brand: 'Jiyu-kukan', seats: 170, kind: '24h' },
  { name: 'Jiyu-kukan Shibuya', lat: 35.6597, lon: 139.7006, prefecture: '東京都', city: '渋谷区', brand: 'Jiyu-kukan', seats: 180, kind: '24h' },
  { name: 'Jiyu-kukan Kabukicho', lat: 35.6953, lon: 139.7028, prefecture: '東京都', city: '新宿区', brand: 'Jiyu-kukan', seats: 190, kind: '24h' },
  { name: 'Jiyu-kukan Umeda', lat: 34.7030, lon: 135.4972, prefecture: '大阪府', city: '大阪市北区', brand: 'Jiyu-kukan', seats: 180, kind: '24h' },
  { name: 'Jiyu-kukan Namba', lat: 34.6663, lon: 135.5017, prefecture: '大阪府', city: '大阪市中央区', brand: 'Jiyu-kukan', seats: 170, kind: '24h' },
  { name: 'Jiyu-kukan Sakae', lat: 35.1712, lon: 136.9075, prefecture: '愛知県', city: '名古屋市中区', brand: 'Jiyu-kukan', seats: 160, kind: '24h' },
  { name: 'Jiyu-kukan Tenjin', lat: 33.5908, lon: 130.3981, prefecture: '福岡県', city: '福岡市中央区', brand: 'Jiyu-kukan', seats: 140, kind: '24h' },

  // Gran Cyber Cafe Bagus — upscale chain with flat beds
  { name: 'Gran Cyber Cafe Bagus Shibuya', lat: 35.6599, lon: 139.7012, prefecture: '東京都', city: '渋谷区', brand: 'Bagus', seats: 220, kind: '24h' },
  { name: 'Gran Cyber Cafe Bagus Akihabara', lat: 35.6986, lon: 139.7735, prefecture: '東京都', city: '千代田区', brand: 'Bagus', seats: 200, kind: '24h' },
  { name: 'Gran Cyber Cafe Bagus Shinjuku', lat: 35.6922, lon: 139.7000, prefecture: '東京都', city: '新宿区', brand: 'Bagus', seats: 210, kind: '24h' },
  { name: 'Gran Cyber Cafe Bagus Roppongi', lat: 35.6626, lon: 139.7312, prefecture: '東京都', city: '港区', brand: 'Bagus', seats: 190, kind: '24h' },

  // DiCE — large 24-hr chain with showers
  { name: 'DiCE Shinjuku Seibu', lat: 35.6944, lon: 139.7008, prefecture: '東京都', city: '新宿区', brand: 'DiCE', seats: 170, kind: '24h' },
  { name: 'DiCE Ikebukuro', lat: 35.7298, lon: 139.7116, prefecture: '東京都', city: '豊島区', brand: 'DiCE', seats: 160, kind: '24h' },
  { name: 'DiCE Ueno', lat: 35.7122, lon: 139.7761, prefecture: '東京都', city: '台東区', brand: 'DiCE', seats: 150, kind: '24h' },
  { name: 'DiCE Akihabara', lat: 35.6992, lon: 139.7742, prefecture: '東京都', city: '千代田区', brand: 'DiCE', seats: 155, kind: '24h' },

  // Manboo! — manga cafe with many budget overnight seats
  { name: 'Manboo! Shinjuku Kabukicho', lat: 35.6953, lon: 139.7024, prefecture: '東京都', city: '新宿区', brand: 'Manboo!', seats: 200, kind: '24h' },
  { name: 'Manboo! Akihabara Washington', lat: 35.6991, lon: 139.7731, prefecture: '東京都', city: '千代田区', brand: 'Manboo!', seats: 180, kind: '24h' },
  { name: 'Manboo! Shibuya Dogenzaka', lat: 35.6589, lon: 139.6986, prefecture: '東京都', city: '渋谷区', brand: 'Manboo!', seats: 170, kind: '24h' },
  { name: 'Manboo! Ikebukuro West', lat: 35.7305, lon: 139.7088, prefecture: '東京都', city: '豊島区', brand: 'Manboo!', seats: 190, kind: '24h' },
  { name: 'Manboo! Ueno Hirokoji', lat: 35.7116, lon: 139.7764, prefecture: '東京都', city: '台東区', brand: 'Manboo!', seats: 175, kind: '24h' },

  // Customa Cafe
  { name: 'Customa Cafe Akihabara', lat: 35.6984, lon: 139.7728, prefecture: '東京都', city: '千代田区', brand: 'Customa Cafe', seats: 140, kind: '24h' },
  { name: 'Customa Cafe Shinjuku', lat: 35.6923, lon: 139.7008, prefecture: '東京都', city: '新宿区', brand: 'Customa Cafe', seats: 150, kind: '24h' },

  // Aprecio
  { name: 'Aprecio Shinjuku East', lat: 35.6922, lon: 139.7039, prefecture: '東京都', city: '新宿区', brand: 'Aprecio', seats: 160, kind: '24h' },
  { name: 'Aprecio Ikebukuro', lat: 35.7292, lon: 139.7108, prefecture: '東京都', city: '豊島区', brand: 'Aprecio', seats: 145, kind: '24h' },

  // Regional
  { name: 'Jiyu-kukan Sapporo Susukino', lat: 43.0554, lon: 141.3542, prefecture: '北海道', city: '札幌市中央区', brand: 'Jiyu-kukan', seats: 150, kind: '24h' },
  { name: 'Jiyu-kukan Sendai Kokubuncho', lat: 38.2663, lon: 140.8716, prefecture: '宮城県', city: '仙台市青葉区', brand: 'Jiyu-kukan', seats: 130, kind: '24h' },
  { name: 'Media Cafe Popeye Hiroshima', lat: 34.3958, lon: 132.4592, prefecture: '広島県', city: '広島市中区', brand: 'Media Cafe Popeye', seats: 120, kind: '24h' },
  { name: 'Jiyu-kukan Kyoto Kawaramachi', lat: 35.0031, lon: 135.7690, prefecture: '京都府', city: '京都市中京区', brand: 'Jiyu-kukan', seats: 140, kind: '24h' },
  { name: 'Manboo! Kobe Sannomiya', lat: 34.6948, lon: 135.1955, prefecture: '兵庫県', city: '神戸市中央区', brand: 'Manboo!', seats: 135, kind: '24h' },
  { name: 'Jiyu-kukan Hakata', lat: 33.5908, lon: 130.4198, prefecture: '福岡県', city: '福岡市博多区', brand: 'Jiyu-kukan', seats: 160, kind: '24h' },
  { name: 'Media Cafe Popeye Okayama', lat: 34.6669, lon: 133.9192, prefecture: '岡山県', city: '岡山市北区', brand: 'Media Cafe Popeye', seats: 110, kind: '24h' },
  { name: 'Aprecio Kumamoto', lat: 32.8031, lon: 130.7086, prefecture: '熊本県', city: '熊本市', brand: 'Aprecio', seats: 120, kind: '24h' },
  { name: 'Media Cafe Popeye Naha', lat: 26.2147, lon: 127.6872, prefecture: '沖縄県', city: '那覇市', brand: 'Media Cafe Popeye', seats: 110, kind: '24h' },
  { name: 'Jiyu-kukan Niigata', lat: 37.9125, lon: 139.0617, prefecture: '新潟県', city: '新潟市中央区', brand: 'Jiyu-kukan', seats: 120, kind: '24h' },
  { name: 'Media Cafe Popeye Kanazawa', lat: 36.5778, lon: 136.6483, prefecture: '石川県', city: '金沢市', brand: 'Media Cafe Popeye', seats: 110, kind: '24h' },

  // Upscale / modern
  { name: 'Customa Cafe Shinagawa', lat: 35.6289, lon: 139.7391, prefecture: '東京都', city: '港区', brand: 'Customa Cafe', seats: 145, kind: '24h' },
  { name: 'Aprecio Gotanda', lat: 35.6263, lon: 139.7236, prefecture: '東京都', city: '品川区', brand: 'Aprecio', seats: 135, kind: '24h' },

  // HQs
  { name: 'Runsystem HQ', lat: 35.6903, lon: 139.7003, prefecture: '東京都', city: '新宿区', brand: 'Media Cafe Popeye / Jiyu-kukan', seats: 0, kind: 'hq' },
];

function generateSeedData() {
  return SEED_NET_CAFES.map((c, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [c.lon, c.lat] },
    properties: {
      cafe_id: `NC_${i + 1}`,
      name: c.name,
      brand: c.brand,
      seats: c.seats,
      kind: c.kind,
      prefecture: c.prefecture,
      city: c.city,
      country: 'JP',
      source: 'chain_seed',
    },
  }));
}

export default async function collectMangaNetCafes() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'manga-net-cafes',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'osm_overpass' : 'chain_seed',
      description: 'Japanese manga/internet cafes — 24-hr chains (Jiyu-kukan, Popeye, Bagus, DiCE, Manboo!)',
    },
    metadata: {},
  };
}
