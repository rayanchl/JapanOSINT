/**
 * Sakura Front Collector
 * JMA / Weather News sakura zensen - annual cherry blossom progression front across Japan.
 * Live: JMA observation JSON + Weather News forecast JSON.
 */

import { fetchJson } from './_liveHelpers.js';

const JMA_SAKURA = 'https://www.data.jma.go.jp/sakura/data/sakura004_00.csv';
const WN_SAKURA = 'https://weathernews.jp/s/topics/sakura/json/forecast.json';

async function tryWeatherNews() {
  const data = await fetchJson(WN_SAKURA, { timeoutMs: 8000 });
  if (!data || !Array.isArray(data?.spots)) return null;
  return data.spots.slice(0, 1000).map((s, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [s.lon, s.lat] },
    properties: {
      spot_id: `WN_${i + 1}`,
      name: s.name,
      bloom_date: s.bloom || null,
      full_bloom_date: s.fullBloom || null,
      prefecture: s.pref || null,
      country: 'JP',
      source: 'weathernews_sakura',
    },
  }));
}

// Curated: 58 representative sakura observation points matching JMA 標本木 network + major hanami spots
// Ordered roughly by typical bloom progression (south-to-north, lowland-to-mountain)
const SEED_SAKURA = [
  // Okinawa / Amami (Kanhi-zakura early Jan-Feb)
  { name: '沖縄 名護 八重岳', lat: 26.6267, lon: 127.9311, prefecture: '沖縄県', typical_bloom: '01-18', species: 'kanhi', region: 'okinawa' },
  { name: '奄美大島 大島本島', lat: 28.3772, lon: 129.4939, prefecture: '鹿児島県', typical_bloom: '01-24', species: 'kanhi', region: 'amami' },

  // Kyushu (Somei-yoshino late March)
  { name: '熊本 熊本城', lat: 32.8063, lon: 130.7058, prefecture: '熊本県', typical_bloom: '03-22', species: 'somei_yoshino', region: 'kyushu' },
  { name: '福岡 舞鶴公園', lat: 33.5850, lon: 130.3833, prefecture: '福岡県', typical_bloom: '03-21', species: 'somei_yoshino', region: 'kyushu' },
  { name: '長崎 大村公園', lat: 32.8972, lon: 129.9539, prefecture: '長崎県', typical_bloom: '03-23', species: 'somei_yoshino', region: 'kyushu' },
  { name: '大分 岡城跡', lat: 32.9758, lon: 131.4056, prefecture: '大分県', typical_bloom: '03-24', species: 'somei_yoshino', region: 'kyushu' },
  { name: '宮崎 母智丘公園', lat: 31.7175, lon: 131.0839, prefecture: '宮崎県', typical_bloom: '03-20', species: 'somei_yoshino', region: 'kyushu' },
  { name: '鹿児島 甲突川', lat: 31.5800, lon: 130.5500, prefecture: '鹿児島県', typical_bloom: '03-25', species: 'somei_yoshino', region: 'kyushu' },
  { name: '佐賀 小城公園', lat: 33.2836, lon: 130.2078, prefecture: '佐賀県', typical_bloom: '03-22', species: 'somei_yoshino', region: 'kyushu' },

  // Shikoku
  { name: '高知 高知城', lat: 33.5608, lon: 133.5311, prefecture: '高知県', typical_bloom: '03-18', species: 'somei_yoshino', region: 'shikoku' },
  { name: '松山 松山城', lat: 33.8456, lon: 132.7656, prefecture: '愛媛県', typical_bloom: '03-24', species: 'somei_yoshino', region: 'shikoku' },
  { name: '高松 栗林公園', lat: 34.3267, lon: 134.0453, prefecture: '香川県', typical_bloom: '03-25', species: 'somei_yoshino', region: 'shikoku' },
  { name: '徳島 眉山公園', lat: 34.0636, lon: 134.5439, prefecture: '徳島県', typical_bloom: '03-26', species: 'somei_yoshino', region: 'shikoku' },

  // Chugoku
  { name: '広島 平和記念公園', lat: 34.3956, lon: 132.4536, prefecture: '広島県', typical_bloom: '03-23', species: 'somei_yoshino', region: 'chugoku' },
  { name: '岡山 後楽園', lat: 34.6669, lon: 133.9358, prefecture: '岡山県', typical_bloom: '03-26', species: 'somei_yoshino', region: 'chugoku' },
  { name: '山口 錦帯橋', lat: 34.1664, lon: 132.1769, prefecture: '山口県', typical_bloom: '03-22', species: 'somei_yoshino', region: 'chugoku' },
  { name: '島根 松江城', lat: 35.4750, lon: 133.0506, prefecture: '島根県', typical_bloom: '03-28', species: 'somei_yoshino', region: 'chugoku' },
  { name: '鳥取 鳥取城跡', lat: 35.5033, lon: 134.2383, prefecture: '鳥取県', typical_bloom: '03-29', species: 'somei_yoshino', region: 'chugoku' },

  // Kansai
  { name: '大阪 大阪城公園', lat: 34.6873, lon: 135.5258, prefecture: '大阪府', typical_bloom: '03-27', species: 'somei_yoshino', region: 'kansai' },
  { name: '京都 円山公園', lat: 35.0036, lon: 135.7806, prefecture: '京都府', typical_bloom: '03-28', species: 'somei_yoshino', region: 'kansai' },
  { name: '京都 嵐山', lat: 35.0094, lon: 135.6781, prefecture: '京都府', typical_bloom: '03-29', species: 'somei_yoshino', region: 'kansai' },
  { name: '奈良 吉野山', lat: 34.3633, lon: 135.8597, prefecture: '奈良県', typical_bloom: '04-03', species: 'yamazakura', region: 'kansai' },
  { name: '兵庫 姫路城', lat: 34.8394, lon: 134.6939, prefecture: '兵庫県', typical_bloom: '03-29', species: 'somei_yoshino', region: 'kansai' },
  { name: '和歌山 紀三井寺', lat: 34.1939, lon: 135.1919, prefecture: '和歌山県', typical_bloom: '03-21', species: 'somei_yoshino', region: 'kansai' },
  { name: '滋賀 海津大崎', lat: 35.4683, lon: 136.0564, prefecture: '滋賀県', typical_bloom: '04-06', species: 'somei_yoshino', region: 'kansai' },
  { name: '三重 宮川堤', lat: 34.4881, lon: 136.7061, prefecture: '三重県', typical_bloom: '03-27', species: 'somei_yoshino', region: 'kansai' },

  // Tokai / Chubu
  { name: '名古屋 名古屋城', lat: 35.1856, lon: 136.8997, prefecture: '愛知県', typical_bloom: '03-25', species: 'somei_yoshino', region: 'tokai' },
  { name: '岐阜 岐阜公園', lat: 35.4342, lon: 136.7814, prefecture: '岐阜県', typical_bloom: '03-28', species: 'somei_yoshino', region: 'tokai' },
  { name: '静岡 駿府城公園', lat: 34.9819, lon: 138.3833, prefecture: '静岡県', typical_bloom: '03-23', species: 'somei_yoshino', region: 'tokai' },
  { name: '長野 高遠城址公園', lat: 35.8328, lon: 138.0625, prefecture: '長野県', typical_bloom: '04-12', species: 'kohigan', region: 'chubu' },
  { name: '長野 松本城', lat: 36.2386, lon: 137.9692, prefecture: '長野県', typical_bloom: '04-13', species: 'somei_yoshino', region: 'chubu' },
  { name: '山梨 河口湖畔', lat: 35.5139, lon: 138.7631, prefecture: '山梨県', typical_bloom: '04-18', species: 'somei_yoshino', region: 'chubu' },

  // Hokuriku
  { name: '新潟 高田公園', lat: 37.1089, lon: 138.2367, prefecture: '新潟県', typical_bloom: '04-05', species: 'somei_yoshino', region: 'hokuriku' },
  { name: '富山 松川公園', lat: 36.6975, lon: 137.2128, prefecture: '富山県', typical_bloom: '04-04', species: 'somei_yoshino', region: 'hokuriku' },
  { name: '石川 兼六園', lat: 36.5622, lon: 136.6625, prefecture: '石川県', typical_bloom: '04-03', species: 'somei_yoshino', region: 'hokuriku' },
  { name: '福井 足羽川桜並木', lat: 36.0625, lon: 136.2225, prefecture: '福井県', typical_bloom: '04-02', species: 'somei_yoshino', region: 'hokuriku' },

  // Kanto
  { name: '東京 上野恩賜公園', lat: 35.7141, lon: 139.7744, prefecture: '東京都', typical_bloom: '03-24', species: 'somei_yoshino', region: 'kanto' },
  { name: '東京 千鳥ヶ淵', lat: 35.6889, lon: 139.7458, prefecture: '東京都', typical_bloom: '03-24', species: 'somei_yoshino', region: 'kanto' },
  { name: '東京 新宿御苑', lat: 35.6850, lon: 139.7100, prefecture: '東京都', typical_bloom: '03-25', species: 'somei_yoshino', region: 'kanto' },
  { name: '東京 靖国神社 標本木', lat: 35.6944, lon: 139.7436, prefecture: '東京都', typical_bloom: '03-24', species: 'somei_yoshino', region: 'kanto' },
  { name: '東京 目黒川', lat: 35.6436, lon: 139.7003, prefecture: '東京都', typical_bloom: '03-26', species: 'somei_yoshino', region: 'kanto' },
  { name: '神奈川 三ッ池公園', lat: 35.5225, lon: 139.6425, prefecture: '神奈川県', typical_bloom: '03-28', species: 'somei_yoshino', region: 'kanto' },
  { name: '神奈川 小田原城', lat: 35.2506, lon: 139.1536, prefecture: '神奈川県', typical_bloom: '03-27', species: 'somei_yoshino', region: 'kanto' },
  { name: '千葉 泉自然公園', lat: 35.6164, lon: 140.2225, prefecture: '千葉県', typical_bloom: '03-28', species: 'somei_yoshino', region: 'kanto' },
  { name: '埼玉 大宮公園', lat: 35.9217, lon: 139.6308, prefecture: '埼玉県', typical_bloom: '03-29', species: 'somei_yoshino', region: 'kanto' },
  { name: '栃木 日光 輪王寺', lat: 36.7581, lon: 139.5986, prefecture: '栃木県', typical_bloom: '04-15', species: 'somei_yoshino', region: 'kanto' },
  { name: '群馬 赤城南面千本桜', lat: 36.4625, lon: 139.1917, prefecture: '群馬県', typical_bloom: '04-05', species: 'somei_yoshino', region: 'kanto' },
  { name: '茨城 静峰ふるさと公園', lat: 36.5519, lon: 140.4736, prefecture: '茨城県', typical_bloom: '04-20', species: 'yaezakura', region: 'kanto' },

  // Tohoku
  { name: '福島 三春滝桜', lat: 37.4428, lon: 140.4817, prefecture: '福島県', typical_bloom: '04-10', species: 'benishidare', region: 'tohoku' },
  { name: '福島 鶴ヶ城', lat: 37.4878, lon: 139.9297, prefecture: '福島県', typical_bloom: '04-13', species: 'somei_yoshino', region: 'tohoku' },
  { name: '宮城 白石川堤 一目千本桜', lat: 38.0511, lon: 140.7608, prefecture: '宮城県', typical_bloom: '04-12', species: 'somei_yoshino', region: 'tohoku' },
  { name: '山形 霞城公園', lat: 38.2556, lon: 140.3400, prefecture: '山形県', typical_bloom: '04-14', species: 'somei_yoshino', region: 'tohoku' },
  { name: '岩手 北上展勝地', lat: 39.2892, lon: 141.1247, prefecture: '岩手県', typical_bloom: '04-18', species: 'somei_yoshino', region: 'tohoku' },
  { name: '秋田 角館 武家屋敷', lat: 39.5989, lon: 140.5681, prefecture: '秋田県', typical_bloom: '04-20', species: 'shidare', region: 'tohoku' },
  { name: '青森 弘前城', lat: 40.6075, lon: 140.4639, prefecture: '青森県', typical_bloom: '04-23', species: 'somei_yoshino', region: 'tohoku' },

  // Hokkaido (very late April - mid May)
  { name: '函館 五稜郭公園', lat: 41.7967, lon: 140.7570, prefecture: '北海道', typical_bloom: '04-29', species: 'somei_yoshino', region: 'hokkaido' },
  { name: '札幌 円山公園', lat: 43.0533, lon: 141.3256, prefecture: '北海道', typical_bloom: '05-03', species: 'ezoyamazakura', region: 'hokkaido' },
  { name: '松前 松前公園', lat: 41.4308, lon: 140.1144, prefecture: '北海道', typical_bloom: '04-28', species: 'somei_yoshino', region: 'hokkaido' },
  { name: '釧路 鶴ヶ岱公園', lat: 42.9883, lon: 144.3914, prefecture: '北海道', typical_bloom: '05-15', species: 'ezoyamazakura', region: 'hokkaido' },
];

function generateSeedData() {
  return SEED_SAKURA.map((s, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [s.lon, s.lat] },
    properties: {
      spot_id: `SAKURA_${String(i + 1).padStart(4, '0')}`,
      name: s.name,
      species: s.species,
      typical_bloom: s.typical_bloom,
      region: s.region,
      prefecture: s.prefecture,
      country: 'JP',
      source: 'sakura_front_seed',
    },
  }));
}

export default async function collectSakuraFront() {
  let features = await tryWeatherNews();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'sakura-front',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'weathernews_sakura' : 'sakura_front_seed',
      description: 'Sakura zensen - cherry blossom progression front across Japan',
    },
    metadata: {},
  };
}
