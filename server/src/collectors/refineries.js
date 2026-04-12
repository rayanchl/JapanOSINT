/**
 * Oil Refineries Collector
 * METI registered oil refineries (~22 active sites).
 * OSM Overpass `industrial=oil_refinery` fallback.
 */

const OVERPASS_URL = 'https://overpass-api.de/api/interpreter';

const SEED_REFINERIES = [
  // ENEOS / JXTG (largest)
  { name: 'ENEOS 仙台製油所', lat: 38.2244, lon: 141.0319, company: 'ENEOS', capacity_bpd: 145000 },
  { name: 'ENEOS 根岸製油所', lat: 35.4014, lon: 139.6311, company: 'ENEOS', capacity_bpd: 270000 },
  { name: 'ENEOS 川崎製油所', lat: 35.5189, lon: 139.7372, company: 'ENEOS', capacity_bpd: 258000 },
  { name: 'ENEOS 千葉製油所', lat: 35.4944, lon: 140.0833, company: 'ENEOS', capacity_bpd: 152000 },
  { name: 'ENEOS 堺製油所', lat: 34.5622, lon: 135.4500, company: 'ENEOS', capacity_bpd: 135000 },
  { name: 'ENEOS 水島製油所', lat: 34.4961, lon: 133.7344, company: 'ENEOS', capacity_bpd: 380200 },
  { name: 'ENEOS 大分製油所', lat: 33.2700, lon: 131.7283, company: 'ENEOS', capacity_bpd: 136000 },
  { name: 'ENEOS 和歌山製油所', lat: 34.1881, lon: 135.1633, company: 'ENEOS', capacity_bpd: 127500 },
  // Idemitsu Showa Shell
  { name: '出光興産 北海道製油所 (苫小牧)', lat: 42.5828, lon: 141.5839, company: 'Idemitsu', capacity_bpd: 150000 },
  { name: '出光興産 千葉事業所', lat: 35.4781, lon: 140.0719, company: 'Idemitsu', capacity_bpd: 190000 },
  { name: '出光興産 愛知製油所', lat: 34.7964, lon: 136.9933, company: 'Idemitsu', capacity_bpd: 160000 },
  { name: '出光興産 山口事業所 (徳山)', lat: 34.0492, lon: 131.8094, company: 'Idemitsu', capacity_bpd: 120000 },
  { name: '昭和四日市石油', lat: 34.9433, lon: 136.6294, company: 'Showa Yokkaichi', capacity_bpd: 255000 },
  { name: '東亜石油 京浜製油所 (扇町)', lat: 35.5333, lon: 139.7400, company: 'Toa Oil', capacity_bpd: 70000 },
  // Cosmo Oil
  { name: 'コスモ石油 千葉製油所', lat: 35.4658, lon: 140.1011, company: 'Cosmo', capacity_bpd: 240000 },
  { name: 'コスモ石油 四日市製油所', lat: 34.9450, lon: 136.6256, company: 'Cosmo', capacity_bpd: 132000 },
  { name: 'コスモ石油 堺製油所', lat: 34.5683, lon: 135.4517, company: 'Cosmo', capacity_bpd: 100000 },
  // Fuji Oil
  { name: '富士石油 袖ケ浦製油所', lat: 35.4319, lon: 139.9928, company: 'Fuji Oil', capacity_bpd: 143000 },
  // Taiyo Oil
  { name: '太陽石油 四国事業所 (今治)', lat: 33.9886, lon: 132.8806, company: 'Taiyo Oil', capacity_bpd: 138000 },
  // West Japan
  { name: '西部石油 山口製油所', lat: 33.9614, lon: 131.0258, company: 'Seibu Oil', capacity_bpd: 120000 },
  // Kashima Oil
  { name: '鹿島石油 鹿島製油所', lat: 35.9367, lon: 140.7186, company: 'Kashima Oil', capacity_bpd: 252500 },
  // Toa
  { name: '東亜石油 水江製油所', lat: 35.5044, lon: 139.7522, company: 'Toa Oil', capacity_bpd: 65000 },
];

async function tryOverpass() {
  const query = `[out:json][timeout:25];area["ISO3166-1"="JP"]->.jp;(way["industrial"="oil_refinery"](area.jp);way["industrial"="refinery"](area.jp);way["man_made"="petroleum_refinery"](area.jp););out center 50;`;
  try {
    const ctrl = new AbortController();
    const timeout = setTimeout(() => ctrl.abort(), 12000);
    const res = await fetch(OVERPASS_URL, {
      method: 'POST',
      signal: ctrl.signal,
      headers: { 'Content-Type': 'text/plain' },
      body: query,
    });
    clearTimeout(timeout);
    if (!res.ok) return null;
    const data = await res.json();
    if (!data?.elements?.length) return null;
    return data.elements
      .map((el) => {
        const lat = el.center?.lat ?? el.lat;
        const lon = el.center?.lon ?? el.lon;
        if (lat == null || lon == null) return null;
        return {
          type: 'Feature',
          geometry: { type: 'Point', coordinates: [lon, lat] },
          properties: {
            refinery_id: `OSM_${el.id}`,
            name: el.tags?.name || 'Refinery',
            company: el.tags?.operator || 'unknown',
            source: 'osm_overpass',
          },
        };
      })
      .filter(Boolean);
  } catch {
    return null;
  }
}

function generateSeedData() {
  return SEED_REFINERIES.map((r, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [r.lon, r.lat] },
    properties: {
      refinery_id: `REF_${String(i + 1).padStart(5, '0')}`,
      name: r.name,
      company: r.company,
      capacity_bpd: r.capacity_bpd,
      country: 'JP',
      source: 'refineries_seed',
    },
  }));
}

export default async function collectRefineries() {
  let features = await tryOverpass();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'refineries',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Active Japanese oil refineries: ENEOS, Idemitsu, Cosmo, Fuji, Taiyo, Kashima Oil',
    },
    metadata: {},
  };
}
