/**
 * Fish Markets Collector
 * MAFF wholesale fish markets (中央卸売市場・地方卸売市場) + famous 朝市.
 * Live: OSM Overpass `amenity=marketplace` with fish/seafood filter.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpass(
    'node["amenity"="marketplace"]["name"~"魚|水産|卸売|市場"](area.jp);way["amenity"="marketplace"]["name"~"魚|水産|卸売|市場"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        market_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || `Market ${i + 1}`,
        name_ja: el.tags?.name || null,
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

// Curated: MAFF 中央卸売市場 (11 fish categories) + famous regional wholesale + tourist morning markets
const SEED_MARKETS = [
  // Tokyo
  { name: '豊洲市場 水産卸売場', lat: 35.6483, lon: 139.7828, prefecture: '東京都', city: '江東区豊洲', kind: 'central_wholesale', throughput_tpy: 480000, note: '世界最大級の魚市場、築地市場後継' },
  { name: '足立市場', lat: 35.7706, lon: 139.8139, prefecture: '東京都', city: '足立区千住橋戸町', kind: 'central_wholesale', throughput_tpy: 40000, note: '東京都中央卸売市場' },
  { name: '大田市場 水産物部', lat: 35.5644, lon: 139.7628, prefecture: '東京都', city: '大田区東海', kind: 'central_wholesale', throughput_tpy: 25000, note: '青果中心だが水産もあり' },

  // Kansai
  { name: '大阪市中央卸売市場本場 水産物部', lat: 34.6819, lon: 135.4900, prefecture: '大阪府', city: '大阪市福島区野田', kind: 'central_wholesale', throughput_tpy: 120000 },
  { name: '大阪市中央卸売市場東部', lat: 34.6703, lon: 135.5653, prefecture: '大阪府', city: '大阪市東住吉区今川', kind: 'central_wholesale', throughput_tpy: 28000 },
  { name: '京都市中央卸売市場第一', lat: 34.9994, lon: 135.7381, prefecture: '京都府', city: '京都市下京区', kind: 'central_wholesale', throughput_tpy: 85000 },
  { name: '神戸市中央卸売市場本場', lat: 34.6558, lon: 135.1858, prefecture: '兵庫県', city: '神戸市兵庫区', kind: 'central_wholesale', throughput_tpy: 90000 },
  { name: '神戸市中央卸売市場東部', lat: 34.7158, lon: 135.2639, prefecture: '兵庫県', city: '神戸市東灘区', kind: 'central_wholesale', throughput_tpy: 60000 },

  // Tohoku / Hokkaido
  { name: '札幌市中央卸売市場', lat: 43.0783, lon: 141.3244, prefecture: '北海道', city: '札幌市中央区', kind: 'central_wholesale', throughput_tpy: 140000 },
  { name: '函館朝市', lat: 41.7733, lon: 140.7267, prefecture: '北海道', city: '函館市若松町', kind: 'tourist_morning_market', throughput_tpy: 0, note: '観光朝市、約250店舗' },
  { name: '釧路和商市場', lat: 42.9867, lon: 144.3825, prefecture: '北海道', city: '釧路市黒金町', kind: 'local_wholesale', throughput_tpy: 0 },
  { name: '旭川市中央卸売市場', lat: 43.7806, lon: 142.3344, prefecture: '北海道', city: '旭川市流通団地', kind: 'local_wholesale', throughput_tpy: 35000 },
  { name: '八戸魚市場', lat: 40.5308, lon: 141.5406, prefecture: '青森県', city: '八戸市', kind: 'local_wholesale', throughput_tpy: 70000, note: 'サバ・イカ水揚げ全国屈指' },
  { name: '青森魚菜センター のっけ丼', lat: 40.8244, lon: 140.7389, prefecture: '青森県', city: '青森市古川', kind: 'tourist_morning_market', throughput_tpy: 0 },
  { name: '塩釜水産物仲卸市場', lat: 38.3111, lon: 141.0222, prefecture: '宮城県', city: '塩竈市新浜町', kind: 'local_wholesale', throughput_tpy: 60000, note: 'マグロ水揚げ全国有数' },
  { name: '仙台市中央卸売市場', lat: 38.2392, lon: 140.9308, prefecture: '宮城県', city: '仙台市若林区', kind: 'central_wholesale', throughput_tpy: 75000 },
  { name: '気仙沼魚市場', lat: 38.9086, lon: 141.5864, prefecture: '宮城県', city: '気仙沼市魚市場前', kind: 'local_wholesale', throughput_tpy: 90000, note: 'カツオ水揚げ日本一' },
  { name: '石巻魚市場', lat: 38.4172, lon: 141.3128, prefecture: '宮城県', city: '石巻市魚町', kind: 'local_wholesale', throughput_tpy: 110000 },
  { name: '秋田市民市場', lat: 39.7178, lon: 140.1119, prefecture: '秋田県', city: '秋田市中通', kind: 'local_wholesale', throughput_tpy: 0 },
  { name: '山形市公設青果花き地方卸売市場', lat: 38.2706, lon: 140.3317, prefecture: '山形県', city: '山形市あかねヶ丘', kind: 'local_wholesale', throughput_tpy: 25000 },
  { name: 'いわき市公設地方卸売市場', lat: 36.9333, lon: 140.8853, prefecture: '福島県', city: 'いわき市平下神谷', kind: 'local_wholesale', throughput_tpy: 30000 },

  // Kanto
  { name: '横浜市中央卸売市場本場', lat: 35.4611, lon: 139.6139, prefecture: '神奈川県', city: '横浜市神奈川区', kind: 'central_wholesale', throughput_tpy: 100000 },
  { name: '横浜市中央卸売市場南部', lat: 35.4181, lon: 139.6358, prefecture: '神奈川県', city: '横浜市金沢区', kind: 'central_wholesale', throughput_tpy: 50000 },
  { name: '小田原漁港 魚市場', lat: 35.2281, lon: 139.1461, prefecture: '神奈川県', city: '小田原市早川', kind: 'port_market', throughput_tpy: 6000 },
  { name: '千葉市地方卸売市場', lat: 35.5889, lon: 140.0606, prefecture: '千葉県', city: '千葉市美浜区', kind: 'local_wholesale', throughput_tpy: 45000 },
  { name: '銚子漁港 魚市場', lat: 35.7314, lon: 140.8469, prefecture: '千葉県', city: '銚子市川口町', kind: 'port_market', throughput_tpy: 280000, note: '水揚量日本一級' },
  { name: '那珂湊おさかな市場', lat: 36.3342, lon: 140.5983, prefecture: '茨城県', city: 'ひたちなか市', kind: 'tourist_morning_market', throughput_tpy: 0 },

  // Chubu / Tokai
  { name: '名古屋市中央卸売市場本場', lat: 35.1422, lon: 136.8767, prefecture: '愛知県', city: '名古屋市熱田区', kind: 'central_wholesale', throughput_tpy: 130000 },
  { name: '名古屋市中央卸売市場北部', lat: 35.2250, lon: 136.9089, prefecture: '愛知県', city: '名古屋市北区', kind: 'central_wholesale', throughput_tpy: 30000 },
  { name: '焼津漁港 魚市場', lat: 34.8683, lon: 138.3236, prefecture: '静岡県', city: '焼津市', kind: 'port_market', throughput_tpy: 180000, note: 'マグロ・カツオ水揚げ全国屈指' },
  { name: '清水魚市場', lat: 35.0197, lon: 138.4939, prefecture: '静岡県', city: '静岡市清水区', kind: 'port_market', throughput_tpy: 40000 },
  { name: '沼津魚市場', lat: 35.0883, lon: 138.8594, prefecture: '静岡県', city: '沼津市千本港町', kind: 'port_market', throughput_tpy: 25000 },
  { name: '金沢中央卸売市場 近江町市場', lat: 36.5711, lon: 136.6569, prefecture: '石川県', city: '金沢市上近江町', kind: 'tourist_morning_market', throughput_tpy: 0 },
  { name: '輪島朝市', lat: 37.3931, lon: 136.9039, prefecture: '石川県', city: '輪島市', kind: 'tourist_morning_market', throughput_tpy: 0 },
  { name: '富山県総合水産地方卸売市場', lat: 36.7528, lon: 137.2258, prefecture: '富山県', city: '富山市草島', kind: 'local_wholesale', throughput_tpy: 30000 },
  { name: '氷見漁港 魚市場', lat: 36.8553, lon: 136.9778, prefecture: '富山県', city: '氷見市中央町', kind: 'port_market', throughput_tpy: 8000, note: '寒ブリ日本一' },
  { name: '新潟市中央卸売市場', lat: 37.8981, lon: 139.0253, prefecture: '新潟県', city: '新潟市東区', kind: 'central_wholesale', throughput_tpy: 60000 },
  { name: '敦賀漁港市場', lat: 35.6553, lon: 136.0661, prefecture: '福井県', city: '敦賀市蓬莱町', kind: 'port_market', throughput_tpy: 15000 },

  // Chugoku / Shikoku
  { name: '広島市中央卸売市場', lat: 34.3639, lon: 132.4050, prefecture: '広島県', city: '広島市西区', kind: 'central_wholesale', throughput_tpy: 75000 },
  { name: '岡山市中央卸売市場', lat: 34.6342, lon: 133.8944, prefecture: '岡山県', city: '岡山市南区', kind: 'central_wholesale', throughput_tpy: 40000 },
  { name: '下関唐戸市場', lat: 33.9536, lon: 130.9436, prefecture: '山口県', city: '下関市唐戸町', kind: 'tourist_wholesale', throughput_tpy: 30000, note: 'フグ水揚げ日本一' },
  { name: '松江市公設卸売市場', lat: 35.4456, lon: 133.0608, prefecture: '島根県', city: '松江市', kind: 'local_wholesale', throughput_tpy: 15000 },
  { name: '境港魚市場', lat: 35.5419, lon: 133.2425, prefecture: '鳥取県', city: '境港市昭和町', kind: 'port_market', throughput_tpy: 90000, note: 'ベニズワイガニ水揚げ日本一' },
  { name: '高松市中央卸売市場', lat: 34.3517, lon: 134.0478, prefecture: '香川県', city: '高松市瀬戸内町', kind: 'central_wholesale', throughput_tpy: 40000 },
  { name: '松山市中央卸売市場', lat: 33.8486, lon: 132.7236, prefecture: '愛媛県', city: '松山市安城寺町', kind: 'central_wholesale', throughput_tpy: 35000 },
  { name: '高知市中央卸売市場', lat: 33.5653, lon: 133.5911, prefecture: '高知県', city: '高知市弘化台', kind: 'central_wholesale', throughput_tpy: 35000 },
  { name: '徳島中央卸売市場', lat: 34.0750, lon: 134.5483, prefecture: '徳島県', city: '徳島市', kind: 'central_wholesale', throughput_tpy: 20000 },

  // Kyushu / Okinawa
  { name: '福岡市中央卸売市場鮮魚市場', lat: 33.6050, lon: 130.3853, prefecture: '福岡県', city: '福岡市中央区長浜', kind: 'central_wholesale', throughput_tpy: 90000 },
  { name: '北九州市中央卸売市場', lat: 33.8664, lon: 130.9525, prefecture: '福岡県', city: '北九州市小倉北区', kind: 'central_wholesale', throughput_tpy: 45000 },
  { name: '長崎魚市場', lat: 32.7617, lon: 129.8431, prefecture: '長崎県', city: '長崎市京泊', kind: 'port_market', throughput_tpy: 75000 },
  { name: '熊本市中央卸売市場', lat: 32.8164, lon: 130.6844, prefecture: '熊本県', city: '熊本市西区', kind: 'central_wholesale', throughput_tpy: 40000 },
  { name: '大分市公設地方卸売市場', lat: 33.2325, lon: 131.6014, prefecture: '大分県', city: '大分市', kind: 'local_wholesale', throughput_tpy: 25000 },
  { name: '宮崎市中央卸売市場', lat: 31.9375, lon: 131.4172, prefecture: '宮崎県', city: '宮崎市', kind: 'central_wholesale', throughput_tpy: 30000 },
  { name: '鹿児島市中央卸売市場 魚類市場', lat: 31.5903, lon: 130.5844, prefecture: '鹿児島県', city: '鹿児島市城南町', kind: 'central_wholesale', throughput_tpy: 40000 },
  { name: '枕崎漁港', lat: 31.2758, lon: 130.2994, prefecture: '鹿児島県', city: '枕崎市', kind: 'port_market', throughput_tpy: 20000, note: 'カツオ節産地' },
  { name: '那覇市第一牧志公設市場', lat: 26.2133, lon: 127.6844, prefecture: '沖縄県', city: '那覇市松尾', kind: 'tourist_morning_market', throughput_tpy: 0 },
  { name: '泊いゆまち 那覇', lat: 26.2214, lon: 127.6719, prefecture: '沖縄県', city: '那覇市港町', kind: 'port_market', throughput_tpy: 15000 },
];

function generateSeedData() {
  return SEED_MARKETS.map((m, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [m.lon, m.lat] },
    properties: {
      market_id: `FISH_${String(i + 1).padStart(4, '0')}`,
      name: m.name,
      kind: m.kind,
      throughput_tpy: m.throughput_tpy,
      note: m.note || null,
      city: m.city,
      prefecture: m.prefecture,
      country: 'JP',
      source: 'maff_fish_seed',
    },
  }));
}

export default async function collectFishMarkets() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'fish-markets',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'osm_overpass' : 'maff_fish_seed',
      description: 'MAFF wholesale fish markets, port fish markets and tourist morning markets across Japan',
    },
    metadata: {},
  };
}
