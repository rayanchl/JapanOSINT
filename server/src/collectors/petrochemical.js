/**
 * Petrochemical Complexes Collector
 * METI petrochemical complex registry — major コンビナート (kombinato).
 * OSM Overpass `industrial=petrochemical` fallback.
 */

const OVERPASS_URL = 'https://overpass-api.de/api/interpreter';

const SEED_PETROCHEM = [
  // Major petrochemical complexes
  { name: '京葉コンビナート 千葉', lat: 35.5400, lon: 140.0900, region: 'Keiyou', operators: 'Cosmo, Maruzen, Idemitsu', ethylene_kt: 3500 },
  { name: '京葉コンビナート 市原', lat: 35.4700, lon: 140.0883, region: 'Keiyou', operators: 'Idemitsu, Sumitomo', ethylene_kt: 1500 },
  { name: '京葉コンビナート 君津', lat: 35.3300, lon: 139.9300, region: 'Keiyou', operators: 'Mixed', ethylene_kt: 0 },
  { name: '京浜コンビナート 川崎', lat: 35.5200, lon: 139.7500, region: 'Keihin', operators: 'JX, Showa Denko, Mitsubishi', ethylene_kt: 1000 },
  { name: '京浜コンビナート 横浜', lat: 35.4700, lon: 139.6800, region: 'Keihin', operators: 'JXTG, Tonen', ethylene_kt: 500 },
  { name: '鹿島臨海工業地帯', lat: 35.9300, lon: 140.7100, region: 'Kashima', operators: 'Mitsubishi Chemical, Kashima Oil', ethylene_kt: 950 },
  { name: '中京コンビナート 四日市', lat: 34.9500, lon: 136.6300, region: 'Yokkaichi', operators: 'Cosmo, Showa Yokkaichi, Mitsubishi', ethylene_kt: 1100 },
  { name: '中京コンビナート 知多', lat: 34.9300, lon: 136.8400, region: 'Chita', operators: 'JXTG, Idemitsu', ethylene_kt: 380 },
  { name: '堺・泉北臨海工業地帯', lat: 34.5400, lon: 135.4000, region: 'Sakai-Senboku', operators: 'Cosmo, JXTG, Mitsui Chem', ethylene_kt: 500 },
  { name: '岩国・大竹コンビナート', lat: 34.2300, lon: 132.2000, region: 'Iwakuni-Otake', operators: 'Mitsui, Tosoh', ethylene_kt: 680 },
  { name: '周南・徳山コンビナート', lat: 34.0500, lon: 131.8100, region: 'Tokuyama', operators: 'Tosoh, Idemitsu, Tokuyama', ethylene_kt: 620 },
  { name: '宇部コンビナート', lat: 33.9700, lon: 131.2500, region: 'Ube', operators: 'Ube Industries', ethylene_kt: 0 },
  { name: '水島コンビナート', lat: 34.5000, lon: 133.7300, region: 'Mizushima', operators: 'JXTG, Asahi Kasei, Mitsubishi Chem', ethylene_kt: 1100 },
  { name: '北九州コンビナート', lat: 33.8800, lon: 130.8800, region: 'Kitakyushu', operators: 'Mitsubishi Chem, Mitsubishi Gas', ethylene_kt: 0 },
  { name: '大分コンビナート', lat: 33.2700, lon: 131.7300, region: 'Oita', operators: 'Showa Denko, JXTG', ethylene_kt: 700 },
  { name: '苫小牧コンビナート', lat: 42.6400, lon: 141.6200, region: 'Tomakomai', operators: 'Idemitsu, Hokkaido Refining', ethylene_kt: 0 },
  // Individual petrochemical sites
  { name: '三井化学 市原工場', lat: 35.4892, lon: 140.0758, region: 'Keiyou', operators: 'Mitsui Chemicals', ethylene_kt: 600 },
  { name: '丸善石油化学 千葉工場', lat: 35.5294, lon: 140.0889, region: 'Keiyou', operators: 'Maruzen Petrochemical', ethylene_kt: 480 },
  { name: '三菱ケミカル 鹿島事業所', lat: 35.9358, lon: 140.7261, region: 'Kashima', operators: 'Mitsubishi Chemical', ethylene_kt: 540 },
  { name: '昭和電工 大分石油化学', lat: 33.2722, lon: 131.7322, region: 'Oita', operators: 'Showa Denko', ethylene_kt: 615 },
  { name: '出光興産 徳山事業所', lat: 34.0492, lon: 131.8094, region: 'Tokuyama', operators: 'Idemitsu Kosan', ethylene_kt: 0 },
  { name: '東ソー 南陽事業所', lat: 34.0517, lon: 131.8189, region: 'Tokuyama', operators: 'Tosoh', ethylene_kt: 525 },
  { name: '住友化学 千葉工場', lat: 35.4811, lon: 140.0703, region: 'Keiyou', operators: 'Sumitomo Chemical', ethylene_kt: 415 },
  { name: '住友化学 大分工場', lat: 33.2725, lon: 131.7350, region: 'Oita', operators: 'Sumitomo Chemical', ethylene_kt: 0 },
  { name: '旭化成 水島製造所', lat: 34.4992, lon: 133.7500, region: 'Mizushima', operators: 'Asahi Kasei', ethylene_kt: 480 },
  { name: '東洋紡 敦賀事業所', lat: 35.6500, lon: 136.0667, region: 'Tsuruga', operators: 'Toyobo', ethylene_kt: 0 },
  { name: 'カネカ 高砂工業所', lat: 34.7333, lon: 134.7917, region: 'Takasago', operators: 'Kaneka', ethylene_kt: 0 },
  { name: '宇部興産 宇部ケミカル工場', lat: 33.9603, lon: 131.2575, region: 'Ube', operators: 'Ube Industries', ethylene_kt: 0 },
  { name: 'デンカ 青海工場', lat: 36.9667, lon: 137.7167, region: 'Itoigawa', operators: 'Denka', ethylene_kt: 0 },
];

async function tryOverpass() {
  const query = `[out:json][timeout:25];area["ISO3166-1"="JP"]->.jp;(way["industrial"="petrochemical"](area.jp);way["industrial"="chemical"](area.jp););out center 80;`;
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
            facility_id: `OSM_${el.id}`,
            name: el.tags?.name || 'Petrochemical',
            operator: el.tags?.operator || 'unknown',
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
  return SEED_PETROCHEM.map((c, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [c.lon, c.lat] },
    properties: {
      facility_id: `PETROCHEM_${String(i + 1).padStart(5, '0')}`,
      name: c.name,
      region: c.region,
      operators: c.operators,
      ethylene_capacity_kt_yr: c.ethylene_kt,
      country: 'JP',
      source: 'petrochem_seed',
    },
  }));
}

export default async function collectPetrochemical() {
  let features = await tryOverpass();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'petrochemical',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Major petrochemical complexes (kombinato) and chemical sites across Japan',
    },
    metadata: {},
  };
}
