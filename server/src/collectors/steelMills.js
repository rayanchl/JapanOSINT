/**
 * Steel Mills Collector
 * Major Japanese steelworks: Nippon Steel, JFE, Kobelco, Daido, Nisshin, etc.
 * OSM Overpass `industrial=steel` with curated seed of integrated mills.
 */

import { fetchOverpass } from './_liveHelpers.js';

const SEED_STEEL = [
  // Nippon Steel
  { name: '日本製鉄 君津製鉄所', lat: 35.3367, lon: 139.9381, company: 'Nippon Steel', capacity_mt: 11.0, kind: 'integrated' },
  { name: '日本製鉄 鹿島製鉄所', lat: 35.9300, lon: 140.7150, company: 'Nippon Steel', capacity_mt: 9.0, kind: 'integrated' },
  { name: '日本製鉄 名古屋製鉄所', lat: 35.0353, lon: 136.7886, company: 'Nippon Steel', capacity_mt: 7.5, kind: 'integrated' },
  { name: '日本製鉄 大分製鉄所', lat: 33.2700, lon: 131.7283, company: 'Nippon Steel', capacity_mt: 9.5, kind: 'integrated' },
  { name: '日本製鉄 八幡製鉄所', lat: 33.8889, lon: 130.8158, company: 'Nippon Steel', capacity_mt: 5.5, kind: 'integrated' },
  { name: '日本製鉄 室蘭製鉄所', lat: 42.3467, lon: 140.9683, company: 'Nippon Steel', capacity_mt: 1.7, kind: 'integrated' },
  { name: '日本製鉄 広畑製鉄所', lat: 34.7858, lon: 134.6383, company: 'Nippon Steel', capacity_mt: 0, kind: 'rolling' },
  { name: '日本製鉄 直江津製造所', lat: 37.1769, lon: 138.2522, company: 'Nippon Steel', capacity_mt: 0, kind: 'specialty' },
  { name: '日本製鉄 関西製鉄所 和歌山地区', lat: 34.2247, lon: 135.1175, company: 'Nippon Steel', capacity_mt: 5.0, kind: 'integrated' },
  // JFE Steel
  { name: 'JFEスチール 千葉地区', lat: 35.5944, lon: 140.0931, company: 'JFE', capacity_mt: 5.4, kind: 'integrated' },
  { name: 'JFEスチール 京浜地区', lat: 35.5369, lon: 139.7383, company: 'JFE', capacity_mt: 4.0, kind: 'integrated' },
  { name: 'JFEスチール 倉敷地区 (水島)', lat: 34.4844, lon: 133.7350, company: 'JFE', capacity_mt: 9.5, kind: 'integrated' },
  { name: 'JFEスチール 福山地区', lat: 34.4344, lon: 133.4072, company: 'JFE', capacity_mt: 11.7, kind: 'integrated' },
  { name: 'JFEスチール 知多製造所', lat: 34.9319, lon: 136.8650, company: 'JFE', capacity_mt: 0, kind: 'pipe' },
  { name: 'JFEスチール 仙台製造所', lat: 38.2611, lon: 141.0317, company: 'JFE', capacity_mt: 0, kind: 'rolling' },
  // Kobe Steel (Kobelco)
  { name: '神戸製鋼 神戸製鉄所 灘浜地区', lat: 34.6989, lon: 135.2289, company: 'Kobelco', capacity_mt: 0, kind: 'specialty' },
  { name: '神戸製鋼 加古川製鉄所', lat: 34.7456, lon: 134.8217, company: 'Kobelco', capacity_mt: 7.5, kind: 'integrated' },
  { name: '神戸製鋼 高砂製作所', lat: 34.7308, lon: 134.7889, company: 'Kobelco', capacity_mt: 0, kind: 'forge' },
  { name: '神戸製鋼 真岡製造所', lat: 36.4628, lon: 140.0150, company: 'Kobelco', capacity_mt: 0, kind: 'rolling' },
  // Daido Steel
  { name: '大同特殊鋼 知多工場', lat: 34.9244, lon: 136.8581, company: 'Daido', capacity_mt: 1.2, kind: 'specialty' },
  { name: '大同特殊鋼 渋川工場', lat: 36.4878, lon: 138.9683, company: 'Daido', capacity_mt: 0, kind: 'specialty' },
  { name: '大同特殊鋼 星崎工場', lat: 35.0744, lon: 136.9275, company: 'Daido', capacity_mt: 0, kind: 'specialty' },
  // Nisshin Steel (now part of Nippon Steel)
  { name: '日新製鋼 阪神製造所', lat: 34.7167, lon: 135.3500, company: 'Nisshin', capacity_mt: 0, kind: 'rolling' },
  { name: '日新製鋼 呉製鉄所', lat: 34.2389, lon: 132.5586, company: 'Nisshin', capacity_mt: 1.6, kind: 'integrated' },
  // Tokyo Steel (electric arc)
  { name: '東京製鐵 田原工場', lat: 34.6481, lon: 137.2811, company: 'Tokyo Steel', capacity_mt: 2.5, kind: 'eaf' },
  { name: '東京製鐵 岡山工場', lat: 34.5158, lon: 133.7569, company: 'Tokyo Steel', capacity_mt: 1.8, kind: 'eaf' },
  { name: '東京製鐵 宇都宮工場', lat: 36.5450, lon: 139.8775, company: 'Tokyo Steel', capacity_mt: 1.2, kind: 'eaf' },
  { name: '東京製鐵 高松工場', lat: 34.3322, lon: 134.0697, company: 'Tokyo Steel', capacity_mt: 0.8, kind: 'eaf' },
  { name: '東京製鐵 九州工場', lat: 33.7233, lon: 130.7706, company: 'Tokyo Steel', capacity_mt: 1.5, kind: 'eaf' },
];

async function tryOverpass() {
  return fetchOverpass(
    'way["industrial"="steel"](area.jp);way["landuse"="industrial"]["product"="steel"](area.jp);',
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        mill_id: `OSM_${el.id}`,
        name: el.tags?.name || 'Steel Mill',
        company: el.tags?.operator || 'unknown',
        source: 'osm_overpass',
      },
    }),
  );
}

function generateSeedData() {
  return SEED_STEEL.map((m, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [m.lon, m.lat] },
    properties: {
      mill_id: `STEEL_${String(i + 1).padStart(5, '0')}`,
      name: m.name,
      company: m.company,
      capacity_mt_yr: m.capacity_mt,
      kind: m.kind,
      country: 'JP',
      source: 'steel_seed',
    },
  }));
}

export default async function collectSteelMills() {
  let features = await tryOverpass();
  const live = !!(features && features.length > 0);
  if (!live) features = [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'steel_mills',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Japanese steelworks: Nippon Steel, JFE, Kobelco, Daido, Tokyo Steel (integrated, EAF, specialty)',
    },
  };
}
