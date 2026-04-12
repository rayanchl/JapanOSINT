/**
 * Nuclear Facilities Collector
 * Maps all nuclear facilities across Japan:
 * - Nuclear power plants (active, suspended, decommissioning)
 * - Nuclear research facilities (JAEA, RIKEN)
 * - Fuel processing / reprocessing plants
 * - Waste storage sites
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return await fetchOverpass(
    'node["power"="plant"]["plant:source"="nuclear"](area.jp);way["power"="plant"]["plant:source"="nuclear"](area.jp);node["industrial"="nuclear"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        facility_id: `NUC_LIVE_${String(i + 1).padStart(4, '0')}`,
        name: el.tags?.name || el.tags?.['name:en'] || `Nuclear facility ${el.id}`,
        operator: el.tags?.operator || 'unknown',
        facility_type: 'npp',
        status: el.tags?.['plant:status'] || 'unknown',
        country: 'JP',
        updated_at: new Date().toISOString(),
        source: 'nuclear_facilities',
      },
    })
  );
}

const NUCLEAR_FACILITIES = [
  // Active / restartable nuclear power plants
  { name: '柏崎刈羽原子力発電所', operator: '東京電力', type: 'npp', status: 'restart_pending', units: 7, capacity_mw: 8212, lat: 37.4286, lon: 138.5950, prefecture: '新潟県' },
  { name: '東通原子力発電所', operator: '東北電力', type: 'npp', status: 'restart_pending', units: 1, capacity_mw: 1100, lat: 41.1881, lon: 141.3756, prefecture: '青森県' },
  { name: '女川原子力発電所', operator: '東北電力', type: 'npp', status: 'restart_approved', units: 3, capacity_mw: 2174, lat: 38.4017, lon: 141.4994, prefecture: '宮城県' },
  { name: '福島第一原子力発電所', operator: '東京電力', type: 'npp', status: 'decommissioning', units: 6, capacity_mw: 4696, lat: 37.4239, lon: 141.0328, prefecture: '福島県' },
  { name: '福島第二原子力発電所', operator: '東京電力', type: 'npp', status: 'decommissioning', units: 4, capacity_mw: 4400, lat: 37.3158, lon: 141.0258, prefecture: '福島県' },
  { name: '東海第二原子力発電所', operator: '日本原子力発電', type: 'npp', status: 'restart_pending', units: 1, capacity_mw: 1100, lat: 36.4664, lon: 140.6064, prefecture: '茨城県' },
  { name: '浜岡原子力発電所', operator: '中部電力', type: 'npp', status: 'suspended', units: 5, capacity_mw: 4969, lat: 34.6242, lon: 138.1431, prefecture: '静岡県' },
  { name: '志賀原子力発電所', operator: '北陸電力', type: 'npp', status: 'suspended', units: 2, capacity_mw: 1898, lat: 37.0594, lon: 136.7281, prefecture: '石川県' },
  { name: '美浜原子力発電所', operator: '関西電力', type: 'npp', status: 'active', units: 1, capacity_mw: 826, lat: 35.7028, lon: 135.9606, prefecture: '福井県' },
  { name: '大飯原子力発電所', operator: '関西電力', type: 'npp', status: 'active', units: 2, capacity_mw: 2360, lat: 35.5403, lon: 135.6519, prefecture: '福井県' },
  { name: '高浜原子力発電所', operator: '関西電力', type: 'npp', status: 'active', units: 4, capacity_mw: 3392, lat: 35.5219, lon: 135.5039, prefecture: '福井県' },
  { name: '島根原子力発電所', operator: '中国電力', type: 'npp', status: 'restart_approved', units: 2, capacity_mw: 1280, lat: 35.5383, lon: 132.9994, prefecture: '島根県' },
  { name: '伊方原子力発電所', operator: '四国電力', type: 'npp', status: 'active', units: 1, capacity_mw: 890, lat: 33.4906, lon: 132.3092, prefecture: '愛媛県' },
  { name: '玄海原子力発電所', operator: '九州電力', type: 'npp', status: 'active', units: 2, capacity_mw: 2360, lat: 33.5147, lon: 129.8403, prefecture: '佐賀県' },
  { name: '川内原子力発電所', operator: '九州電力', type: 'npp', status: 'active', units: 2, capacity_mw: 1780, lat: 31.8338, lon: 130.1903, prefecture: '鹿児島県' },
  { name: '泊原子力発電所', operator: '北海道電力', type: 'npp', status: 'restart_pending', units: 3, capacity_mw: 2070, lat: 43.0353, lon: 140.5117, prefecture: '北海道' },
  { name: '敦賀原子力発電所', operator: '日本原子力発電', type: 'npp', status: 'decommissioning', units: 2, capacity_mw: 1517, lat: 35.7456, lon: 136.0089, prefecture: '福井県' },
  { name: 'もんじゅ', operator: '日本原子力研究開発機構', type: 'fbr', status: 'decommissioning', units: 1, capacity_mw: 280, lat: 35.7397, lon: 135.9892, prefecture: '福井県' },
  { name: 'ふげん', operator: '日本原子力研究開発機構', type: 'atr', status: 'decommissioning', units: 1, capacity_mw: 165, lat: 35.7497, lon: 136.0097, prefecture: '福井県' },

  // Fuel cycle facilities
  { name: '六ヶ所再処理工場', operator: '日本原燃', type: 'reprocessing', status: 'commissioning', units: 0, capacity_mw: 0, lat: 40.9667, lon: 141.3833, prefecture: '青森県' },
  { name: '六ヶ所ウラン濃縮工場', operator: '日本原燃', type: 'enrichment', status: 'active', units: 0, capacity_mw: 0, lat: 40.9550, lon: 141.3950, prefecture: '青森県' },
  { name: '六ヶ所低レベル放射性廃棄物埋設センター', operator: '日本原燃', type: 'waste_storage', status: 'active', units: 0, capacity_mw: 0, lat: 40.9633, lon: 141.4067, prefecture: '青森県' },
  { name: '六ヶ所高レベル放射性廃棄物貯蔵管理センター', operator: '日本原燃', type: 'waste_storage', status: 'active', units: 0, capacity_mw: 0, lat: 40.9700, lon: 141.4100, prefecture: '青森県' },
  { name: '東海再処理施設', operator: '日本原子力研究開発機構', type: 'reprocessing', status: 'decommissioning', units: 0, capacity_mw: 0, lat: 36.4753, lon: 140.5697, prefecture: '茨城県' },
  { name: '人形峠ウラン濃縮原型プラント', operator: '日本原子力研究開発機構', type: 'enrichment', status: 'decommissioning', units: 0, capacity_mw: 0, lat: 35.2900, lon: 133.9900, prefecture: '岡山県' },

  // Research facilities
  { name: '原子力科学研究所 (東海)', operator: '日本原子力研究開発機構', type: 'research', status: 'active', units: 0, capacity_mw: 0, lat: 36.4500, lon: 140.5800, prefecture: '茨城県' },
  { name: '大洗研究所', operator: '日本原子力研究開発機構', type: 'research', status: 'active', units: 0, capacity_mw: 0, lat: 36.3000, lon: 140.5600, prefecture: '茨城県' },
  { name: '高速実験炉「常陽」', operator: '日本原子力研究開発機構', type: 'research', status: 'restart_pending', units: 1, capacity_mw: 100, lat: 36.3000, lon: 140.5600, prefecture: '茨城県' },
  { name: 'J-PARC (大強度陽子加速器)', operator: 'J-PARC', type: 'accelerator', status: 'active', units: 0, capacity_mw: 0, lat: 36.4500, lon: 140.6000, prefecture: '茨城県' },
  { name: 'KEKつくばキャンパス', operator: '高エネルギー加速器研究機構', type: 'accelerator', status: 'active', units: 0, capacity_mw: 0, lat: 36.1500, lon: 140.0700, prefecture: '茨城県' },
  { name: 'RIKEN和光本所', operator: '理化学研究所', type: 'research', status: 'active', units: 0, capacity_mw: 0, lat: 35.7800, lon: 139.5900, prefecture: '埼玉県' },
  { name: 'SPring-8 (大型放射光施設)', operator: '理化学研究所', type: 'accelerator', status: 'active', units: 0, capacity_mw: 0, lat: 34.9500, lon: 134.4300, prefecture: '兵庫県' },
  { name: '京都大学複合原子力科学研究所', operator: '京都大学', type: 'research', status: 'active', units: 0, capacity_mw: 0, lat: 34.3900, lon: 135.3300, prefecture: '大阪府' },
  { name: '近畿大学原子力研究所', operator: '近畿大学', type: 'research', status: 'active', units: 0, capacity_mw: 0, lat: 34.6500, lon: 135.5900, prefecture: '大阪府' },
];

function generateSeedData() {
  const now = new Date();
  return NUCLEAR_FACILITIES.map((n, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [n.lon, n.lat] },
    properties: {
      facility_id: `NUC_${String(i + 1).padStart(4, '0')}`,
      name: n.name,
      operator: n.operator,
      facility_type: n.type,
      status: n.status,
      reactor_units: n.units,
      capacity_mw: n.capacity_mw,
      prefecture: n.prefecture,
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'nuclear_facilities',
    },
  }));
}

export default async function collectNuclearFacilities() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'nuclear_facilities',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Japan nuclear facilities - power plants, fuel cycle, research, waste storage',
    },
    metadata: {},
  };
}
