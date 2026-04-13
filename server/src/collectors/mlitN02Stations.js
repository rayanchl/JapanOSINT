/**
 * MLIT N02 — National Land Numerical Information: Railway Stations
 * 国土数値情報 鉄道データ N02
 *
 * MLIT publishes the authoritative nationwide rail dataset (every line and
 * station operated in Japan, updated yearly) at:
 *   https://nlftp.mlit.go.jp/ksj/gml/datalist/KsjTmplt-N02-v3_0.html
 *
 * The canonical distribution is a zipped GML/Shapefile which requires a
 * shapefile parser at runtime. To avoid pulling a heavy dependency we accept
 * a GeoJSON URL via env (MLIT_N02_GEOJSON_URL) — typical deployments host a
 * pre-converted copy on S3 / a CDN / the app server itself. We also try a
 * few well-known community mirrors before giving up.
 *
 * When no remote source is reachable we still emit a small seed of flagship
 * stations so the layer doesn't come back empty in dev.
 */

import { fetchJson, fetchOverpassTiled } from './_liveHelpers.js';

const ENV_URL = process.env.MLIT_N02_GEOJSON_URL || null;

// Best-effort community mirrors of the converted N02 dataset. These are
// tried in order; any 404 / network failure simply falls through.
const MIRROR_URLS = [
  // Users may override / extend via env; keep this list short and stable.
  'https://nlftp.mlit.go.jp/ksj/gml/data/N02/N02-22/N02-22_Station.geojson',
];

const SEED_STATIONS = [
  { name: 'Tokyo',    name_ja: '東京',    line: 'JR Tokaido Shinkansen', operator: 'JR East',    lat: 35.6812, lon: 139.7671 },
  { name: 'Shinjuku', name_ja: '新宿',    line: 'JR Yamanote',           operator: 'JR East',    lat: 35.6896, lon: 139.7006 },
  { name: 'Osaka',    name_ja: '大阪',    line: 'JR Loop Line',          operator: 'JR West',    lat: 34.7024, lon: 135.4959 },
  { name: 'Kyoto',    name_ja: '京都',    line: 'JR Tokaido',            operator: 'JR West',    lat: 34.9858, lon: 135.7588 },
  { name: 'Nagoya',   name_ja: '名古屋',  line: 'JR Tokaido Shinkansen', operator: 'JR Central', lat: 35.1709, lon: 136.8815 },
  { name: 'Hakata',   name_ja: '博多',    line: 'JR Sanyo Shinkansen',   operator: 'JR Kyushu',  lat: 33.5897, lon: 130.4207 },
  { name: 'Sapporo',  name_ja: '札幌',    line: 'JR Hakodate',           operator: 'JR Hokkaido', lat: 43.0687, lon: 141.3508 },
  { name: 'Sendai',   name_ja: '仙台',    line: 'JR Tohoku Shinkansen',  operator: 'JR East',    lat: 38.2601, lon: 140.8822 },
  { name: 'Hiroshima', name_ja: '広島',   line: 'JR Sanyo Shinkansen',   operator: 'JR West',    lat: 34.3978, lon: 132.4752 },
  { name: 'Naha',     name_ja: '那覇',    line: 'Yui Rail',              operator: 'Okinawa Monorail', lat: 26.2058, lon: 127.6517 },
];

/**
 * Normalize a MLIT N02 feature into the layer's canonical shape.
 * Handles both "raw N02" field names (N02_003, N02_004, N02_005) and the
 * friendlier attribute names that most converters emit.
 */
function normalizeFeature(f, i) {
  if (!f || !f.geometry) return null;
  const props = f.properties || {};
  const lineName =
    props.railwayLineName || props.line_name || props.line
    || props.N02_003 || null;
  const operator =
    props.operatorCompany || props.operator || props.company
    || props.N02_004 || null;
  const stationName =
    props.stationName || props.station_name || props.name
    || props.N02_005 || null;

  let coords = null;
  if (f.geometry.type === 'Point') {
    coords = f.geometry.coordinates;
  } else if (f.geometry.type === 'LineString') {
    const mid = f.geometry.coordinates[Math.floor(f.geometry.coordinates.length / 2)];
    coords = mid;
  } else if (f.geometry.type === 'MultiLineString') {
    const first = f.geometry.coordinates[0];
    if (first && first.length) coords = first[Math.floor(first.length / 2)];
  }
  if (!coords || coords.length < 2) return null;

  return {
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [coords[0], coords[1]] },
    properties: {
      station_id: `MLIT_N02_${i}`,
      name: stationName,
      name_ja: stationName,
      line: lineName,
      operator,
      classification: props.railwayType || props.N02_001 || null,
      institution_type: props.institutionType || props.N02_002 || null,
      source: 'mlit_n02',
    },
  };
}

function generateSeedData() {
  return SEED_STATIONS.map((s, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [s.lon, s.lat] },
    properties: {
      station_id: `MLIT_N02_SEED_${i}`,
      name: s.name,
      name_ja: s.name_ja,
      line: s.line,
      operator: s.operator,
      source: 'mlit_n02_seed',
    },
  }));
}

async function tryUrl(url) {
  const data = await fetchJson(url, { timeoutMs: 30000 });
  if (!data) return null;
  // Accept both FeatureCollection and raw array-of-features.
  const raw = Array.isArray(data) ? data
    : Array.isArray(data.features) ? data.features
    : null;
  if (!raw) return null;
  const features = raw.map(normalizeFeature).filter(Boolean);
  return features.length > 0 ? features : null;
}

/**
 * Live nationwide fallback: pull every railway station mapped in OSM via the
 * tiled Overpass helper. OSM coverage of JR/private lines in Japan is at
 * parity with MLIT N02 in practice (~10k stations).
 */
async function tryOsmStations() {
  return fetchOverpassTiled(
    (bbox) => [
      `node["railway"="station"](${bbox});`,
      `node["railway"="halt"](${bbox});`,
      `node["railway"="tram_stop"](${bbox});`,
      `node["station"="subway"](${bbox});`,
      `way["railway"="station"](${bbox});`,
    ].join(''),
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        station_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || 'Station',
        name_ja: el.tags?.name || el.tags?.['name:ja'] || null,
        line: el.tags?.line || el.tags?.network || null,
        operator: el.tags?.operator || null,
        classification: el.tags?.railway || el.tags?.station || null,
        wikidata: el.tags?.wikidata || null,
        country: 'JP',
        source: 'osm_overpass_railway',
      },
    }),
    { queryTimeout: 180, timeoutMs: 90_000 },
  );
}

export default async function collectMlitN02Stations() {
  const urls = [ENV_URL, ...MIRROR_URLS].filter(Boolean);
  let features = null;
  let usedUrl = null;
  let liveSrc = null;

  for (const url of urls) {
    const result = await tryUrl(url);
    if (result) {
      features = result;
      usedUrl = url;
      liveSrc = 'mlit_n02_live';
      break;
    }
  }

  if (!features) {
    features = await tryOsmStations();
    if (features && features.length) liveSrc = 'osm_overpass_railway';
  }

  const live = !!(features && features.length);
  if (!live) features = generateSeedData();

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: liveSrc || 'mlit_n02_seed',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      source_url: usedUrl,
      env_hint: live ? null : 'Set MLIT_N02_GEOJSON_URL to a hosted GeoJSON copy of MLIT KSJ N02 to enable full nationwide coverage (~10,000 stations).',
      description: 'MLIT KSJ N02 — authoritative nationwide rail stations (yearly-updated government dataset)',
    },
    metadata: {},
  };
}
