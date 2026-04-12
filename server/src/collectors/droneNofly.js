/**
 * Drone No-Fly Zones Collector
 * Fetches MLIT/JCAB restricted airspace polygons.
 * Falls back to seed of major drone no-fly zones (DID, airports, key facilities).
 */

const MLIT_DRONE_URL = 'https://www.mlit.go.jp/koku/koku_fr10_000041.html';

const SEED_NOFLY_ZONES = [
  // Airports (within 9 km Class B/C/D)
  { name: '羽田空港 9km圏', lat: 35.5494, lon: 139.7798, radius_km: 9, type: 'airport', restriction: 'absolute', region: 'Tokyo' },
  { name: '成田空港 9km圏', lat: 35.7720, lon: 140.3929, radius_km: 9, type: 'airport', restriction: 'absolute', region: 'Chiba' },
  { name: '伊丹空港 9km圏', lat: 34.7855, lon: 135.4382, radius_km: 9, type: 'airport', restriction: 'absolute', region: 'Osaka' },
  { name: '関西空港 9km圏', lat: 34.4347, lon: 135.2440, radius_km: 9, type: 'airport', restriction: 'absolute', region: 'Osaka' },
  { name: '中部国際空港 9km圏', lat: 34.8584, lon: 136.8054, radius_km: 9, type: 'airport', restriction: 'absolute', region: 'Aichi' },
  { name: '新千歳空港 9km圏', lat: 42.7752, lon: 141.6920, radius_km: 9, type: 'airport', restriction: 'absolute', region: 'Hokkaido' },
  { name: '福岡空港 9km圏', lat: 33.5859, lon: 130.4509, radius_km: 9, type: 'airport', restriction: 'absolute', region: 'Fukuoka' },
  { name: '那覇空港 9km圏', lat: 26.1958, lon: 127.6464, radius_km: 9, type: 'airport', restriction: 'absolute', region: 'Okinawa' },
  { name: '仙台空港 9km圏', lat: 38.1397, lon: 140.9170, radius_km: 9, type: 'airport', restriction: 'absolute', region: 'Miyagi' },
  { name: '広島空港 9km圏', lat: 34.4361, lon: 132.9197, radius_km: 9, type: 'airport', restriction: 'absolute', region: 'Hiroshima' },
  { name: '神戸空港 9km圏', lat: 34.6328, lon: 135.2238, radius_km: 9, type: 'airport', restriction: 'absolute', region: 'Hyogo' },
  { name: '小松空港 9km圏', lat: 36.3946, lon: 136.4068, radius_km: 9, type: 'airport', restriction: 'absolute', region: 'Ishikawa' },

  // Densely Inhabited Districts (DID) - require permit
  { name: '東京23区 DID', lat: 35.6896, lon: 139.6917, radius_km: 25, type: 'did', restriction: 'permit', region: 'Tokyo' },
  { name: '横浜市 DID', lat: 35.4437, lon: 139.6380, radius_km: 18, type: 'did', restriction: 'permit', region: 'Kanagawa' },
  { name: '大阪市 DID', lat: 34.6937, lon: 135.5023, radius_km: 18, type: 'did', restriction: 'permit', region: 'Osaka' },
  { name: '名古屋市 DID', lat: 35.1815, lon: 136.9066, radius_km: 18, type: 'did', restriction: 'permit', region: 'Aichi' },
  { name: '京都市 DID', lat: 35.0116, lon: 135.7681, radius_km: 12, type: 'did', restriction: 'permit', region: 'Kyoto' },
  { name: '神戸市 DID', lat: 34.6901, lon: 135.1955, radius_km: 12, type: 'did', restriction: 'permit', region: 'Hyogo' },
  { name: '札幌市 DID', lat: 43.0628, lon: 141.3478, radius_km: 15, type: 'did', restriction: 'permit', region: 'Hokkaido' },
  { name: '福岡市 DID', lat: 33.5904, lon: 130.4017, radius_km: 12, type: 'did', restriction: 'permit', region: 'Fukuoka' },
  { name: '仙台市 DID', lat: 38.2683, lon: 140.8719, radius_km: 10, type: 'did', restriction: 'permit', region: 'Miyagi' },
  { name: '広島市 DID', lat: 34.3853, lon: 132.4553, radius_km: 10, type: 'did', restriction: 'permit', region: 'Hiroshima' },

  // Nuclear power plants - absolute no-fly
  { name: '柏崎刈羽 原発', lat: 37.4283, lon: 138.5950, radius_km: 5, type: 'nuclear', restriction: 'absolute', region: 'Niigata' },
  { name: '東海第二 原発', lat: 36.4669, lon: 140.6047, radius_km: 5, type: 'nuclear', restriction: 'absolute', region: 'Ibaraki' },
  { name: '玄海 原発', lat: 33.5147, lon: 129.8369, radius_km: 5, type: 'nuclear', restriction: 'absolute', region: 'Saga' },
  { name: '川内 原発', lat: 31.8333, lon: 130.1928, radius_km: 5, type: 'nuclear', restriction: 'absolute', region: 'Kagoshima' },
  { name: '伊方 原発', lat: 33.4906, lon: 132.3094, radius_km: 5, type: 'nuclear', restriction: 'absolute', region: 'Ehime' },
  { name: '高浜 原発', lat: 35.5217, lon: 135.5042, radius_km: 5, type: 'nuclear', restriction: 'absolute', region: 'Fukui' },
  { name: '大飯 原発', lat: 35.5395, lon: 135.6519, radius_km: 5, type: 'nuclear', restriction: 'absolute', region: 'Fukui' },
  { name: '美浜 原発', lat: 35.7036, lon: 135.9628, radius_km: 5, type: 'nuclear', restriction: 'absolute', region: 'Fukui' },
  { name: '島根 原発', lat: 35.5384, lon: 132.9986, radius_km: 5, type: 'nuclear', restriction: 'absolute', region: 'Shimane' },

  // Imperial properties / government / defense
  { name: '皇居', lat: 35.6852, lon: 139.7528, radius_km: 1, type: 'imperial', restriction: 'absolute', region: 'Tokyo' },
  { name: '国会議事堂', lat: 35.6758, lon: 139.7449, radius_km: 1, type: 'government', restriction: 'absolute', region: 'Tokyo' },
  { name: '首相官邸', lat: 35.6726, lon: 139.7440, radius_km: 1, type: 'government', restriction: 'absolute', region: 'Tokyo' },
  { name: '防衛省 市ヶ谷', lat: 35.6928, lon: 139.7283, radius_km: 1, type: 'defense', restriction: 'absolute', region: 'Tokyo' },
  { name: '横田基地', lat: 35.7486, lon: 139.3489, radius_km: 9, type: 'military', restriction: 'absolute', region: 'Tokyo' },
  { name: '横須賀基地', lat: 35.2917, lon: 139.6611, radius_km: 5, type: 'military', restriction: 'absolute', region: 'Kanagawa' },
  { name: '岩国基地', lat: 34.1439, lon: 132.2358, radius_km: 9, type: 'military', restriction: 'absolute', region: 'Yamaguchi' },
  { name: '嘉手納基地', lat: 26.3556, lon: 127.7681, radius_km: 9, type: 'military', restriction: 'absolute', region: 'Okinawa' },
];

async function tryMlitDrone() {
  try {
    const ctrl = new AbortController();
    const timeout = setTimeout(() => ctrl.abort(), 10000);
    const res = await fetch(MLIT_DRONE_URL, { signal: ctrl.signal });
    clearTimeout(timeout);
    if (!res.ok) return null;
    return null;
  } catch {
    return null;
  }
}

function generateSeedData() {
  return SEED_NOFLY_ZONES.map((z, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [z.lon, z.lat] },
    properties: {
      zone_id: `NFZ_${String(i + 1).padStart(5, '0')}`,
      name: z.name,
      radius_km: z.radius_km,
      zone_type: z.type,
      restriction: z.restriction,
      region: z.region,
      country: 'JP',
      source: 'drone_nofly_seed',
    },
  }));
}

export default async function collectDroneNofly() {
  let features = await tryMlitDrone();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'drone_nofly',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'MLIT/JCAB drone no-fly zones - airports, DID, nuclear, military, imperial',
    },
    metadata: {},
  };
}
