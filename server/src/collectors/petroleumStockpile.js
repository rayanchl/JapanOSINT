/**
 * Petroleum Stockpile Collector
 * METI strategic petroleum reserve sites — 10 designated national reserves +
 * commercial stockpile bases.
 */

const SEED_RESERVES = [
  // National reserves (10 sites operated by JOGMEC)
  { name: '苫小牧東部国家石油備蓄基地', lat: 42.6044, lon: 141.6919, kind: 'national', capacity_kl: 6400000 },
  { name: 'むつ小川原国家石油備蓄基地', lat: 40.9744, lon: 141.3814, kind: 'national', capacity_kl: 5700000 },
  { name: '久慈国家石油備蓄基地', lat: 40.2292, lon: 141.7956, kind: 'national', capacity_kl: 1750000 },
  { name: '秋田国家石油備蓄基地', lat: 39.7706, lon: 140.0561, kind: 'national', capacity_kl: 4500000 },
  { name: '志布志国家石油備蓄基地', lat: 31.4878, lon: 131.0997, kind: 'national', capacity_kl: 5000000 },
  { name: '上五島国家石油備蓄基地', lat: 32.9981, lon: 129.0850, kind: 'national', capacity_kl: 4400000 },
  { name: '白島国家石油備蓄基地', lat: 33.9389, lon: 130.7194, kind: 'national', capacity_kl: 5600000 },
  { name: '福井国家石油備蓄基地', lat: 35.7800, lon: 136.0717, kind: 'national', capacity_kl: 3400000 },
  { name: '菊間国家石油備蓄基地', lat: 34.0461, lon: 132.8983, kind: 'national', capacity_kl: 1500000 },
  { name: '串木野国家石油備蓄基地', lat: 31.6864, lon: 130.2697, kind: 'national', capacity_kl: 1750000 },
  // National LPG reserves (5 sites)
  { name: '七尾国家石油ガス備蓄基地', lat: 37.0567, lon: 136.9569, kind: 'national_lpg', capacity_t: 250000 },
  { name: '福島国家石油ガス備蓄基地 (相馬)', lat: 37.7967, lon: 140.9694, kind: 'national_lpg', capacity_t: 200000 },
  { name: '波方国家石油ガス備蓄基地', lat: 34.1356, lon: 132.9572, kind: 'national_lpg', capacity_t: 450000 },
  { name: '倉敷国家石油ガス備蓄基地', lat: 34.5036, lon: 133.7669, kind: 'national_lpg', capacity_t: 400000 },
  { name: '神栖国家石油ガス備蓄基地', lat: 35.9133, lon: 140.6906, kind: 'national_lpg', capacity_t: 200000 },
  // Major commercial stockpile bases (oil distribution depots)
  { name: 'ENEOS 川崎油槽所', lat: 35.5189, lon: 139.7372, kind: 'commercial', capacity_kl: 800000 },
  { name: '出光 千葉油槽所', lat: 35.4781, lon: 140.0719, kind: 'commercial', capacity_kl: 600000 },
  { name: 'コスモ 千葉油槽所', lat: 35.4658, lon: 140.1011, kind: 'commercial', capacity_kl: 750000 },
  { name: 'ENEOS 仙台油槽所', lat: 38.2244, lon: 141.0319, kind: 'commercial', capacity_kl: 400000 },
  { name: 'ENEOS 根岸油槽所', lat: 35.4014, lon: 139.6311, kind: 'commercial', capacity_kl: 1200000 },
  { name: 'ENEOS 水島油槽所', lat: 34.4961, lon: 133.7344, kind: 'commercial', capacity_kl: 900000 },
  { name: 'ENEOS 大分油槽所', lat: 33.2700, lon: 131.7283, kind: 'commercial', capacity_kl: 500000 },
  { name: '出光 北海道油槽所 (苫小牧)', lat: 42.5828, lon: 141.5839, kind: 'commercial', capacity_kl: 700000 },
];

function generateSeedData() {
  return SEED_RESERVES.map((r, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [r.lon, r.lat] },
    properties: {
      reserve_id: `STOCK_${String(i + 1).padStart(5, '0')}`,
      name: r.name,
      kind: r.kind,
      capacity_kl: r.capacity_kl || null,
      capacity_t: r.capacity_t || null,
      country: 'JP',
      source: 'petroleum_stockpile_seed',
    },
  }));
}

export default async function collectPetroleumStockpile() {
  const features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'petroleum_stockpile',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: false,
      description: 'JOGMEC strategic petroleum reserves (10 oil + 5 LPG) and major commercial stockpile depots',
    },
    metadata: {},
  };
}
