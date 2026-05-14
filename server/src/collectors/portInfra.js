/**
 * Port Infrastructure Collector
 * Maps port and maritime infrastructure across Japan:
 * - Special Important Ports (特定重要港湾)
 * - Important Ports (重要港湾)
 * - Local Ports (地方港湾)
 * - Fishing Ports (漁港)
 * Uses OSM and MLIT port data
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryOSMPorts() {
  return fetchOverpass(
    'node["harbour"](area.jp);way["harbour"](area.jp);node["industrial"="port"](area.jp);way["landuse"="port"](area.jp);node["seamark:type"="harbour"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        port_id: `OSM_${el.id}`,
        name: el.tags?.name || el.tags?.['name:en'] || `Port facility ${i + 1}`,
        name_ja: el.tags?.['name:ja'] || el.tags?.name || null,
        port_class: el.tags?.harbour || 'unknown',
        operator: el.tags?.operator || 'unknown',
        prefecture: el.tags?.['addr:province'] || el.tags?.['addr:state'] || null,
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

const PORT_FACILITIES = [
  // International Strategic Ports (国際戦略港湾 + 国際拠点港湾)
  { name: 'Tokyo Port', name_ja: '東京港', lat: 35.6200, lon: 139.7800, port_class: 'international_strategic', container_teu: 4_500_000, cargo_mt: 86_000_000, prefecture: 'Tokyo' },
  { name: 'Yokohama Port', name_ja: '横浜港', lat: 35.4500, lon: 139.6500, port_class: 'international_strategic', container_teu: 2_900_000, cargo_mt: 118_000_000, prefecture: 'Kanagawa' },
  { name: 'Nagoya Port', name_ja: '名古屋港', lat: 35.0800, lon: 136.8800, port_class: 'international_strategic', container_teu: 2_700_000, cargo_mt: 197_000_000, prefecture: 'Aichi' },
  { name: 'Osaka Port', name_ja: '大阪港', lat: 34.6500, lon: 135.4300, port_class: 'international_strategic', container_teu: 2_400_000, cargo_mt: 82_000_000, prefecture: 'Osaka' },
  { name: 'Kobe Port', name_ja: '神戸港', lat: 34.6700, lon: 135.1900, port_class: 'international_strategic', container_teu: 2_900_000, cargo_mt: 99_000_000, prefecture: 'Hyogo' },

  // Important Ports (重要港湾)
  { name: 'Kitakyushu', name_ja: '北九州港', lat: 33.8900, lon: 130.8800, port_class: 'important', cargo_mt: 95_000_000, prefecture: 'Fukuoka' },
  { name: 'Hakata', name_ja: '博多港', lat: 33.6100, lon: 130.4000, port_class: 'important', cargo_mt: 38_000_000, prefecture: 'Fukuoka' },
  { name: 'Shimonoseki', name_ja: '下関港', lat: 33.9500, lon: 130.9400, port_class: 'important', cargo_mt: 22_000_000, prefecture: 'Yamaguchi' },
  { name: 'Hiroshima', name_ja: '広島港', lat: 34.3600, lon: 132.4600, port_class: 'important', cargo_mt: 15_000_000, prefecture: 'Hiroshima' },
  { name: 'Sakai-Senboku', name_ja: '堺泉北港', lat: 34.5500, lon: 135.4500, port_class: 'important', cargo_mt: 85_000_000, prefecture: 'Osaka' },
  { name: 'Chiba', name_ja: '千葉港', lat: 35.5800, lon: 140.1000, port_class: 'important', cargo_mt: 156_000_000, prefecture: 'Chiba' },
  { name: 'Kawasaki', name_ja: '川崎港', lat: 35.5000, lon: 139.7600, port_class: 'important', cargo_mt: 75_000_000, prefecture: 'Kanagawa' },
  { name: 'Shimizu', name_ja: '清水港', lat: 35.0200, lon: 138.5100, port_class: 'important', cargo_mt: 18_000_000, prefecture: 'Shizuoka' },
  { name: 'Yokkaichi', name_ja: '四日市港', lat: 34.9600, lon: 136.6500, port_class: 'important', cargo_mt: 52_000_000, prefecture: 'Mie' },
  { name: 'Niigata', name_ja: '新潟港', lat: 37.9200, lon: 139.0500, port_class: 'important', cargo_mt: 25_000_000, prefecture: 'Niigata' },
  { name: 'Akita', name_ja: '秋田港', lat: 39.7600, lon: 140.0600, port_class: 'important', cargo_mt: 8_000_000, prefecture: 'Akita' },
  { name: 'Sendai-Shiogama', name_ja: '仙台塩釜港', lat: 38.2500, lon: 141.0200, port_class: 'important', cargo_mt: 22_000_000, prefecture: 'Miyagi' },
  { name: 'Tomakomai', name_ja: '苫小牧港', lat: 42.6200, lon: 141.6300, port_class: 'important', cargo_mt: 102_000_000, prefecture: 'Hokkaido' },
  { name: 'Muroran', name_ja: '室蘭港', lat: 42.3200, lon: 140.9700, port_class: 'important', cargo_mt: 14_000_000, prefecture: 'Hokkaido' },
  { name: 'Naha', name_ja: '那覇港', lat: 26.2200, lon: 127.6700, port_class: 'important', cargo_mt: 9_000_000, prefecture: 'Okinawa' },
  { name: 'Kagoshima', name_ja: '鹿児島港', lat: 31.5900, lon: 130.5600, port_class: 'important', cargo_mt: 6_000_000, prefecture: 'Kagoshima' },
  { name: 'Tokuyama-Kudamatsu', name_ja: '徳山下松港', lat: 34.0500, lon: 131.8200, port_class: 'important', cargo_mt: 32_000_000, prefecture: 'Yamaguchi' },
  { name: 'Mizushima', name_ja: '水島港', lat: 34.5000, lon: 133.7200, port_class: 'important', cargo_mt: 72_000_000, prefecture: 'Okayama' },
  { name: 'Oita', name_ja: '大分港', lat: 33.2400, lon: 131.6400, port_class: 'important', cargo_mt: 28_000_000, prefecture: 'Oita' },
  { name: 'Wakayama-Shimotsu', name_ja: '和歌山下津港', lat: 34.1800, lon: 135.1400, port_class: 'important', cargo_mt: 28_000_000, prefecture: 'Wakayama' },

  // Fishing Ports (漁港)
  { name: 'Choshi', name_ja: '銚子漁港', lat: 35.7400, lon: 140.8600, port_class: 'fishing', catch_mt: 280_000, prefecture: 'Chiba' },
  { name: 'Yaizu', name_ja: '焼津漁港', lat: 34.8700, lon: 138.3200, port_class: 'fishing', catch_mt: 190_000, prefecture: 'Shizuoka' },
  { name: 'Sakai (Tottori)', name_ja: '境港', lat: 35.5400, lon: 133.2300, port_class: 'fishing', catch_mt: 120_000, prefecture: 'Tottori' },
  { name: 'Kushiro', name_ja: '釧路漁港', lat: 42.9700, lon: 144.3800, port_class: 'fishing', catch_mt: 100_000, prefecture: 'Hokkaido' },
  { name: 'Ishinomaki', name_ja: '石巻漁港', lat: 38.4200, lon: 141.3100, port_class: 'fishing', catch_mt: 80_000, prefecture: 'Miyagi' },
  { name: 'Hachinohe', name_ja: '八戸漁港', lat: 40.5200, lon: 141.5400, port_class: 'fishing', catch_mt: 75_000, prefecture: 'Aomori' },
  { name: 'Nagasaki', name_ja: '長崎漁港', lat: 32.7500, lon: 129.8700, port_class: 'fishing', catch_mt: 60_000, prefecture: 'Nagasaki' },
  { name: 'Kesennuma', name_ja: '気仙沼漁港', lat: 38.9000, lon: 141.5700, port_class: 'fishing', catch_mt: 75_000, prefecture: 'Miyagi' },
  { name: 'Makurazaki', name_ja: '枕崎漁港', lat: 31.2700, lon: 130.2900, port_class: 'fishing', catch_mt: 50_000, prefecture: 'Kagoshima' },
  { name: 'Misaki', name_ja: '三崎漁港', lat: 35.1400, lon: 139.6200, port_class: 'fishing', catch_mt: 40_000, prefecture: 'Kanagawa' },

  // Ferry Terminals
  { name: 'Tokyo-Ogasawara', name_ja: '東京-小笠原フェリーターミナル', lat: 35.6280, lon: 139.7640, port_class: 'ferry_terminal', ferry_routes: 'Tokyo - Ogasawara (Chichijima)', prefecture: 'Tokyo' },
  { name: 'Osaka Nanko', name_ja: '大阪南港フェリーターミナル', lat: 34.6300, lon: 135.4100, port_class: 'ferry_terminal', ferry_routes: 'Osaka - Beppu, Osaka - Shikoku', prefecture: 'Osaka' },
  { name: 'Kobe Ferry Terminal', name_ja: '神戸フェリーターミナル', lat: 34.6700, lon: 135.2000, port_class: 'ferry_terminal', ferry_routes: 'Kobe - Shikoku (Takamatsu, Tokushima)', prefecture: 'Hyogo' },
  { name: 'Oma-Hakodate', name_ja: '大間-函館フェリーターミナル', lat: 41.5200, lon: 140.9100, port_class: 'ferry_terminal', ferry_routes: 'Oma - Hakodate', prefecture: 'Aomori' },
  { name: 'Aomori-Hakodate', name_ja: '青森-函館フェリーターミナル', lat: 40.8200, lon: 140.7300, port_class: 'ferry_terminal', ferry_routes: 'Aomori - Hakodate (Tsugaru Kaikyo Ferry)', prefecture: 'Aomori' },
  { name: 'Kagoshima-Yakushima', name_ja: '鹿児島-屋久島フェリーターミナル', lat: 31.5900, lon: 130.5600, port_class: 'ferry_terminal', ferry_routes: 'Kagoshima - Yakushima, Tanegashima', prefecture: 'Kagoshima' },
  { name: 'Naha-Miyako-Ishigaki', name_ja: '那覇-宮古-石垣フェリーターミナル', lat: 26.2200, lon: 127.6700, port_class: 'ferry_terminal', ferry_routes: 'Naha - Miyako - Ishigaki', prefecture: 'Okinawa' },
  { name: 'Niigata-Sado', name_ja: '新潟-佐渡フェリーターミナル', lat: 37.9400, lon: 139.0700, port_class: 'ferry_terminal', ferry_routes: 'Niigata - Ryotsu (Sado Island)', prefecture: 'Niigata' },
  { name: 'Toba-Irago', name_ja: '鳥羽-伊良湖フェリーターミナル', lat: 34.4800, lon: 136.8400, port_class: 'ferry_terminal', ferry_routes: 'Toba - Irago (Ise Bay Ferry)', prefecture: 'Mie' },
  { name: 'Hakata-Busan', name_ja: '博多-釜山フェリーターミナル', lat: 33.6100, lon: 130.4000, port_class: 'ferry_terminal', ferry_routes: 'Hakata - Busan (international)', prefecture: 'Fukuoka' },
];

function generateSeedData() {
  const now = new Date();
  return PORT_FACILITIES.map((p, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [p.lon, p.lat] },
    properties: {
      port_id: `PORT_${String(i + 1).padStart(4, '0')}`,
      name: p.name,
      name_ja: p.name_ja,
      port_class: p.port_class,
      operator: p.operator || null,
      cargo_tonnage_mt: p.cargo_mt || null,
      container_teu: p.container_teu || null,
      catch_mt: p.catch_mt || null,
      ferry_routes: p.ferry_routes || null,
      prefecture: p.prefecture || null,
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'port_infra',
    },
  }));
}

export default async function collectPortInfra() {
  const results = await Promise.allSettled([
    tryOSMPorts(),
  ]);

  let osmFeatures = results[0].status === 'fulfilled' ? results[0].value : null;

  const live = !!(osmFeatures && osmFeatures.length > 0);
  const seedFeatures = [];

  const features = live ? [...osmFeatures, ...seedFeatures] : seedFeatures;

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'port_infra',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Japan port infrastructure - international strategic ports, important ports, fishing ports, ferry terminals',
    },
  };
}
