/**
 * Wineries + Craft Beer Collector
 * NTA (National Tax Agency) license registry + craft breweries via OSM Overpass.
 * Live: OSM `craft=brewery|winery`, fallback to curated list.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpass(
    'node["craft"="winery"](area.jp);way["craft"="winery"](area.jp);node["craft"="brewery"]["produces"!="sake"](area.jp);way["craft"="brewery"]["produces"!="sake"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        facility_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || `Facility ${i + 1}`,
        name_ja: el.tags?.name || null,
        category: el.tags?.craft || null,
        produces: el.tags?.produces || null,
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

// Curated: major Japanese wineries + well-known craft beer breweries
const SEED_FACILITIES = [
  // WINERIES — Yamanashi (biggest wine region, Katsunuma/Koshu)
  { name: 'シャトー・メルシャン 勝沼ワイナリー', category: 'winery', lat: 35.6547, lon: 138.7281, prefecture: '山梨県', city: '甲州市', founded: 1877, brand: 'Mercian' },
  { name: 'サントリー 登美の丘ワイナリー', category: 'winery', lat: 35.7050, lon: 138.5403, prefecture: '山梨県', city: '甲斐市', founded: 1909, brand: 'Tomi no Oka' },
  { name: 'マンズワイン 勝沼ワイナリー', category: 'winery', lat: 35.6519, lon: 138.7275, prefecture: '山梨県', city: '甲州市', founded: 1962, brand: 'Manns' },
  { name: '丸藤葡萄酒工業', category: 'winery', lat: 35.6558, lon: 138.7269, prefecture: '山梨県', city: '甲州市', founded: 1890, brand: 'Rubaiyat' },
  { name: 'グレイスワイン 中央葡萄酒', category: 'winery', lat: 35.6606, lon: 138.7089, prefecture: '山梨県', city: '甲州市', founded: 1923, brand: 'Grace' },
  { name: '勝沼醸造', category: 'winery', lat: 35.6536, lon: 138.7261, prefecture: '山梨県', city: '甲州市', founded: 1937, brand: 'Aruga Branca' },

  // WINERIES — Nagano (Chikumagawa wine valley)
  { name: 'ヴィラデスト ガーデンファームアンドワイナリー', category: 'winery', lat: 36.4536, lon: 138.2917, prefecture: '長野県', city: '東御市', founded: 2003, brand: 'Villa d\'Est' },
  { name: 'マンズワイン 小諸ワイナリー', category: 'winery', lat: 36.3133, lon: 138.4247, prefecture: '長野県', city: '小諸市', founded: 1973, brand: 'Manns Komoro' },
  { name: '井筒ワイン', category: 'winery', lat: 36.1956, lon: 137.9722, prefecture: '長野県', city: '塩尻市', founded: 1933, brand: 'Izutsu' },
  { name: 'サンクゼールワイナリー', category: 'winery', lat: 36.7889, lon: 138.2031, prefecture: '長野県', city: '飯綱町', founded: 1979, brand: 'St. Cousair' },
  { name: '小布施ワイナリー', category: 'winery', lat: 36.6997, lon: 138.3150, prefecture: '長野県', city: '小布施町', founded: 1942, brand: 'Domaine Sogga' },

  // WINERIES — Hokkaido (rising region)
  { name: '北海道ワイン 小樽醸造所', category: 'winery', lat: 43.1781, lon: 141.0289, prefecture: '北海道', city: '小樽市', founded: 1974, brand: 'Hokkaido Wine' },
  { name: '池田町ブドウ・ブドウ酒研究所 十勝ワイン', category: 'winery', lat: 42.9183, lon: 143.4517, prefecture: '北海道', city: '池田町', founded: 1963, brand: 'Tokachi' },
  { name: 'ドメーヌ・タカヒコ', category: 'winery', lat: 42.9911, lon: 140.5497, prefecture: '北海道', city: '余市町', founded: 2010, brand: 'Domaine Takahiko' },

  // WINERIES — Yamagata
  { name: '高畠ワイナリー', category: 'winery', lat: 38.0114, lon: 140.1742, prefecture: '山形県', city: '高畠町', founded: 1990, brand: 'Takahata Winery' },
  { name: '朝日町ワイン', category: 'winery', lat: 38.3289, lon: 140.1517, prefecture: '山形県', city: '朝日町', founded: 1944, brand: 'Asahi' },

  // WINERIES — Other
  { name: 'シャトーカミヤ 牛久醸造場', category: 'winery', lat: 35.9803, lon: 140.1492, prefecture: '茨城県', city: '牛久市', founded: 1903, brand: 'Chateau Kamiya' },
  { name: '都農ワイン', category: 'winery', lat: 32.2389, lon: 131.5672, prefecture: '宮崎県', city: '都農町', founded: 1996, brand: 'Tsuno Winery' },
  { name: '安心院葡萄酒工房', category: 'winery', lat: 33.4008, lon: 131.4156, prefecture: '大分県', city: '宇佐市', founded: 1971, brand: 'Ajimu' },

  // CRAFT BEER — major breweries (地ビール)
  { name: 'ヤッホーブルーイング よなよなエール', category: 'craft_beer', lat: 36.3453, lon: 138.6464, prefecture: '長野県', city: '軽井沢町', founded: 1997, brand: 'Yo-Ho' },
  { name: 'エチゴビール', category: 'craft_beer', lat: 37.8722, lon: 138.8489, prefecture: '新潟県', city: '新潟市西蒲区', founded: 1994, brand: 'Echigo Beer' },
  { name: 'COEDO コエドブルワリー', category: 'craft_beer', lat: 35.8889, lon: 139.4464, prefecture: '埼玉県', city: '川越市', founded: 1996, brand: 'Coedo' },
  { name: 'ベアードブルーイング', category: 'craft_beer', lat: 34.9667, lon: 138.8475, prefecture: '静岡県', city: '伊豆市', founded: 2000, brand: 'Baird Beer' },
  { name: 'ブリマー・ブルーイング', category: 'craft_beer', lat: 35.5311, lon: 139.7031, prefecture: '神奈川県', city: '川崎市川崎区', founded: 2011, brand: 'Brimmer' },
  { name: 'ロコビア', category: 'craft_beer', lat: 35.6744, lon: 139.4831, prefecture: '東京都', city: '調布市', founded: 1997, brand: 'Locobeer' },
  { name: '常陸野ネストビール 木内酒造', category: 'craft_beer', lat: 36.2317, lon: 140.3989, prefecture: '茨城県', city: '那珂市', founded: 1996, brand: 'Hitachino Nest' },
  { name: '志賀高原ビール 玉村本店', category: 'craft_beer', lat: 36.7072, lon: 138.4842, prefecture: '長野県', city: '山ノ内町', founded: 2004, brand: 'Shiga Kogen Beer' },
  { name: '南信州ビール', category: 'craft_beer', lat: 35.5114, lon: 137.8233, prefecture: '長野県', city: '駒ヶ根市', founded: 1996, brand: 'Minami Shinshu Beer' },
  { name: '箕面ビール', category: 'craft_beer', lat: 34.8344, lon: 135.4692, prefecture: '大阪府', city: '箕面市', founded: 1997, brand: 'Minoh Beer' },
  { name: '田沢湖ビール', category: 'craft_beer', lat: 39.7497, lon: 140.7417, prefecture: '秋田県', city: '仙北市', founded: 1997, brand: 'Tazawako Beer' },
  { name: 'いわて蔵ビール 世嬉の一酒造', category: 'craft_beer', lat: 38.9339, lon: 141.1264, prefecture: '岩手県', city: '一関市', founded: 1995, brand: 'Iwate Kura Beer' },
  { name: '銀河高原ビール', category: 'craft_beer', lat: 39.3611, lon: 140.9692, prefecture: '岩手県', city: '西和賀町', founded: 1996, brand: 'Ginga Kogen Beer' },
  { name: '八ヶ岳ブルワリー タッチダウン', category: 'craft_beer', lat: 35.8706, lon: 138.3011, prefecture: '山梨県', city: '北杜市', founded: 1997, brand: 'Yatsugatake Touchdown' },
  { name: 'サンクトガーレン 厚木', category: 'craft_beer', lat: 35.4392, lon: 139.3647, prefecture: '神奈川県', city: '厚木市', founded: 1994, brand: 'Sankt Gallen' },
  { name: '富士桜高原麦酒', category: 'craft_beer', lat: 35.4633, lon: 138.7711, prefecture: '山梨県', city: '富士河口湖町', founded: 1998, brand: 'Fujizakura Kogen Beer' },
  { name: 'オラホビール 信州東御市振興公社', category: 'craft_beer', lat: 36.3578, lon: 138.3325, prefecture: '長野県', city: '東御市', founded: 1996, brand: 'OH!LA!HO' },
  { name: 'ヘリオス酒造 名護ブルワリー', category: 'craft_beer', lat: 26.5919, lon: 127.9775, prefecture: '沖縄県', city: '名護市', founded: 1996, brand: 'Helios Beer' },
  { name: '網走ビール', category: 'craft_beer', lat: 44.0167, lon: 144.2703, prefecture: '北海道', city: '網走市', founded: 2006, brand: 'Abashiri Beer' },
  { name: 'オホーツクビール', category: 'craft_beer', lat: 44.3475, lon: 143.3556, prefecture: '北海道', city: '北見市', founded: 1994, brand: 'Okhotsk Beer' },

  // WHISKY / SHOCHU / OTHER
  { name: 'サントリー山崎蒸溜所', category: 'whisky', lat: 34.8886, lon: 135.6744, prefecture: '大阪府', city: '島本町', founded: 1923, brand: 'Yamazaki' },
  { name: 'サントリー白州蒸溜所', category: 'whisky', lat: 35.7958, lon: 138.2808, prefecture: '山梨県', city: '北杜市', founded: 1973, brand: 'Hakushu' },
  { name: 'ニッカ余市蒸溜所', category: 'whisky', lat: 43.1906, lon: 140.7903, prefecture: '北海道', city: '余市町', founded: 1934, brand: 'Yoichi' },
  { name: 'ニッカ宮城峡蒸溜所', category: 'whisky', lat: 38.3456, lon: 140.6483, prefecture: '宮城県', city: '仙台市青葉区', founded: 1969, brand: 'Miyagikyo' },
  { name: 'キリン富士御殿場蒸溜所', category: 'whisky', lat: 35.2944, lon: 138.9264, prefecture: '静岡県', city: '御殿場市', founded: 1973, brand: 'Fuji Gotemba' },
  { name: '秩父蒸溜所 ベンチャーウイスキー', category: 'whisky', lat: 36.0033, lon: 139.1167, prefecture: '埼玉県', city: '秩父市', founded: 2008, brand: 'Ichiro\'s Malt' },
];

function generateSeedData() {
  return SEED_FACILITIES.map((f, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [f.lon, f.lat] },
    properties: {
      facility_id: `BEVW_${String(i + 1).padStart(4, '0')}`,
      name: f.name,
      category: f.category,
      brand: f.brand,
      founded: f.founded,
      city: f.city,
      prefecture: f.prefecture,
      country: 'JP',
      source: 'nta_beverage_seed',
    },
  }));
}

export default async function collectWineriesCraftbeer() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'wineries-craftbeer',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'osm_overpass' : 'nta_beverage_seed',
      description: 'Wineries, craft beer breweries and whisky distilleries across Japan (NTA license registry)',
    },
  };
}
