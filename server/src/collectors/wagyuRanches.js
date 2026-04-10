/**
 * Wagyu Ranches Collector
 * Certified wagyu production regions - Matsusaka, Kobe, Omi, Hida, Yonezawa, Maesawa, etc.
 * Live: No national registry; uses prefectural/regional certification associations.
 */

import { fetchJson } from './_liveHelpers.js';

const JLEC_API = 'https://www.jlec.net/api/wagyu-regions.json';

async function tryLiveJLEC() {
  const data = await fetchJson(JLEC_API, { timeoutMs: 8000 });
  if (!data || !Array.isArray(data?.regions)) return null;
  return data.regions.slice(0, 200).map((r, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [r.lon, r.lat] },
    properties: {
      ranch_id: `JLEC_${i + 1}`,
      name: r.name,
      brand: r.brand,
      certification: r.certification,
      head_count: r.head_count,
      prefecture: r.prefecture,
      country: 'JP',
      source: 'jlec_wagyu',
    },
  }));
}

// Curated: major certified wagyu brands by production region
const SEED_RANCHES = [
  // Top-tier (三大和牛 + famous brands)
  { name: '松阪牛 産地', brand: 'matsusaka', lat: 34.5781, lon: 136.5406, prefecture: '三重県', city: '松阪市', cert_body: '松阪牛協議会', head_count: 7500, tier: 'premium' },
  { name: '神戸ビーフ 産地 (但馬牛)', brand: 'kobe', lat: 35.5444, lon: 134.8231, prefecture: '兵庫県', city: '但馬地域', cert_body: '神戸肉流通推進協議会', head_count: 6000, tier: 'premium' },
  { name: '近江牛 産地', brand: 'omi', lat: 35.2725, lon: 136.2558, prefecture: '滋賀県', city: '近江八幡市', cert_body: '近江牛生産・流通推進協議会', head_count: 25000, tier: 'premium' },
  { name: '飛騨牛 産地', brand: 'hida', lat: 36.1414, lon: 137.2528, prefecture: '岐阜県', city: '高山市', cert_body: '飛騨牛銘柄推進協議会', head_count: 17000, tier: 'premium' },
  { name: '米沢牛 産地', brand: 'yonezawa', lat: 37.9219, lon: 140.1167, prefecture: '山形県', city: '米沢市', cert_body: '米沢牛銘柄推進協議会', head_count: 11000, tier: 'premium' },
  { name: '前沢牛 産地', brand: 'maesawa', lat: 39.1014, lon: 141.1333, prefecture: '岩手県', city: '奥州市前沢区', cert_body: '前沢牛普及推進協議会', head_count: 5500, tier: 'premium' },

  // Hokkaido
  { name: '十勝和牛 産地', brand: 'tokachi', lat: 42.9833, lon: 143.2244, prefecture: '北海道', city: '帯広市', cert_body: '十勝農協連', head_count: 25000, tier: 'regional' },
  { name: '白老牛 産地', brand: 'shiraoi', lat: 42.5500, lon: 141.3583, prefecture: '北海道', city: '白老町', cert_body: '白老牛銘柄推進協議会', head_count: 2500, tier: 'regional' },
  { name: 'びえい牛 産地', brand: 'biei', lat: 43.5881, lon: 142.4678, prefecture: '北海道', city: '美瑛町', cert_body: '美瑛町農協', head_count: 1500, tier: 'regional' },
  { name: 'はやきた牛 産地', brand: 'hayakita', lat: 42.7656, lon: 141.8039, prefecture: '北海道', city: '安平町', cert_body: '早来町農協', head_count: 1800, tier: 'regional' },

  // Tohoku
  { name: '仙台牛 産地', brand: 'sendai', lat: 38.2683, lon: 140.8719, prefecture: '宮城県', city: '仙台地域', cert_body: '仙台牛銘柄推進協議会', head_count: 14000, tier: 'regional' },
  { name: '山形牛 産地', brand: 'yamagata', lat: 38.2403, lon: 140.3633, prefecture: '山形県', city: '山形地域', cert_body: '山形県総合畜産公社', head_count: 15000, tier: 'regional' },
  { name: '秋田牛 産地', brand: 'akita', lat: 39.7186, lon: 140.1024, prefecture: '秋田県', city: '秋田地域', cert_body: '秋田県畜産農協', head_count: 8000, tier: 'regional' },
  { name: '短角牛 いわて (岩手短角)', brand: 'iwate_tankaku', lat: 40.0536, lon: 141.3700, prefecture: '岩手県', city: '久慈市', cert_body: '岩手県畜産課', head_count: 2500, tier: 'regional' },
  { name: '福島牛 産地', brand: 'fukushima', lat: 37.7503, lon: 140.4675, prefecture: '福島県', city: '福島地域', cert_body: '福島県畜産振興協会', head_count: 10000, tier: 'regional' },

  // Kanto / Chubu
  { name: '常陸牛 産地', brand: 'hitachi', lat: 36.2197, lon: 140.1344, prefecture: '茨城県', city: '水戸地域', cert_body: '茨城県肉用牛振興協会', head_count: 9500, tier: 'regional' },
  { name: '上州和牛 産地', brand: 'joshu', lat: 36.3911, lon: 139.0608, prefecture: '群馬県', city: '群馬地域', cert_body: '上州和牛協議会', head_count: 8500, tier: 'regional' },
  { name: 'とちぎ和牛 産地', brand: 'tochigi', lat: 36.5658, lon: 139.8836, prefecture: '栃木県', city: '那須塩原地域', cert_body: 'とちぎ和牛銘柄推進協議会', head_count: 6500, tier: 'regional' },
  { name: '信州プレミアム牛肉', brand: 'shinshu', lat: 36.6489, lon: 138.1944, prefecture: '長野県', city: '長野地域', cert_body: '長野県', head_count: 7500, tier: 'regional' },
  { name: '越後牛 産地', brand: 'echigo', lat: 37.9161, lon: 139.0364, prefecture: '新潟県', city: '新潟地域', cert_body: '新潟県総合畜産', head_count: 4000, tier: 'regional' },
  { name: '能登牛 産地', brand: 'noto', lat: 37.1983, lon: 136.9858, prefecture: '石川県', city: '能登地域', cert_body: '能登牛銘柄推進協議会', head_count: 1200, tier: 'regional' },
  { name: '若狭牛 産地', brand: 'wakasa', lat: 35.5147, lon: 135.7494, prefecture: '福井県', city: '若狭地域', cert_body: '福井県畜産課', head_count: 800, tier: 'regional' },
  { name: '美濃ヘルシービーフ', brand: 'mino', lat: 35.5458, lon: 136.8900, prefecture: '岐阜県', city: '美濃地域', cert_body: '岐阜県', head_count: 2000, tier: 'regional' },

  // Kansai
  { name: '伊賀牛 産地', brand: 'iga', lat: 34.7658, lon: 136.1375, prefecture: '三重県', city: '伊賀市', cert_body: '伊賀肉牛流通協議会', head_count: 1200, tier: 'regional' },
  { name: '大和牛 産地', brand: 'yamato', lat: 34.6850, lon: 135.8048, prefecture: '奈良県', city: '奈良地域', cert_body: '大和畜産', head_count: 900, tier: 'regional' },
  { name: '熊野牛 産地', brand: 'kumano', lat: 34.0081, lon: 135.7244, prefecture: '和歌山県', city: '田辺市', cert_body: '熊野牛振興協議会', head_count: 500, tier: 'regional' },
  { name: '淡路ビーフ 産地', brand: 'awaji', lat: 34.3700, lon: 134.8978, prefecture: '兵庫県', city: '淡路島', cert_body: '淡路ビーフ振興協議会', head_count: 2500, tier: 'regional' },

  // Chugoku
  { name: '千屋牛 産地', brand: 'chiya', lat: 34.9892, lon: 133.4736, prefecture: '岡山県', city: '新見市', cert_body: '千屋牛振興協議会', head_count: 1800, tier: 'regional' },
  { name: '鳥取和牛 産地', brand: 'tottori', lat: 35.5039, lon: 134.2378, prefecture: '鳥取県', city: '鳥取地域', cert_body: '鳥取県畜産農協', head_count: 6500, tier: 'regional' },
  { name: '見蘭牛 島根和牛', brand: 'shimane', lat: 35.4722, lon: 133.0506, prefecture: '島根県', city: '島根地域', cert_body: '島根県畜産技術連盟', head_count: 9500, tier: 'regional' },
  { name: '広島牛 産地', brand: 'hiroshima', lat: 34.3853, lon: 132.4553, prefecture: '広島県', city: '広島地域', cert_body: '広島県', head_count: 4000, tier: 'regional' },
  { name: '見島牛 (特別天然記念物)', brand: 'mishima', lat: 34.7783, lon: 131.1517, prefecture: '山口県', city: '萩市見島', cert_body: '萩市', head_count: 80, tier: 'heritage' },

  // Shikoku
  { name: '阿波牛 産地', brand: 'awa', lat: 34.0658, lon: 134.5594, prefecture: '徳島県', city: '徳島地域', cert_body: '徳島県畜産会', head_count: 2500, tier: 'regional' },
  { name: '讃岐牛 産地', brand: 'sanuki', lat: 34.3401, lon: 134.0434, prefecture: '香川県', city: '高松地域', cert_body: '讃岐牛振興協議会', head_count: 2800, tier: 'regional' },
  { name: '伊予牛 絹の味', brand: 'iyo', lat: 33.8392, lon: 132.7656, prefecture: '愛媛県', city: '松山地域', cert_body: '伊予牛絹の味協議会', head_count: 3500, tier: 'regional' },
  { name: '土佐あかうし', brand: 'tosa_akaushi', lat: 33.5594, lon: 133.5311, prefecture: '高知県', city: '高知地域', cert_body: '土佐あかうし振興協議会', head_count: 1800, tier: 'rare_breed' },

  // Kyushu
  { name: '博多和牛 産地', brand: 'hakata', lat: 33.5904, lon: 130.4017, prefecture: '福岡県', city: '福岡地域', cert_body: '博多和牛推進協議会', head_count: 7500, tier: 'regional' },
  { name: '佐賀牛 産地', brand: 'saga', lat: 33.2494, lon: 130.2989, prefecture: '佐賀県', city: '佐賀地域', cert_body: 'JAグループ佐賀', head_count: 10000, tier: 'premium' },
  { name: '長崎和牛 産地', brand: 'nagasaki', lat: 32.7503, lon: 129.8775, prefecture: '長崎県', city: '長崎地域', cert_body: '長崎和牛銘柄推進協議会', head_count: 13000, tier: 'regional' },
  { name: 'くまもとあか牛', brand: 'kumamoto_akaushi', lat: 32.8019, lon: 130.7256, prefecture: '熊本県', city: '阿蘇地域', cert_body: '熊本県', head_count: 14000, tier: 'rare_breed' },
  { name: '豊後牛 産地', brand: 'bungo', lat: 33.2381, lon: 131.6126, prefecture: '大分県', city: '大分地域', cert_body: '豊後牛流通促進対策協議会', head_count: 8500, tier: 'regional' },
  { name: '宮崎牛 産地', brand: 'miyazaki', lat: 31.9111, lon: 131.4239, prefecture: '宮崎県', city: '宮崎地域', cert_body: '宮崎県畜産協会', head_count: 84000, tier: 'premium' },
  { name: '鹿児島黒牛 産地', brand: 'kagoshima', lat: 31.5963, lon: 130.5571, prefecture: '鹿児島県', city: '鹿児島地域', cert_body: '鹿児島県', head_count: 120000, tier: 'premium' },

  // Okinawa
  { name: '石垣牛 産地', brand: 'ishigaki', lat: 24.3450, lon: 124.1558, prefecture: '沖縄県', city: '石垣市', cert_body: '石垣牛銘柄推進協議会', head_count: 4500, tier: 'regional' },
  { name: 'もとぶ牛 産地', brand: 'motobu', lat: 26.6544, lon: 127.8778, prefecture: '沖縄県', city: '本部町', cert_body: '本部町', head_count: 1500, tier: 'regional' },
];

function generateSeedData() {
  return SEED_RANCHES.map((r, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [r.lon, r.lat] },
    properties: {
      ranch_id: `WAGYU_${String(i + 1).padStart(4, '0')}`,
      name: r.name,
      brand: r.brand,
      tier: r.tier,
      head_count: r.head_count,
      cert_body: r.cert_body,
      city: r.city,
      prefecture: r.prefecture,
      country: 'JP',
      source: 'wagyu_cert_seed',
    },
  }));
}

export default async function collectWagyuRanches() {
  let features = await tryLiveJLEC();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'wagyu-ranches',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'jlec_wagyu' : 'wagyu_cert_seed',
      description: 'Certified wagyu brand production regions - Matsusaka, Kobe, Omi, Hida and prefectural brands',
    },
    metadata: {},
  };
}
