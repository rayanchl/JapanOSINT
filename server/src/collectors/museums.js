/**
 * Museums Collector
 * OSM Overpass `tourism=museum` + curated major museums.
 */

import { fetchOverpass } from './_liveHelpers.js';

const SEED_MUSEUMS = [
  // Tokyo majors
  { name: '東京国立博物館', lat: 35.7188, lon: 139.7764, kind: 'national', city: '東京' },
  { name: '国立科学博物館', lat: 35.7164, lon: 139.7758, kind: 'science', city: '東京' },
  { name: '国立西洋美術館', lat: 35.7156, lon: 139.7758, kind: 'art', city: '東京' },
  { name: '東京都美術館', lat: 35.7186, lon: 139.7728, kind: 'art', city: '東京' },
  { name: '東京都現代美術館', lat: 35.6792, lon: 139.8086, kind: 'art', city: '東京' },
  { name: '森美術館', lat: 35.6606, lon: 139.7297, kind: 'art', city: '東京' },
  { name: '国立新美術館', lat: 35.6650, lon: 139.7264, kind: 'art', city: '東京' },
  { name: 'サントリー美術館', lat: 35.6650, lon: 139.7322, kind: 'art', city: '東京' },
  { name: 'すみだ北斎美術館', lat: 35.6989, lon: 139.8031, kind: 'art', city: '東京' },
  { name: '日本科学未来館', lat: 35.6195, lon: 139.7764, kind: 'science', city: '東京' },
  { name: '江戸東京博物館', lat: 35.6964, lon: 139.7964, kind: 'history', city: '東京' },
  { name: '三鷹の森ジブリ美術館', lat: 35.6961, lon: 139.5703, kind: 'special', city: '三鷹' },
  // Kanto
  { name: '横浜美術館', lat: 35.4569, lon: 139.6306, kind: 'art', city: '横浜' },
  { name: 'カップヌードルミュージアム', lat: 35.4528, lon: 139.6383, kind: 'special', city: '横浜' },
  { name: 'MOA美術館', lat: 35.0894, lon: 139.0833, kind: 'art', city: '熱海' },
  { name: '大原美術館', lat: 34.5950, lon: 133.7708, kind: 'art', city: '倉敷' },
  // Kansai
  { name: '京都国立博物館', lat: 34.9897, lon: 135.7728, kind: 'national', city: '京都' },
  { name: '奈良国立博物館', lat: 34.6850, lon: 135.8364, kind: 'national', city: '奈良' },
  { name: '九州国立博物館', lat: 33.5183, lon: 130.5475, kind: 'national', city: '太宰府' },
  { name: '大阪市立美術館', lat: 34.6517, lon: 135.5117, kind: 'art', city: '大阪' },
  { name: '兵庫県立美術館', lat: 34.6939, lon: 135.2244, kind: 'art', city: '神戸' },
  { name: '京都国立近代美術館', lat: 35.0117, lon: 135.7831, kind: 'art', city: '京都' },
  // Aichi
  { name: 'トヨタ博物館', lat: 35.1819, lon: 136.9803, kind: 'special', city: '長久手' },
  { name: '名古屋市科学館', lat: 35.1656, lon: 136.8983, kind: 'science', city: '名古屋' },
  { name: 'リニア・鉄道館', lat: 35.0386, lon: 136.8519, kind: 'transport', city: '名古屋' },
  // Hokkaido
  { name: '北海道博物館', lat: 43.0383, lon: 141.4797, kind: 'history', city: '札幌' },
  { name: '札幌市時計台', lat: 43.0628, lon: 141.3536, kind: 'history', city: '札幌' },
  { name: '北海道立近代美術館', lat: 43.0628, lon: 141.3236, kind: 'art', city: '札幌' },
  // Tohoku
  { name: '青森県立美術館', lat: 40.8217, lon: 140.6919, kind: 'art', city: '青森' },
  { name: '宮城県美術館', lat: 38.2517, lon: 140.8456, kind: 'art', city: '仙台' },
  // Kyushu / Okinawa
  { name: '長崎原爆資料館', lat: 32.7733, lon: 129.8636, kind: 'history', city: '長崎' },
  { name: '広島平和記念資料館', lat: 34.3919, lon: 132.4525, kind: 'history', city: '広島' },
  { name: '沖縄県立博物館・美術館', lat: 26.2275, lon: 127.6911, kind: 'history', city: '那覇' },
  // Special / Iconic
  { name: 'ポーラ美術館', lat: 35.2389, lon: 139.0144, kind: 'art', city: '箱根' },
  { name: '岡本太郎美術館', lat: 35.6111, lon: 139.5783, kind: 'art', city: '川崎' },
  { name: 'ベネッセハウスミュージアム', lat: 34.4525, lon: 133.9933, kind: 'art', city: '直島' },
  { name: '地中美術館', lat: 34.4503, lon: 133.9911, kind: 'art', city: '直島' },
  { name: '金沢21世紀美術館', lat: 36.5611, lon: 136.6589, kind: 'art', city: '金沢' },
  { name: '足立美術館', lat: 35.4314, lon: 133.2083, kind: 'art', city: '安来' },
  { name: '福岡市美術館', lat: 33.5836, lon: 130.3786, kind: 'art', city: '福岡' },
];

async function tryOSMOverpass() {
  const features = await fetchOverpass(
    'node["tourism"="museum"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        museum_id: `OSM_${el.id}`,
        name: el.tags?.name || el.tags?.['name:en'] || `Museum ${i + 1}`,
        kind: el.tags?.museum || 'general',
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
  if (!features) return null;
  return features.slice(0, 300);
}

function generateSeedData() {
  return SEED_MUSEUMS.map((m, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [m.lon, m.lat] },
    properties: {
      museum_id: `MUS_${String(i + 1).padStart(5, '0')}`,
      name: m.name,
      kind: m.kind,
      city: m.city,
      country: 'JP',
      source: 'museum_seed',
    },
  }));
}

export default async function collectMuseums() {
  let features = await tryOSMOverpass();
  if (!features || features.length === 0) features = [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'museums',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: features?.[0]?.properties?.source === 'osm_overpass',
      description: 'Major Japanese museums: national, art, science, history',
    },
  };
}
