/**
 * Shrine & Temple Collector
 * Shinto shrines + Buddhist temples across Japan.
 * Live: OSM Overpass `amenity=place_of_worship` with religion filter.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpass(
    'node["amenity"="place_of_worship"]["religion"~"shinto|buddhist"]["name"](area.jp);way["amenity"="place_of_worship"]["religion"~"shinto|buddhist"]["name"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        place_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || `Place ${i + 1}`,
        name_ja: el.tags?.name || null,
        religion: el.tags?.religion || 'unknown',
        denomination: el.tags?.denomination || null,
        wikidata: el.tags?.wikidata || null,
        wikipedia: el.tags?.wikipedia || null,
        website: el.tags?.website || null,
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
    180_000,
  );
}

// Curated top shrines + temples — famous historic sites
const SEED_SITES = [
  // ── Top Shinto shrines ──────────────────────────────
  { name: '伊勢神宮 (内宮)', lat: 34.4549, lon: 136.7256, religion: 'shinto', rank: 'ichinomiya', prefecture: '三重県' },
  { name: '伊勢神宮 (外宮)', lat: 34.4881, lon: 136.7028, religion: 'shinto', rank: 'ichinomiya', prefecture: '三重県' },
  { name: '出雲大社', lat: 35.4019, lon: 132.6855, religion: 'shinto', rank: 'ichinomiya', prefecture: '島根県' },
  { name: '明治神宮', lat: 35.6764, lon: 139.6993, religion: 'shinto', rank: 'kanpei', prefecture: '東京都' },
  { name: '靖国神社', lat: 35.6942, lon: 139.7435, religion: 'shinto', rank: 'kanpei', prefecture: '東京都' },
  { name: '日光東照宮', lat: 36.7581, lon: 139.5986, religion: 'shinto', rank: 'bekkaku', prefecture: '栃木県' },
  { name: '伏見稲荷大社', lat: 34.9671, lon: 135.7727, religion: 'shinto', rank: 'kanpei', prefecture: '京都府' },
  { name: '平安神宮', lat: 35.0164, lon: 135.7822, religion: 'shinto', rank: 'kanpei', prefecture: '京都府' },
  { name: '八坂神社', lat: 35.0036, lon: 135.7786, religion: 'shinto', rank: 'kanpei', prefecture: '京都府' },
  { name: '北野天満宮', lat: 35.0311, lon: 135.7350, religion: 'shinto', rank: 'kanpei', prefecture: '京都府' },
  { name: '下鴨神社', lat: 35.0392, lon: 135.7725, religion: 'shinto', rank: 'kanpei', prefecture: '京都府' },
  { name: '上賀茂神社', lat: 35.0603, lon: 135.7528, religion: 'shinto', rank: 'kanpei', prefecture: '京都府' },
  { name: '春日大社', lat: 34.6814, lon: 135.8484, religion: 'shinto', rank: 'kanpei', prefecture: '奈良県' },
  { name: '住吉大社', lat: 34.6125, lon: 135.4931, religion: 'shinto', rank: 'kanpei', prefecture: '大阪府' },
  { name: '厳島神社', lat: 34.2960, lon: 132.3199, religion: 'shinto', rank: 'kanpei', prefecture: '広島県' },
  { name: '太宰府天満宮', lat: 33.5219, lon: 130.5347, religion: 'shinto', rank: 'kanpei', prefecture: '福岡県' },
  { name: '宇佐神宮', lat: 33.5264, lon: 131.3744, religion: 'shinto', rank: 'kanpei', prefecture: '大分県' },
  { name: '熊野本宮大社', lat: 33.8400, lon: 135.7733, religion: 'shinto', rank: 'kanpei', prefecture: '和歌山県' },
  { name: '熊野那智大社', lat: 33.6683, lon: 135.8908, religion: 'shinto', rank: 'kanpei', prefecture: '和歌山県' },
  { name: '熊野速玉大社', lat: 33.7342, lon: 135.9875, religion: 'shinto', rank: 'kanpei', prefecture: '和歌山県' },
  { name: '鶴岡八幡宮', lat: 35.3256, lon: 139.5564, religion: 'shinto', rank: 'kokuhei', prefecture: '神奈川県' },
  { name: '香取神宮', lat: 35.8867, lon: 140.5289, religion: 'shinto', rank: 'kanpei', prefecture: '千葉県' },
  { name: '鹿島神宮', lat: 35.9681, lon: 140.6306, religion: 'shinto', rank: 'kanpei', prefecture: '茨城県' },
  { name: '諏訪大社 (上社本宮)', lat: 35.9958, lon: 138.1208, religion: 'shinto', rank: 'kanpei', prefecture: '長野県' },
  { name: '気多大社', lat: 36.9208, lon: 136.7836, religion: 'shinto', rank: 'kokuhei', prefecture: '石川県' },
  { name: '熱田神宮', lat: 35.1267, lon: 136.9083, religion: 'shinto', rank: 'kanpei', prefecture: '愛知県' },
  { name: '氷川神社 (大宮)', lat: 35.9081, lon: 139.6297, religion: 'shinto', rank: 'kanpei', prefecture: '埼玉県' },
  { name: '富岡八幡宮', lat: 35.6719, lon: 139.8000, religion: 'shinto', rank: 'fusha', prefecture: '東京都' },
  { name: '神田明神', lat: 35.7025, lon: 139.7678, religion: 'shinto', rank: 'fusha', prefecture: '東京都' },
  { name: '日枝神社', lat: 35.6736, lon: 139.7394, religion: 'shinto', rank: 'kanpei', prefecture: '東京都' },
  { name: '箱根神社', lat: 35.2036, lon: 139.0247, religion: 'shinto', rank: 'kokuhei', prefecture: '神奈川県' },
  { name: '白山比咩神社', lat: 36.4922, lon: 136.6264, religion: 'shinto', rank: 'kokuhei', prefecture: '石川県' },

  // ── Top Buddhist temples ─────────────────────────────
  { name: '浅草寺', lat: 35.7148, lon: 139.7967, religion: 'buddhist', sect: 'tendai', prefecture: '東京都' },
  { name: '増上寺', lat: 35.6569, lon: 139.7486, religion: 'buddhist', sect: 'jodo', prefecture: '東京都' },
  { name: '豊川稲荷東京別院', lat: 35.6803, lon: 139.7322, religion: 'buddhist', sect: 'soto', prefecture: '東京都' },
  { name: '高尾山薬王院', lat: 35.6253, lon: 139.2436, religion: 'buddhist', sect: 'shingon', prefecture: '東京都' },
  { name: '川崎大師', lat: 35.5344, lon: 139.7303, religion: 'buddhist', sect: 'shingon', prefecture: '神奈川県' },
  { name: '鎌倉大仏 (高徳院)', lat: 35.3167, lon: 139.5361, religion: 'buddhist', sect: 'jodo', prefecture: '神奈川県' },
  { name: '建長寺', lat: 35.3358, lon: 139.5542, religion: 'buddhist', sect: 'rinzai', prefecture: '神奈川県' },
  { name: '円覚寺', lat: 35.3375, lon: 139.5511, religion: 'buddhist', sect: 'rinzai', prefecture: '神奈川県' },
  { name: '長谷寺 (鎌倉)', lat: 35.3128, lon: 139.5336, religion: 'buddhist', sect: 'jodo', prefecture: '神奈川県' },
  { name: '清水寺', lat: 34.9949, lon: 135.7851, religion: 'buddhist', sect: 'hosso', prefecture: '京都府' },
  { name: '金閣寺 (鹿苑寺)', lat: 35.0394, lon: 135.7292, religion: 'buddhist', sect: 'rinzai', prefecture: '京都府' },
  { name: '銀閣寺 (慈照寺)', lat: 35.0270, lon: 135.7983, religion: 'buddhist', sect: 'rinzai', prefecture: '京都府' },
  { name: '東寺', lat: 34.9806, lon: 135.7478, religion: 'buddhist', sect: 'shingon', prefecture: '京都府' },
  { name: '西本願寺', lat: 34.9919, lon: 135.7514, religion: 'buddhist', sect: 'jodo_shinshu', prefecture: '京都府' },
  { name: '東本願寺', lat: 34.9911, lon: 135.7578, religion: 'buddhist', sect: 'jodo_shinshu', prefecture: '京都府' },
  { name: '龍安寺', lat: 35.0342, lon: 135.7181, religion: 'buddhist', sect: 'rinzai', prefecture: '京都府' },
  { name: '天龍寺', lat: 35.0158, lon: 135.6736, religion: 'buddhist', sect: 'rinzai', prefecture: '京都府' },
  { name: '醍醐寺', lat: 34.9514, lon: 135.8197, religion: 'buddhist', sect: 'shingon', prefecture: '京都府' },
  { name: '仁和寺', lat: 35.0306, lon: 135.7139, religion: 'buddhist', sect: 'shingon', prefecture: '京都府' },
  { name: '三千院', lat: 35.1192, lon: 135.8344, religion: 'buddhist', sect: 'tendai', prefecture: '京都府' },
  { name: '東大寺', lat: 34.6890, lon: 135.8398, religion: 'buddhist', sect: 'kegon', prefecture: '奈良県' },
  { name: '興福寺', lat: 34.6831, lon: 135.8319, religion: 'buddhist', sect: 'hosso', prefecture: '奈良県' },
  { name: '薬師寺', lat: 34.6683, lon: 135.7839, religion: 'buddhist', sect: 'hosso', prefecture: '奈良県' },
  { name: '法隆寺', lat: 34.6147, lon: 135.7347, religion: 'buddhist', sect: 'shotoku', prefecture: '奈良県' },
  { name: '唐招提寺', lat: 34.6767, lon: 135.7842, religion: 'buddhist', sect: 'ritsu', prefecture: '奈良県' },
  { name: '四天王寺', lat: 34.6542, lon: 135.5169, religion: 'buddhist', sect: 'wasou', prefecture: '大阪府' },
  { name: '比叡山延暦寺', lat: 35.0703, lon: 135.8408, religion: 'buddhist', sect: 'tendai', prefecture: '滋賀県' },
  { name: '高野山金剛峯寺', lat: 34.2142, lon: 135.5839, religion: 'buddhist', sect: 'shingon', prefecture: '和歌山県' },
  { name: '善光寺', lat: 36.6614, lon: 138.1875, religion: 'buddhist', sect: 'tendai_jodo', prefecture: '長野県' },
  { name: '瑞巌寺', lat: 38.3678, lon: 141.0617, religion: 'buddhist', sect: 'rinzai', prefecture: '宮城県' },
  { name: '中尊寺', lat: 38.9925, lon: 141.0989, religion: 'buddhist', sect: 'tendai', prefecture: '岩手県' },
  { name: '毛越寺', lat: 38.9867, lon: 141.1139, religion: 'buddhist', sect: 'tendai', prefecture: '岩手県' },
  { name: '恐山菩提寺', lat: 41.3278, lon: 141.0939, religion: 'buddhist', sect: 'soto', prefecture: '青森県' },
  { name: '永平寺', lat: 36.0567, lon: 136.3553, religion: 'buddhist', sect: 'soto', prefecture: '福井県' },
  { name: '總持寺', lat: 35.5139, lon: 139.6828, religion: 'buddhist', sect: 'soto', prefecture: '神奈川県' },
  { name: '身延山久遠寺', lat: 35.3958, lon: 138.4189, religion: 'buddhist', sect: 'nichiren', prefecture: '山梨県' },
  { name: '中山寺', lat: 34.8036, lon: 135.3486, religion: 'buddhist', sect: 'shingon', prefecture: '兵庫県' },
  { name: '石山寺', lat: 34.9603, lon: 135.9056, religion: 'buddhist', sect: 'shingon', prefecture: '滋賀県' },
  { name: '三井寺 (園城寺)', lat: 35.0139, lon: 135.8533, religion: 'buddhist', sect: 'tendai', prefecture: '滋賀県' },
];

function generateSeedData() {
  return SEED_SITES.map((s, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [s.lon, s.lat] },
    properties: {
      place_id: `PLACE_${String(i + 1).padStart(5, '0')}`,
      name: s.name,
      religion: s.religion,
      rank: s.rank || null,
      sect: s.sect || null,
      prefecture: s.prefecture,
      country: 'JP',
      source: 'shrine_temple_seed',
    },
  }));
}

export default async function collectShrineTemple() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'shrine-temple',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'osm_overpass' : 'shrine_temple_seed',
      description: 'Shinto shrines and Buddhist temples - major historic sites across Japan',
    },
  };
}
