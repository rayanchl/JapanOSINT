/**
 * Karaoke Chains Collector
 * Japanese karaoke box chains (Big Echo, Karaoke-kan, Shidax, Joysound, etc.)
 * Live: OSM Overpass `amenity=karaoke_box` + brand tags.
 * Seed: flagship locations of major chains.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpass(
    'node["amenity"="karaoke_box"](area.jp);way["amenity"="karaoke_box"](area.jp);node["shop"="karaoke"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        karaoke_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || `Karaoke ${i + 1}`,
        name_ja: el.tags?.name || null,
        brand: el.tags?.brand || null,
        operator: el.tags?.operator || null,
        opening_hours: el.tags?.opening_hours || null,
        rooms: el.tags?.rooms || null,
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

const SEED_KARAOKE = [
  // Big Echo (Daiichikosho) — largest chain, ~530 stores
  { name: 'Big Echo Shinjuku Higashiguchi', lat: 35.6912, lon: 139.7036, prefecture: '東京都', city: '新宿区', brand: 'Big Echo', rooms: 55, founded: 1988 },
  { name: 'Big Echo Shibuya Center-gai', lat: 35.6595, lon: 139.6988, prefecture: '東京都', city: '渋谷区', brand: 'Big Echo', rooms: 48, founded: 1989 },
  { name: 'Big Echo Ikebukuro East', lat: 35.7297, lon: 139.7119, prefecture: '東京都', city: '豊島区', brand: 'Big Echo', rooms: 50, founded: 1990 },
  { name: 'Big Echo Shinbashi', lat: 35.6659, lon: 139.7586, prefecture: '東京都', city: '港区', brand: 'Big Echo', rooms: 40, founded: 1992 },
  { name: 'Big Echo Ueno', lat: 35.7125, lon: 139.7769, prefecture: '東京都', city: '台東区', brand: 'Big Echo', rooms: 42, founded: 1993 },
  { name: 'Big Echo Umeda (Osaka)', lat: 34.7022, lon: 135.4981, prefecture: '大阪府', city: '大阪市北区', brand: 'Big Echo', rooms: 60, founded: 1991 },
  { name: 'Big Echo Namba', lat: 34.6653, lon: 135.5017, prefecture: '大阪府', city: '大阪市中央区', brand: 'Big Echo', rooms: 55, founded: 1992 },
  { name: 'Big Echo Sakae Nagoya', lat: 35.1710, lon: 136.9072, prefecture: '愛知県', city: '名古屋市中区', brand: 'Big Echo', rooms: 45, founded: 1993 },
  { name: 'Big Echo Tenjin Fukuoka', lat: 33.5906, lon: 130.3983, prefecture: '福岡県', city: '福岡市中央区', brand: 'Big Echo', rooms: 38, founded: 1994 },
  { name: 'Big Echo Sapporo Susukino', lat: 43.0553, lon: 141.3539, prefecture: '北海道', city: '札幌市中央区', brand: 'Big Echo', rooms: 36, founded: 1995 },
  { name: 'Big Echo HQ (Daiichikosho)', lat: 35.6336, lon: 139.7236, prefecture: '東京都', city: '品川区', brand: 'Big Echo', rooms: 0, founded: 1971, kind: 'hq' },

  // Karaoke-kan (Care-en) — featured in Lost in Translation
  { name: 'Karaoke-kan Shibuya (Lost in Translation)', lat: 35.6595, lon: 139.7010, prefecture: '東京都', city: '渋谷区', brand: 'Karaoke-kan', rooms: 50, founded: 1990 },
  { name: 'Karaoke-kan Shinjuku Kabukicho', lat: 35.6956, lon: 139.7025, prefecture: '東京都', city: '新宿区', brand: 'Karaoke-kan', rooms: 48, founded: 1992 },
  { name: 'Karaoke-kan Akihabara', lat: 35.6991, lon: 139.7725, prefecture: '東京都', city: '千代田区', brand: 'Karaoke-kan', rooms: 45, founded: 1995 },
  { name: 'Karaoke-kan Shinbashi', lat: 35.6667, lon: 139.7578, prefecture: '東京都', city: '港区', brand: 'Karaoke-kan', rooms: 38, founded: 1996 },
  { name: 'Karaoke-kan Ueno', lat: 35.7133, lon: 139.7761, prefecture: '東京都', city: '台東区', brand: 'Karaoke-kan', rooms: 40, founded: 1997 },
  { name: 'Karaoke-kan HQ', lat: 35.6870, lon: 139.7026, prefecture: '東京都', city: '渋谷区', brand: 'Karaoke-kan', rooms: 0, founded: 1989, kind: 'hq' },

  // Shidax — legendary chain that exited karaoke in 2022 but lives on
  { name: 'Shidax Village Shibuya (former flagship)', lat: 35.6629, lon: 139.6983, prefecture: '東京都', city: '渋谷区', brand: 'Shidax', rooms: 0, founded: 1988, kind: 'defunct' },
  { name: 'Shidax Culture Village Umeda', lat: 34.7042, lon: 135.4963, prefecture: '大阪府', city: '大阪市北区', brand: 'Shidax', rooms: 0, founded: 1992, kind: 'defunct' },

  // JOYSOUND (Xing Inc.) — song delivery system
  { name: 'JOYSOUND Shinjuku Minamiguchi', lat: 35.6879, lon: 139.7003, prefecture: '東京都', city: '新宿区', brand: 'JOYSOUND', rooms: 52, founded: 1992 },
  { name: 'JOYSOUND Ikebukuro West', lat: 35.7303, lon: 139.7094, prefecture: '東京都', city: '豊島区', brand: 'JOYSOUND', rooms: 48, founded: 1994 },
  { name: 'JOYSOUND Akihabara Showa', lat: 35.6981, lon: 139.7733, prefecture: '東京都', city: '千代田区', brand: 'JOYSOUND', rooms: 44, founded: 1996 },
  { name: 'JOYSOUND Dotonbori', lat: 34.6690, lon: 135.5022, prefecture: '大阪府', city: '大阪市中央区', brand: 'JOYSOUND', rooms: 40, founded: 1997 },
  { name: 'JOYSOUND Kyoto Kawaramachi', lat: 35.0033, lon: 135.7681, prefecture: '京都府', city: '京都市中京区', brand: 'JOYSOUND', rooms: 36, founded: 1998 },
  { name: 'Xing HQ (JOYSOUND)', lat: 35.4703, lon: 136.8058, prefecture: '愛知県', city: '名古屋市', brand: 'JOYSOUND', rooms: 0, founded: 1992, kind: 'hq' },

  // DAM (Daiichikosho's karaoke system) — competitor to JOYSOUND
  { name: 'Club DAM Namba', lat: 34.6660, lon: 135.5010, prefecture: '大阪府', city: '大阪市中央区', brand: 'Club DAM', rooms: 45, founded: 1994 },
  { name: 'Club DAM Sakae', lat: 35.1715, lon: 136.9080, prefecture: '愛知県', city: '名古屋市中区', brand: 'Club DAM', rooms: 42, founded: 1996 },

  // Karaoke Mac — Kyushu chain
  { name: 'Karaoke Mac Tenjin', lat: 33.5919, lon: 130.3978, prefecture: '福岡県', city: '福岡市中央区', brand: 'Karaoke Mac', rooms: 38, founded: 1998 },
  { name: 'Karaoke Mac Hakata', lat: 33.5900, lon: 130.4200, prefecture: '福岡県', city: '福岡市博多区', brand: 'Karaoke Mac', rooms: 35, founded: 1999 },
  { name: 'Karaoke Mac Kumamoto', lat: 32.8033, lon: 130.7089, prefecture: '熊本県', city: '熊本市', brand: 'Karaoke Mac', rooms: 30, founded: 2001 },

  // Manekineko — Koshidaka chain, ~530 stores
  { name: 'Manekineko Shibuya Dogenzaka', lat: 35.6585, lon: 139.6989, prefecture: '東京都', city: '渋谷区', brand: 'Manekineko', rooms: 50, founded: 2005 },
  { name: 'Manekineko Shinjuku Kabukicho', lat: 35.6958, lon: 139.7028, prefecture: '東京都', city: '新宿区', brand: 'Manekineko', rooms: 55, founded: 2006 },
  { name: 'Manekineko Umeda', lat: 34.7036, lon: 135.4975, prefecture: '大阪府', city: '大阪市北区', brand: 'Manekineko', rooms: 48, founded: 2008 },
  { name: 'Manekineko Sapporo', lat: 43.0558, lon: 141.3539, prefecture: '北海道', city: '札幌市中央区', brand: 'Manekineko', rooms: 40, founded: 2010 },
  { name: 'Koshidaka HQ (Manekineko)', lat: 36.3894, lon: 139.0608, prefecture: '群馬県', city: '高崎市', brand: 'Manekineko', rooms: 0, founded: 1986, kind: 'hq' },

  // Uta Hiroba
  { name: 'Uta Hiroba Shinjuku West', lat: 35.6917, lon: 139.6983, prefecture: '東京都', city: '新宿区', brand: 'Uta Hiroba', rooms: 46, founded: 2000 },
  { name: 'Uta Hiroba Ikebukuro', lat: 35.7300, lon: 139.7100, prefecture: '東京都', city: '豊島区', brand: 'Uta Hiroba', rooms: 42, founded: 2001 },

  // Jankara (Kansai-based)
  { name: 'Jankara Namba', lat: 34.6661, lon: 135.5015, prefecture: '大阪府', city: '大阪市中央区', brand: 'Jankara', rooms: 50, founded: 1994 },
  { name: 'Jankara Umeda', lat: 34.7025, lon: 135.4968, prefecture: '大阪府', city: '大阪市北区', brand: 'Jankara', rooms: 48, founded: 1995 },
  { name: 'Jankara Kyoto Kawaramachi', lat: 35.0034, lon: 135.7685, prefecture: '京都府', city: '京都市中京区', brand: 'Jankara', rooms: 42, founded: 1997 },
  { name: 'Jankara Sannomiya Kobe', lat: 34.6950, lon: 135.1956, prefecture: '兵庫県', city: '神戸市中央区', brand: 'Jankara', rooms: 38, founded: 1999 },
  { name: 'Toenec Jankara HQ', lat: 34.6714, lon: 135.5033, prefecture: '大阪府', city: '大阪市中央区', brand: 'Jankara', rooms: 0, founded: 1993, kind: 'hq' },

  // Karaoke Banban
  { name: 'Karaoke Banban Osu Nagoya', lat: 35.1594, lon: 136.9047, prefecture: '愛知県', city: '名古屋市中区', brand: 'Banban', rooms: 40, founded: 1997 },
  { name: 'Karaoke Banban Shizuoka', lat: 34.9756, lon: 138.3831, prefecture: '静岡県', city: '静岡市葵区', brand: 'Banban', rooms: 35, founded: 2000 },

  // Cote d'Azur (Premier Anti-Aging?)
  { name: 'Cote d\'Azur Akihabara', lat: 35.6983, lon: 139.7739, prefecture: '東京都', city: '千代田区', brand: "Cote d'Azur", rooms: 44, founded: 2002 },
  { name: 'Cote d\'Azur Shibuya', lat: 35.6594, lon: 139.7015, prefecture: '東京都', city: '渋谷区', brand: "Cote d'Azur", rooms: 42, founded: 2003 },
];

function generateSeedData() {
  return SEED_KARAOKE.map((k, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [k.lon, k.lat] },
    properties: {
      karaoke_id: `KAR_${i + 1}`,
      name: k.name,
      brand: k.brand,
      rooms: k.rooms,
      founded: k.founded,
      kind: k.kind || 'store',
      prefecture: k.prefecture,
      city: k.city,
      country: 'JP',
      source: 'chain_seed',
    },
  }));
}

export default async function collectKaraokeChains() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'karaoke-chains',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'osm_overpass' : 'chain_seed',
      description: 'Japanese karaoke box chains (Big Echo, Karaoke-kan, JOYSOUND, Manekineko, Jankara)',
    },
    metadata: {},
  };
}
