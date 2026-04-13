/**
 * Sakura Front Collector
 * JMA / Weather News sakura zensen - annual cherry blossom progression front across Japan.
 * Live: JMA observation JSON + Weather News forecast JSON.
 */

import { fetchJson, fetchText } from './_liveHelpers.js';

const JMA_SAKURA_CSV_URLS = [
  // JMA publishes annual sakura observation tables; column structure varies year-to-year.
  'https://www.data.jma.go.jp/sakura/data/sakura004_00.csv',
  'https://www.data.jma.go.jp/sakura/data/sakura004_06.csv',
  'https://www.data.jma.go.jp/sakura/data/sakura004_07.csv',
];
const WN_SAKURA_URLS = [
  'https://weathernews.jp/s/topics/sakura/json/forecast.json',
  'https://weathernews.jp/sakura/json/spots.json',
  'https://weathernews.jp/s/topics/sakura/json/spot_data.json',
];

// JMA 標本木 stations (58) — name → coords. Used to geocode the JMA CSV rows.
const JMA_STATION_COORDS = {
  '稚内': [45.4156, 141.6731], '旭川': [43.7706, 142.3650], '札幌': [43.0618, 141.3545],
  '帯広': [42.9237, 143.1965], '釧路': [42.9849, 144.3820], '函館': [41.7686, 140.7286],
  '青森': [40.8244, 140.7400], '盛岡': [39.7036, 141.1527], '仙台': [38.2683, 140.8694],
  '秋田': [39.7186, 140.1023], '山形': [38.2407, 140.3633], '福島': [37.7503, 140.4675],
  '東京': [35.6944, 139.7436], '宇都宮': [36.5658, 139.8836], '前橋': [36.3911, 139.0608],
  '水戸': [36.3658, 140.4711], '熊谷': [36.1473, 139.3886], '銚子': [35.7339, 140.8567],
  '横浜': [35.4437, 139.6380], '長野': [36.6483, 138.1942], '甲府': [35.6642, 138.5683],
  '新潟': [37.9028, 139.0234], '富山': [36.6953, 137.2114], '金沢': [36.5614, 136.6564],
  '福井': [36.0612, 136.2226], '岐阜': [35.3911, 136.7222], '名古屋': [35.1815, 136.9066],
  '津': [34.7186, 136.5056], '彦根': [35.2756, 136.2592], '大津': [35.0044, 135.8686],
  '京都': [35.0116, 135.7681], '奈良': [34.6850, 135.8050], '大阪': [34.6937, 135.5023],
  '神戸': [34.6913, 135.1830], '和歌山': [34.2300, 135.1675], '岡山': [34.6551, 133.9195],
  '広島': [34.3853, 132.4553], '松江': [35.4722, 133.0506], '鳥取': [35.5036, 134.2383],
  '下関': [33.9577, 130.9408], '山口': [34.1858, 131.4706], '徳島': [34.0658, 134.5594],
  '高松': [34.3401, 134.0434], '松山': [33.8417, 132.7656], '高知': [33.5597, 133.5311],
  '福岡': [33.5904, 130.4017], '佐賀': [33.2494, 130.2989], '長崎': [32.7503, 129.8778],
  '熊本': [32.8032, 130.7079], '大分': [33.2381, 131.6126], '宮崎': [31.9077, 131.4203],
  '鹿児島': [31.5963, 130.5571], '名瀬': [28.3772, 129.4939], '沖縄': [26.2125, 127.6809],
  '宮古島': [24.7964, 125.2814], '石垣島': [24.3367, 124.1564], '南大東島': [25.8294, 131.2289],
  '宇和島': [33.2233, 132.5667],
};

async function tryWeatherNews() {
  for (const url of WN_SAKURA_URLS) {
    const data = await fetchJson(url, { timeoutMs: 10_000, retries: 1 });
    if (!data) continue;
    const arr = Array.isArray(data) ? data : (data.spots || data.list || data.spot || []);
    if (!Array.isArray(arr) || arr.length === 0) continue;
    const features = arr
      .map((s, i) => {
        const lon = Number(s.lon ?? s.lng ?? s.longitude ?? s.x);
        const lat = Number(s.lat ?? s.latitude ?? s.y);
        if (!Number.isFinite(lat) || !Number.isFinite(lon)) return null;
        return {
          type: 'Feature',
          geometry: { type: 'Point', coordinates: [lon, lat] },
          properties: {
            spot_id: `WN_${i + 1}`,
            name: s.name || s.spotName || s.title || null,
            bloom_date: s.bloom || s.bloomDate || s.kaika || null,
            full_bloom_date: s.fullBloom || s.fullBloomDate || s.mankai || null,
            prefecture: s.pref || s.prefecture || s.area || null,
            country: 'JP',
            source: 'weathernews_sakura',
          },
        };
      })
      .filter(Boolean);
    if (features.length > 0) return features;
  }
  return null;
}

async function tryJmaCsv() {
  for (const url of JMA_SAKURA_CSV_URLS) {
    const text = await fetchText(url, { timeoutMs: 10_000, retries: 1 });
    if (!text) continue;
    const rows = text.split(/\r?\n/).filter((r) => r.trim());
    const features = [];
    for (const row of rows) {
      const cols = row.split(',').map((c) => c.replace(/"/g, '').trim());
      const station = cols.find((c) => JMA_STATION_COORDS[c]);
      if (!station) continue;
      const coords = JMA_STATION_COORDS[station];
      // JMA bloom dates are formatted M/D within the same row
      const dateCells = cols.filter((c) => /^\d{1,2}\/\d{1,2}$/.test(c));
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [coords[1], coords[0]] },
        properties: {
          spot_id: `JMA_${station}`,
          name: `${station} 標本木`,
          station,
          bloom_date: dateCells[0] || null,
          full_bloom_date: dateCells[1] || null,
          country: 'JP',
          source: 'jma_sakura_csv',
        },
      });
    }
    if (features.length > 0) return features;
  }
  return null;
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
  const wn = await tryWeatherNews();
  const jma = await tryJmaCsv();
  const seen = new Set();
  const features = [];
  for (const f of [...(wn || []), ...(jma || [])]) {
    const k = f.properties?.spot_id;
    if (!k || seen.has(k)) continue;
    seen.add(k);
    features.push(f);
  }
  const live = features.length > 0;
  if (!live) features.push(...generateSeedData());
  const liveSrc = wn && jma ? 'weathernews+jma' : (wn ? 'weathernews_sakura' : (jma ? 'jma_sakura_csv' : 'sakura_front_seed'));
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'sakura-front',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: liveSrc,
      counts: { weathernews: wn?.length || 0, jma: jma?.length || 0 },
      description: 'Sakura zensen - cherry blossom progression front across Japan',
    },
    metadata: {},
  };
}
