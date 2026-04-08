/**
 * e-Stat Population Mesh Collector
 * Population density data for major Japanese cities as 1km grid squares
 * Fallback with representative mesh data for Tokyo, Osaka, Nagoya, Fukuoka, Sapporo
 */

const API_URL = 'https://api.e-stat.go.jp/rest/3.0/app/json/getStatsData';
const TIMEOUT_MS = 5000;

// 1km mesh grid squares for major cities
// Each entry: [baseLat, baseLon, rows, cols, cityName, baseDensity]
const CITY_GRIDS = [
  { name: '東京都心', baseLat: 35.660, baseLon: 139.720, rows: 6, cols: 6, basePop: 18000, baseDensity: 18000 },
  { name: '東京副都心', baseLat: 35.685, baseLon: 139.680, rows: 4, cols: 4, basePop: 15000, baseDensity: 15000 },
  { name: '東京東部', baseLat: 35.670, baseLon: 139.790, rows: 4, cols: 4, basePop: 13000, baseDensity: 13000 },
  { name: '大阪市中心', baseLat: 34.670, baseLon: 135.490, rows: 5, cols: 5, basePop: 14000, baseDensity: 14000 },
  { name: '名古屋市中心', baseLat: 35.165, baseLon: 136.880, rows: 4, cols: 4, basePop: 10000, baseDensity: 10000 },
  { name: '福岡市中心', baseLat: 33.585, baseLon: 130.390, rows: 4, cols: 4, basePop: 8000, baseDensity: 8000 },
  { name: '札幌市中心', baseLat: 43.050, baseLon: 141.330, rows: 4, cols: 4, basePop: 7500, baseDensity: 7500 },
];

const STEP = 0.009; // ~1km in degrees latitude at Japan's latitude

function generateMeshSquare(lat, lon, population, density, cityName, idx) {
  const halfStep = STEP / 2;
  return {
    type: 'Feature',
    geometry: {
      type: 'Polygon',
      coordinates: [[
        [lon - halfStep, lat - halfStep],
        [lon + halfStep, lat - halfStep],
        [lon + halfStep, lat + halfStep],
        [lon - halfStep, lat + halfStep],
        [lon - halfStep, lat - halfStep],
      ]],
    },
    properties: {
      mesh_id: `MESH_${String(idx).padStart(5, '0')}`,
      city_area: cityName,
      population,
      density_per_sqkm: density,
      households: Math.round(population / 2.1),
      elderly_ratio: Math.round((15 + Math.random() * 20) * 10) / 10,
      child_ratio: Math.round((5 + Math.random() * 10) * 10) / 10,
      year: 2025,
      source: 'estat_seed',
    },
  };
}

function generateSeedData() {
  const features = [];
  let idx = 0;

  for (const city of CITY_GRIDS) {
    for (let r = 0; r < city.rows; r++) {
      for (let c = 0; c < city.cols; c++) {
        const lat = city.baseLat + r * STEP;
        const lon = city.baseLon + c * STEP;

        // Population decreases from center
        const distFromCenter = Math.sqrt(
          Math.pow(r - city.rows / 2, 2) + Math.pow(c - city.cols / 2, 2)
        );
        const falloff = Math.max(0.3, 1 - distFromCenter * 0.15);
        const variation = 0.8 + Math.random() * 0.4;
        const population = Math.round(city.basePop * falloff * variation);
        const density = Math.round(population); // per sq km

        features.push(generateMeshSquare(lat, lon, population, density, city.name, ++idx));
      }
    }
  }

  return features;
}

export default async function collectEstatPopulation() {
  let features = [];
  let source = 'estat_live';

  try {
    const apiKey = process.env.ESTAT_API_KEY;
    if (!apiKey) throw new Error('No e-Stat API key configured');

    const params = new URLSearchParams({
      appId: apiKey,
      statsDataId: '0003445078', // Population mesh data
      limit: 1000,
    });

    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(`${API_URL}?${params}`, { signal: controller.signal });
    clearTimeout(timer);

    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.json();

    const values = data?.GET_STATS_DATA?.STATISTICAL_DATA?.DATA_INF?.VALUE;
    if (Array.isArray(values) && values.length > 0) {
      // Parse mesh code to lat/lon and build polygons
      for (const v of values) {
        const meshCode = v['@area'];
        if (!meshCode || meshCode.length < 8) continue;
        // Standard mesh code decoding
        const lat1 = parseInt(meshCode.substring(0, 2)) / 1.5;
        const lon1 = parseInt(meshCode.substring(2, 4)) + 100;
        const lat2 = parseInt(meshCode.substring(4, 5)) / 1.5 / 8;
        const lon2 = parseInt(meshCode.substring(5, 6)) / 8;
        const lat3 = parseInt(meshCode.substring(6, 7)) / 1.5 / 80;
        const lon3 = parseInt(meshCode.substring(7, 8)) / 80;
        const lat = lat1 + lat2 + lat3;
        const lon = lon1 + lon2 + lon3;
        const pop = parseInt(v.$) || 0;

        features.push(generateMeshSquare(lat, lon, pop, pop, 'live', features.length));
      }
    }
    if (features.length === 0) throw new Error('No features parsed');
  } catch {
    features = generateSeedData();
    source = 'estat_seed';
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Population mesh data from e-Stat for major Japanese cities',
    },
    metadata: {},
  };
}
