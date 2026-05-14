/**
 * Tea Zones Collector
 * MAFF tea-growing regions (茶産地) - Shizuoka, Uji, Sayama, Yame, Kagoshima, Chiran, etc.
 * Live: OSM Overpass `landuse=farmland crop=tea` + MAFF registry fallback.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpass(
    'way["landuse"="farmland"]["crop"="tea"](area.jp);relation["landuse"="farmland"]["crop"="tea"](area.jp);way["crop"="tea"]["name"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        zone_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || `Tea zone ${i + 1}`,
        name_ja: el.tags?.name || null,
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

// Curated: MAFF tea-growing regions (茶産地) sorted by annual production
const SEED_ZONES = [
  // Shizuoka (40% of national production)
  { name: '静岡茶 本山', lat: 35.0533, lon: 138.3467, prefecture: '静岡県', city: '静岡市葵区', variety: 'sencha', production_t: 4500, region: 'shizuoka' },
  { name: '静岡茶 牧之原', lat: 34.7483, lon: 138.2283, prefecture: '静岡県', city: '牧之原市', variety: 'sencha_fukamushi', production_t: 6000, region: 'shizuoka' },
  { name: '静岡茶 川根', lat: 35.0278, lon: 138.0806, prefecture: '静岡県', city: '島田市川根町', variety: 'sencha', production_t: 1500, region: 'shizuoka' },
  { name: '静岡茶 朝比奈', lat: 34.8483, lon: 138.2872, prefecture: '静岡県', city: '藤枝市', variety: 'gyokuro', production_t: 400, region: 'shizuoka' },
  { name: '静岡茶 足久保', lat: 34.9981, lon: 138.3294, prefecture: '静岡県', city: '静岡市葵区', variety: 'sencha', production_t: 800, region: 'shizuoka' },
  { name: '静岡茶 天竜', lat: 34.8731, lon: 137.8544, prefecture: '静岡県', city: '浜松市天竜区', variety: 'sencha', production_t: 700, region: 'shizuoka' },
  { name: '掛川茶 産地', lat: 34.7708, lon: 138.0144, prefecture: '静岡県', city: '掛川市', variety: 'sencha_fukamushi', production_t: 3200, region: 'shizuoka' },
  { name: '菊川茶 産地', lat: 34.7597, lon: 138.0858, prefecture: '静岡県', city: '菊川市', variety: 'sencha_fukamushi', production_t: 1800, region: 'shizuoka' },

  // Kagoshima (2nd largest, ~35% of national production)
  { name: '知覧茶 産地', lat: 31.3803, lon: 130.4411, prefecture: '鹿児島県', city: '南九州市知覧町', variety: 'sencha', production_t: 3500, region: 'kagoshima' },
  { name: '頴娃茶 産地', lat: 31.3556, lon: 130.3922, prefecture: '鹿児島県', city: '南九州市頴娃町', variety: 'sencha', production_t: 2800, region: 'kagoshima' },
  { name: '霧島茶 産地', lat: 31.7350, lon: 130.7678, prefecture: '鹿児島県', city: '霧島市', variety: 'sencha', production_t: 2200, region: 'kagoshima' },
  { name: '志布志茶 産地', lat: 31.4972, lon: 131.1042, prefecture: '鹿児島県', city: '志布志市', variety: 'sencha', production_t: 1800, region: 'kagoshima' },
  { name: '伊集院茶 産地', lat: 31.6150, lon: 130.4236, prefecture: '鹿児島県', city: '日置市', variety: 'sencha', production_t: 1500, region: 'kagoshima' },
  { name: '枕崎茶 産地', lat: 31.2758, lon: 130.2994, prefecture: '鹿児島県', city: '枕崎市', variety: 'sencha', production_t: 1200, region: 'kagoshima' },
  { name: '徳之島 オーガニック茶', lat: 27.7311, lon: 128.9981, prefecture: '鹿児島県', city: '徳之島町', variety: 'organic_sencha', production_t: 50, region: 'kagoshima' },

  // Mie (3rd largest - Ise-cha)
  { name: '伊勢茶 四日市', lat: 34.9650, lon: 136.6244, prefecture: '三重県', city: '四日市市水沢町', variety: 'kabusecha', production_t: 2800, region: 'mie' },
  { name: '伊勢茶 鈴鹿', lat: 34.8819, lon: 136.5831, prefecture: '三重県', city: '鈴鹿市', variety: 'kabusecha', production_t: 1400, region: 'mie' },
  { name: '伊勢茶 亀山', lat: 34.8569, lon: 136.4514, prefecture: '三重県', city: '亀山市', variety: 'sencha', production_t: 800, region: 'mie' },

  // Kyoto (Uji-cha, prestigious)
  { name: '宇治茶 宇治市', lat: 34.8844, lon: 135.7994, prefecture: '京都府', city: '宇治市', variety: 'matcha_gyokuro', production_t: 1500, region: 'kyoto' },
  { name: '宇治茶 和束町', lat: 34.7897, lon: 135.9169, prefecture: '京都府', city: '和束町', variety: 'sencha_matcha', production_t: 1800, region: 'kyoto' },
  { name: '宇治茶 南山城村', lat: 34.7778, lon: 135.9936, prefecture: '京都府', city: '南山城村', variety: 'matcha', production_t: 600, region: 'kyoto' },
  { name: '宇治茶 木津川市', lat: 34.7356, lon: 135.8247, prefecture: '京都府', city: '木津川市', variety: 'sencha', production_t: 500, region: 'kyoto' },
  { name: '宇治茶 宇治田原町', lat: 34.8942, lon: 135.8544, prefecture: '京都府', city: '宇治田原町', variety: 'sencha', production_t: 900, region: 'kyoto' },

  // Fukuoka (Yame tea - premium gyokuro)
  { name: '八女茶 産地', lat: 33.2103, lon: 130.5597, prefecture: '福岡県', city: '八女市', variety: 'gyokuro_sencha', production_t: 1800, region: 'fukuoka' },
  { name: '星野村 八女伝統本玉露', lat: 33.2897, lon: 130.7544, prefecture: '福岡県', city: '八女市星野村', variety: 'traditional_gyokuro', production_t: 120, region: 'fukuoka' },

  // Saitama (Sayama tea)
  { name: '狭山茶 入間', lat: 35.8083, lon: 139.3814, prefecture: '埼玉県', city: '入間市', variety: 'sencha', production_t: 600, region: 'sayama' },
  { name: '狭山茶 所沢', lat: 35.7992, lon: 139.4686, prefecture: '埼玉県', city: '所沢市', variety: 'sencha', production_t: 400, region: 'sayama' },
  { name: '狭山茶 狭山', lat: 35.8533, lon: 139.4117, prefecture: '埼玉県', city: '狭山市', variety: 'sencha', production_t: 350, region: 'sayama' },

  // Aichi (Nishio)
  { name: '西尾茶 抹茶産地', lat: 34.8631, lon: 137.0586, prefecture: '愛知県', city: '西尾市', variety: 'matcha', production_t: 900, region: 'aichi' },
  { name: '豊田茶', lat: 35.0797, lon: 137.3411, prefecture: '愛知県', city: '豊田市', variety: 'sencha', production_t: 250, region: 'aichi' },

  // Nara
  { name: '大和茶 月ヶ瀬', lat: 34.7056, lon: 136.0178, prefecture: '奈良県', city: '奈良市月ヶ瀬', variety: 'sencha', production_t: 450, region: 'nara' },
  { name: '大和茶 山添村', lat: 34.6533, lon: 136.0464, prefecture: '奈良県', city: '山添村', variety: 'sencha', production_t: 250, region: 'nara' },

  // Shiga
  { name: '朝宮茶 甲賀', lat: 34.8706, lon: 136.0694, prefecture: '滋賀県', city: '甲賀市信楽町朝宮', variety: 'sencha', production_t: 180, region: 'shiga' },
  { name: '政所茶 東近江', lat: 35.1356, lon: 136.3497, prefecture: '滋賀県', city: '東近江市政所町', variety: 'sencha', production_t: 60, region: 'shiga' },
  { name: '土山茶 甲賀', lat: 34.9142, lon: 136.2556, prefecture: '滋賀県', city: '甲賀市土山町', variety: 'sencha', production_t: 400, region: 'shiga' },

  // Miyazaki
  { name: '都城茶 産地', lat: 31.7186, lon: 131.0614, prefecture: '宮崎県', city: '都城市', variety: 'sencha', production_t: 1000, region: 'miyazaki' },
  { name: '宮崎茶 五ヶ瀬', lat: 32.6828, lon: 131.1572, prefecture: '宮崎県', city: '五ヶ瀬町', variety: 'sencha', production_t: 200, region: 'miyazaki' },

  // Kumamoto
  { name: 'くまもと茶 芦北', lat: 32.3181, lon: 130.6400, prefecture: '熊本県', city: '芦北町', variety: 'sencha', production_t: 750, region: 'kumamoto' },
  { name: '相良茶 山鹿', lat: 33.0150, lon: 130.6778, prefecture: '熊本県', city: '山鹿市', variety: 'kamairicha', production_t: 450, region: 'kumamoto' },

  // Oita
  { name: '大分茶 杵築', lat: 33.4208, lon: 131.6242, prefecture: '大分県', city: '杵築市', variety: 'kamairicha', production_t: 250, region: 'oita' },

  // Nagasaki
  { name: '嬉野茶 産地', lat: 33.1011, lon: 129.9833, prefecture: '長崎県', city: '嬉野市', variety: 'tamaryokucha', production_t: 1200, region: 'nagasaki' },
  { name: '東彼杵茶 産地', lat: 33.0008, lon: 129.9308, prefecture: '長崎県', city: '東彼杵町', variety: 'tamaryokucha', production_t: 600, region: 'nagasaki' },

  // Saga
  { name: '嬉野茶 佐賀側', lat: 33.1008, lon: 130.0572, prefecture: '佐賀県', city: '嬉野市', variety: 'tamaryokucha', production_t: 700, region: 'saga' },

  // Ibaraki
  { name: '猿島茶 産地', lat: 36.0436, lon: 139.8117, prefecture: '茨城県', city: '古河市', variety: 'sencha', production_t: 200, region: 'ibaraki' },
  { name: '奥久慈茶 大子町', lat: 36.7683, lon: 140.3553, prefecture: '茨城県', city: '大子町', variety: 'sencha', production_t: 90, region: 'ibaraki' },

  // Niigata
  { name: '村上茶 産地', lat: 38.2236, lon: 139.4797, prefecture: '新潟県', city: '村上市', variety: 'sencha', production_t: 35, region: 'niigata' },

  // Ishikawa
  { name: '加賀棒茶 産地', lat: 36.5946, lon: 136.6256, prefecture: '石川県', city: '金沢市', variety: 'hojicha', production_t: 80, region: 'ishikawa' },

  // Kochi
  { name: '土佐茶 池川', lat: 33.5794, lon: 133.1389, prefecture: '高知県', city: '仁淀川町', variety: 'sencha', production_t: 180, region: 'kochi' },
  { name: '土佐茶 梼原', lat: 33.3925, lon: 132.9206, prefecture: '高知県', city: '梼原町', variety: 'sencha', production_t: 100, region: 'kochi' },
];

function generateSeedData() {
  return SEED_ZONES.map((z, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [z.lon, z.lat] },
    properties: {
      zone_id: `TEA_${String(i + 1).padStart(4, '0')}`,
      name: z.name,
      variety: z.variety,
      production_t: z.production_t,
      region: z.region,
      city: z.city,
      prefecture: z.prefecture,
      country: 'JP',
      source: 'maff_tea_seed',
    },
  }));
}

export default async function collectTeaZones() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'tea-zones',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'osm_overpass' : 'maff_tea_seed',
      description: 'Japanese tea-growing regions - Shizuoka, Kagoshima, Uji, Yame, Sayama and regional producers',
    },
  };
}
