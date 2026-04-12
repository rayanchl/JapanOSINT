/**
 * Sentinel Hub Collector
 * Pulls recent Sentinel-2 L2A scene footprints over Japan from the
 * Sentinel Hub Catalog API. Each feature is a scene polygon with
 * acquisition date, cloud cover and a preview URL.
 *
 * Live: OAuth2 client_credentials → POST /catalog/1.0.0/search
 * Docs: https://docs.sentinel-hub.com/api/latest/api/catalog/
 *
 * Env: SENTINELHUB_CLIENT_ID, SENTINELHUB_CLIENT_SECRET
 * Japan bbox: lon 122..154, lat 24..46
 */

const CLIENT_ID = process.env.SENTINELHUB_CLIENT_ID || '';
const CLIENT_SECRET = process.env.SENTINELHUB_CLIENT_SECRET || '';

const TOKEN_URL = 'https://services.sentinel-hub.com/oauth/token';
const CATALOG_URL = 'https://services.sentinel-hub.com/api/v1/catalog/1.0.0/search';

async function getAccessToken() {
  if (!CLIENT_ID || !CLIENT_SECRET) return null;
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), 10000);
    const body = new URLSearchParams({
      grant_type: 'client_credentials',
      client_id: CLIENT_ID,
      client_secret: CLIENT_SECRET,
    });
    const res = await fetch(TOKEN_URL, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body,
      signal: ctrl.signal,
    });
    clearTimeout(timer);
    if (!res.ok) return null;
    const data = await res.json();
    return data.access_token || null;
  } catch { return null; }
}

async function tryCatalog() {
  const token = await getAccessToken();
  if (!token) return null;
  const to = new Date();
  const from = new Date(Date.now() - 30 * 24 * 3600 * 1000);
  const body = {
    bbox: [122, 24, 154, 46],
    datetime: `${from.toISOString()}/${to.toISOString()}`,
    collections: ['sentinel-2-l2a'],
    limit: 100,
    filter: { op: '<=', args: [{ property: 'eo:cloud_cover' }, 40] },
  };
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), 12000);
    const res = await fetch(CATALOG_URL, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        Authorization: `Bearer ${token}`,
      },
      body: JSON.stringify(body),
      signal: ctrl.signal,
    });
    clearTimeout(timer);
    if (!res.ok) return null;
    const data = await res.json();
    const feats = data?.features || [];
    if (!feats.length) return null;
    return feats.map((f, i) => {
      const geom = f.geometry || null;
      const centroid = geom?.type === 'Polygon' && geom.coordinates?.[0]?.length
        ? geom.coordinates[0].reduce(
            (acc, [x, y]) => { acc[0] += x; acc[1] += y; return acc; },
            [0, 0],
          ).map((v) => v / geom.coordinates[0].length)
        : [139, 35];
      return {
        type: 'Feature',
        geometry: { type: 'Point', coordinates: centroid },
        properties: {
          id: `S2_${f.id || i}`,
          scene_id: f.id,
          datetime: f.properties?.datetime,
          cloud_cover: f.properties?.['eo:cloud_cover'],
          platform: f.properties?.platform,
          country: 'JP',
          bbox_geom: geom,
          preview: f.assets?.thumbnail?.href,
          source: 'sentinel_hub_catalog',
        },
      };
    });
  } catch { return null; }
}

// Seed: approximate Sentinel-2 tile centroids over Japan (MGRS 53/54/55 S/T zones)
const SEED_TILES = [
  { tile: '53SMT', lat: 34.7, lon: 135.5, region: 'Kansai' },
  { tile: '53SNS', lat: 34.6, lon: 136.4, region: 'Tokai' },
  { tile: '54SUE', lat: 35.7, lon: 139.7, region: 'Kanto (Tokyo)' },
  { tile: '54SVE', lat: 35.5, lon: 140.5, region: 'Chiba' },
  { tile: '54SUF', lat: 36.5, lon: 139.7, region: 'Tochigi' },
  { tile: '54STH', lat: 38.3, lon: 140.9, region: 'Tohoku (Sendai)' },
  { tile: '54SUH', lat: 39.7, lon: 140.1, region: 'Akita' },
  { tile: '54STK', lat: 41.8, lon: 140.7, region: 'Hokkaido south' },
  { tile: '54SUK', lat: 42.8, lon: 141.3, region: 'Sapporo' },
  { tile: '54SVJ', lat: 43.0, lon: 144.3, region: 'Kushiro' },
  { tile: '52SDF', lat: 33.5, lon: 130.4, region: 'Fukuoka' },
  { tile: '52SCE', lat: 32.8, lon: 130.7, region: 'Kumamoto' },
  { tile: '52SDE', lat: 32.8, lon: 131.4, region: 'Oita/Miyazaki' },
  { tile: '52SCF', lat: 33.5, lon: 129.9, region: 'Nagasaki' },
  { tile: '52SCG', lat: 34.4, lon: 132.5, region: 'Hiroshima' },
  { tile: '52SDH', lat: 34.3, lon: 134.0, region: 'Shikoku' },
  { tile: '51RVK', lat: 26.2, lon: 127.7, region: 'Okinawa Naha' },
  { tile: '51RUL', lat: 24.3, lon: 124.2, region: 'Ishigaki' },
  { tile: '51RUK', lat: 24.8, lon: 125.3, region: 'Miyako' },
  { tile: '54TWN', lat: 45.3, lon: 141.7, region: 'Wakkanai' },
];

function generateSeed() {
  return SEED_TILES.map((t, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [t.lon, t.lat] },
    properties: {
      id: `S2_SEED_${String(i + 1).padStart(4, '0')}`,
      scene_id: t.tile,
      datetime: new Date().toISOString(),
      cloud_cover: null,
      platform: 'Sentinel-2',
      region: t.region,
      country: 'JP',
      source: 'sentinel_hub_seed',
    },
  }));
}

export default async function collectSentinelHub() {
  let features = await tryCatalog();
  let liveSource = 'sentinel_hub_catalog';
  const live = !!(features && features.length > 0);
  if (!live) {
    features = generateSeed();
    liveSource = 'sentinel_hub_seed';
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'sentinel-hub',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: liveSource,
      description: 'Sentinel-2 L2A scene footprints over Japan (last 30 days, <40% cloud)',
    },
    metadata: {},
  };
}
