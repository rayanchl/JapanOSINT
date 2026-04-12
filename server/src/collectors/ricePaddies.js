/**
 * Rice Paddies Collector
 * MAFF rice cropping mesh - major rice-producing regions and premium brand areas.
 * Live: OSM Overpass `landuse=farmland crop=rice` polygon centroids.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpass(
    'way["landuse"="farmland"]["crop"="rice"](area.jp);relation["landuse"="farmland"]["crop"="rice"](area.jp);way["crop"="rice"]["name"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        paddy_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || `Rice paddy ${i + 1}`,
        name_ja: el.tags?.name || null,
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

// Curated: MAFF major rice-producing regions with cultivar/brand + tanada (terraced rice fields)
const SEED_PADDIES = [
  // Niigata - largest production region (Koshihikari)
  { name: '魚沼 コシヒカリ産地', lat: 37.2500, lon: 138.9633, prefecture: '新潟県', city: '魚沼市', cultivar: 'koshihikari', brand: 'uonuma_koshi', area_ha: 11000, production_t: 55000, grade: 'special_a' },
  { name: '南魚沼 コシヒカリ産地', lat: 37.0875, lon: 138.9456, prefecture: '新潟県', city: '南魚沼市', cultivar: 'koshihikari', brand: 'minami_uonuma', area_ha: 8000, production_t: 40000, grade: 'special_a' },
  { name: '岩船 コシヒカリ', lat: 38.2236, lon: 139.4797, prefecture: '新潟県', city: '村上市', cultivar: 'koshihikari', brand: 'iwafune', area_ha: 5000, production_t: 25000, grade: 'special_a' },
  { name: '佐渡 コシヒカリ', lat: 38.0322, lon: 138.3681, prefecture: '新潟県', city: '佐渡市', cultivar: 'koshihikari', brand: 'sado_toki', area_ha: 6000, production_t: 30000, grade: 'special_a' },
  { name: '長岡 コシヒカリ', lat: 37.4466, lon: 138.8467, prefecture: '新潟県', city: '長岡市', cultivar: 'koshihikari', brand: 'nagaoka', area_ha: 8500, production_t: 42500, grade: 'a' },
  { name: '新潟平野', lat: 37.9161, lon: 139.0364, prefecture: '新潟県', city: '新潟市', cultivar: 'koshihikari', brand: 'niigata_ippan', area_ha: 40000, production_t: 200000, grade: 'a' },

  // Hokkaido - 2nd largest (Nanatsuboshi, Yumepirika)
  { name: '空知平野 ななつぼし', lat: 43.3561, lon: 141.8300, prefecture: '北海道', city: '空知地域', cultivar: 'nanatsuboshi', brand: 'sorachi', area_ha: 28000, production_t: 150000, grade: 'special_a' },
  { name: '上川 ゆめぴりか', lat: 43.8917, lon: 142.5125, prefecture: '北海道', city: '上川地域', cultivar: 'yumepirika', brand: 'kamikawa', area_ha: 20000, production_t: 105000, grade: 'special_a' },
  { name: '旭川盆地 ゆめぴりか', lat: 43.7706, lon: 142.3650, prefecture: '北海道', city: '旭川市', cultivar: 'yumepirika', brand: 'asahikawa', area_ha: 10000, production_t: 55000, grade: 'special_a' },
  { name: '富良野 ゆめぴりか', lat: 43.3417, lon: 142.3836, prefecture: '北海道', city: '富良野市', cultivar: 'yumepirika', brand: 'furano', area_ha: 4000, production_t: 22000, grade: 'a' },
  { name: '岩見沢 ななつぼし', lat: 43.1961, lon: 141.7750, prefecture: '北海道', city: '岩見沢市', cultivar: 'nanatsuboshi', brand: 'iwamizawa', area_ha: 6500, production_t: 34000, grade: 'a' },

  // Akita (Akitakomachi)
  { name: '大潟村 あきたこまち', lat: 39.9983, lon: 139.9983, prefecture: '秋田県', city: '大潟村', cultivar: 'akitakomachi', brand: 'ogata', area_ha: 8900, production_t: 50000, grade: 'special_a' },
  { name: '横手盆地 あきたこまち', lat: 39.3181, lon: 140.5675, prefecture: '秋田県', city: '横手市', cultivar: 'akitakomachi', brand: 'yokote', area_ha: 11000, production_t: 58000, grade: 'special_a' },
  { name: '大仙市 あきたこまち', lat: 39.4531, lon: 140.4756, prefecture: '秋田県', city: '大仙市', cultivar: 'akitakomachi', brand: 'daisen', area_ha: 9500, production_t: 50000, grade: 'special_a' },

  // Yamagata (Tsuyahime, Haenuki)
  { name: '庄内平野 つや姫', lat: 38.7269, lon: 139.8264, prefecture: '山形県', city: '鶴岡市', cultivar: 'tsuyahime', brand: 'shonai', area_ha: 18000, production_t: 95000, grade: 'special_a' },
  { name: '山形盆地 はえぬき', lat: 38.2403, lon: 140.3633, prefecture: '山形県', city: '山形市', cultivar: 'haenuki', brand: 'yamagata_naichi', area_ha: 15000, production_t: 78000, grade: 'special_a' },
  { name: '置賜 雪若丸', lat: 38.0000, lon: 140.2500, prefecture: '山形県', city: '米沢市', cultivar: 'yukiwakamaru', brand: 'okitama', area_ha: 6000, production_t: 32000, grade: 'a' },

  // Iwate (Hitomebore)
  { name: '岩手 ひとめぼれ', lat: 39.5028, lon: 141.1361, prefecture: '岩手県', city: '奥州市', cultivar: 'hitomebore', brand: 'iwate_nanbu', area_ha: 12000, production_t: 62000, grade: 'special_a' },
  { name: '岩手 金色の風', lat: 39.6972, lon: 141.2069, prefecture: '岩手県', city: '花巻市', cultivar: 'kinnoironokaze', brand: 'iwate_premium', area_ha: 2000, production_t: 11000, grade: 'special_a' },

  // Miyagi (Sasanishiki, Hitomebore)
  { name: '宮城 大崎耕土', lat: 38.5742, lon: 140.9533, prefecture: '宮城県', city: '大崎市', cultivar: 'hitomebore', brand: 'osaki_sasanishiki', area_ha: 11000, production_t: 58000, grade: 'special_a' },
  { name: '登米 ひとめぼれ', lat: 38.7236, lon: 141.2667, prefecture: '宮城県', city: '登米市', cultivar: 'hitomebore', brand: 'tome', area_ha: 10500, production_t: 55000, grade: 'special_a' },

  // Fukushima
  { name: '会津 コシヒカリ', lat: 37.4869, lon: 139.9297, prefecture: '福島県', city: '会津若松市', cultivar: 'koshihikari', brand: 'aizu_koshi', area_ha: 13000, production_t: 68000, grade: 'special_a' },
  { name: '福島中通り', lat: 37.7503, lon: 140.4675, prefecture: '福島県', city: '福島市', cultivar: 'koshihikari', brand: 'fukushima_nakadori', area_ha: 15000, production_t: 78000, grade: 'a' },

  // Aomori (Seitenno Hekireki)
  { name: '津軽 青天の霹靂', lat: 40.6075, lon: 140.4639, prefecture: '青森県', city: '弘前市', cultivar: 'seitennohekireki', brand: 'tsugaru_premium', area_ha: 3500, production_t: 18000, grade: 'special_a' },
  { name: '津軽 まっしぐら', lat: 40.8244, lon: 140.7400, prefecture: '青森県', city: '青森市', cultivar: 'masshigura', brand: 'tsugaru', area_ha: 14000, production_t: 72000, grade: 'a' },

  // Chiba
  { name: '千葉北総 コシヒカリ', lat: 35.9211, lon: 140.2350, prefecture: '千葉県', city: '成田地域', cultivar: 'koshihikari', brand: 'hokuso', area_ha: 15000, production_t: 78000, grade: 'a' },
  { name: '九十九里 ふさこがね', lat: 35.4681, lon: 140.3911, prefecture: '千葉県', city: '山武地域', cultivar: 'fusakogane', brand: 'kujukuri', area_ha: 8000, production_t: 42000, grade: 'a' },

  // Ibaraki
  { name: '茨城 コシヒカリ', lat: 36.2197, lon: 140.1344, prefecture: '茨城県', city: '水戸地域', cultivar: 'koshihikari', brand: 'ibaraki', area_ha: 30000, production_t: 160000, grade: 'a' },

  // Tochigi
  { name: '栃木 なすひかり', lat: 36.9333, lon: 140.0639, prefecture: '栃木県', city: '那須塩原市', cultivar: 'nasuhikari', brand: 'tochigi_premium', area_ha: 8000, production_t: 42000, grade: 'special_a' },

  // Toyama (Tentakaku, Koshihikari)
  { name: '富山平野 コシヒカリ', lat: 36.6953, lon: 137.2113, prefecture: '富山県', city: '富山市', cultivar: 'koshihikari', brand: 'toyama', area_ha: 16000, production_t: 85000, grade: 'a' },

  // Ishikawa
  { name: '加賀平野 ひゃくまん穀', lat: 36.5946, lon: 136.6256, prefecture: '石川県', city: '金沢市', cultivar: 'hyakumangoku', brand: 'kaga_hyakumangoku', area_ha: 7500, production_t: 39000, grade: 'special_a' },

  // Fukui (Ichihomare)
  { name: '福井 いちほまれ', lat: 36.0652, lon: 136.2216, prefecture: '福井県', city: '福井市', cultivar: 'ichihomare', brand: 'fukui_premium', area_ha: 4500, production_t: 23000, grade: 'special_a' },

  // Nagano
  { name: '信州 風さやか', lat: 36.6489, lon: 138.1944, prefecture: '長野県', city: '長野地域', cultivar: 'kazesayaka', brand: 'shinshu_premium', area_ha: 3000, production_t: 16000, grade: 'special_a' },
  { name: '北信 コシヒカリ', lat: 36.7058, lon: 138.4350, prefecture: '長野県', city: '中野市', cultivar: 'koshihikari', brand: 'hokushin', area_ha: 10000, production_t: 52000, grade: 'a' },

  // Niigata mountain / Tanada
  { name: '星峠の棚田', lat: 37.1411, lon: 138.6767, prefecture: '新潟県', city: '十日町市', cultivar: 'koshihikari', brand: 'tanada', area_ha: 30, production_t: 150, grade: 'terraced' },
  { name: '松之山 棚田', lat: 37.0892, lon: 138.6089, prefecture: '新潟県', city: '十日町市松之山', cultivar: 'koshihikari', brand: 'tanada', area_ha: 20, production_t: 100, grade: 'terraced' },

  // Wakayama (Aragi no Tanada - famous terraced paddies)
  { name: 'あらぎ島 棚田', lat: 34.0897, lon: 135.4525, prefecture: '和歌山県', city: '有田川町', cultivar: 'kinuhikari', brand: 'tanada', area_ha: 2, production_t: 10, grade: 'terraced_100' },

  // Saga
  { name: '佐賀平野 さがびより', lat: 33.2494, lon: 130.2989, prefecture: '佐賀県', city: '佐賀市', cultivar: 'sagabiyori', brand: 'saga', area_ha: 12000, production_t: 65000, grade: 'special_a' },

  // Kumamoto
  { name: '菊池 森のくまさん', lat: 32.9758, lon: 130.8167, prefecture: '熊本県', city: '菊池市', cultivar: 'morinokumasan', brand: 'kumamoto_premium', area_ha: 9000, production_t: 47000, grade: 'special_a' },
  { name: '阿蘇 コシヒカリ', lat: 32.9442, lon: 131.0703, prefecture: '熊本県', city: '阿蘇市', cultivar: 'koshihikari', brand: 'aso', area_ha: 2500, production_t: 13000, grade: 'a' },

  // Kagawa (rice terraces)
  { name: '中山千枚田', lat: 34.4789, lon: 134.2517, prefecture: '香川県', city: '小豆島町', cultivar: 'hinohikari', brand: 'tanada', area_ha: 3, production_t: 15, grade: 'terraced_100' },

  // Ehime
  { name: '泉谷の棚田', lat: 33.5158, lon: 132.8533, prefecture: '愛媛県', city: '内子町', cultivar: 'hinohikari', brand: 'tanada', area_ha: 2, production_t: 10, grade: 'terraced_100' },

  // Gunma
  { name: '群馬 ゴロピカリ', lat: 36.3911, lon: 139.0608, prefecture: '群馬県', city: '前橋地域', cultivar: 'goropikari', brand: 'gunma', area_ha: 7000, production_t: 37000, grade: 'a' },

  // Hyogo (Yumenishiki)
  { name: '但馬 コシヒカリ', lat: 35.5444, lon: 134.8231, prefecture: '兵庫県', city: '但馬地域', cultivar: 'koshihikari', brand: 'tajima', area_ha: 6500, production_t: 34000, grade: 'a' },
];

function generateSeedData() {
  return SEED_PADDIES.map((p, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [p.lon, p.lat] },
    properties: {
      paddy_id: `RICE_${String(i + 1).padStart(4, '0')}`,
      name: p.name,
      cultivar: p.cultivar,
      brand: p.brand,
      area_ha: p.area_ha,
      production_t: p.production_t,
      grade: p.grade,
      city: p.city,
      prefecture: p.prefecture,
      country: 'JP',
      source: 'maff_rice_seed',
    },
  }));
}

export default async function collectRicePaddies() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'rice-paddies',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'osm_overpass' : 'maff_rice_seed',
      description: 'Major rice-producing regions across Japan - premium brand areas and heritage tanada',
    },
    metadata: {},
  };
}
