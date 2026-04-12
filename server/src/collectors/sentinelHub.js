/**
 * Sentinel-2 Collector (multi-source, free-first)
 *
 * Pulls recent Sentinel-2 L2A scene footprints over Japan. Each feature is a
 * scene centroid with acquisition date, cloud cover and preview/asset URL.
 *
 * Fallback chain (first to return results wins):
 *   1. Sentinel Hub Catalog   — if SENTINELHUB_CLIENT_ID/SECRET set (paid/freemium)
 *   2. Element84 Earth Search — public STAC, no auth (AWS Open Data, free)
 *   3. Microsoft Planetary Computer STAC — no auth for browse (free)
 *   4. Copernicus Data Space OData — no auth for browse (free, official ESA)
 *   5. Seeded MGRS tile centroids over Japan — offline fallback
 *
 * Japan bbox: lon 122..154, lat 24..46
 */

const CLIENT_ID = process.env.SENTINELHUB_CLIENT_ID || '';
const CLIENT_SECRET = process.env.SENTINELHUB_CLIENT_SECRET || '';

const TOKEN_URL = 'https://services.sentinel-hub.com/oauth/token';
const CATALOG_URL = 'https://services.sentinel-hub.com/api/v1/catalog/1.0.0/search';
const EARTH_SEARCH_URL = 'https://earth-search.aws.element84.com/v1/search';
const PLANETARY_COMPUTER_URL = 'https://planetarycomputer.microsoft.com/api/stac/v1/search';
const CDSE_ODATA_URL = 'https://catalogue.dataspace.copernicus.eu/odata/v1/Products';

const JAPAN_BBOX = [122, 24, 154, 46];
const LOOKBACK_DAYS = 10;
const CLOUD_MAX = 40;
const SCENE_LIMIT = 100;

function isoWindow(days = LOOKBACK_DAYS) {
  const to = new Date();
  const from = new Date(Date.now() - days * 24 * 3600 * 1000);
  return { from: from.toISOString(), to: to.toISOString() };
}

function centroidFromGeom(geom) {
  try {
    const ring = geom?.type === 'Polygon'
      ? geom.coordinates?.[0]
      : geom?.type === 'MultiPolygon'
        ? geom.coordinates?.[0]?.[0]
        : null;
    if (!ring?.length) return [139, 35];
    const sum = ring.reduce((acc, [x, y]) => { acc[0] += x; acc[1] += y; return acc; }, [0, 0]);
    return [sum[0] / ring.length, sum[1] / ring.length];
  } catch { return [139, 35]; }
}

function mapStacFeature(f, i, sourceTag) {
  const geom = f.geometry || null;
  const centroid = centroidFromGeom(geom);
  const preview = f.assets?.thumbnail?.href
    || f.assets?.preview?.href
    || f.assets?.visual?.href
    || f.assets?.rendered_preview?.href
    || null;
  return {
    type: 'Feature',
    geometry: { type: 'Point', coordinates: centroid },
    properties: {
      id: `S2_${f.id || i}`,
      scene_id: f.id,
      datetime: f.properties?.datetime,
      cloud_cover: f.properties?.['eo:cloud_cover'],
      platform: f.properties?.platform || 'Sentinel-2',
      country: 'JP',
      bbox_geom: geom,
      preview,
      source: sourceTag,
    },
  };
}

async function fetchJson(url, opts = {}, timeoutMs = 12000) {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeoutMs);
  try {
    const res = await fetch(url, { ...opts, signal: ctrl.signal });
    if (!res.ok) return null;
    return await res.json();
  } catch { return null; }
  finally { clearTimeout(timer); }
}

// ── 1. Sentinel Hub (needs creds) ───────────────────────────────────────
async function getAccessToken() {
  if (!CLIENT_ID || !CLIENT_SECRET) return null;
  const body = new URLSearchParams({
    grant_type: 'client_credentials',
    client_id: CLIENT_ID,
    client_secret: CLIENT_SECRET,
  });
  const data = await fetchJson(TOKEN_URL, {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body,
  }, 10000);
  return data?.access_token || null;
}

async function trySentinelHub() {
  const token = await getAccessToken();
  if (!token) return null;
  const { from, to } = isoWindow(30);
  const body = {
    bbox: JAPAN_BBOX,
    datetime: `${from}/${to}`,
    collections: ['sentinel-2-l2a'],
    limit: SCENE_LIMIT,
    filter: { op: '<=', args: [{ property: 'eo:cloud_cover' }, CLOUD_MAX] },
  };
  const data = await fetchJson(CATALOG_URL, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      Authorization: `Bearer ${token}`,
    },
    body: JSON.stringify(body),
  });
  const feats = data?.features || [];
  if (!feats.length) return null;
  return feats.map((f, i) => mapStacFeature(f, i, 'sentinel_hub_catalog'));
}

// ── 2. Element84 Earth Search (free, no auth) ───────────────────────────
async function tryEarthSearch() {
  const { from, to } = isoWindow();
  const body = {
    bbox: JAPAN_BBOX,
    datetime: `${from}/${to}`,
    collections: ['sentinel-2-l2a'],
    limit: SCENE_LIMIT,
    query: { 'eo:cloud_cover': { lt: CLOUD_MAX } },
  };
  const data = await fetchJson(EARTH_SEARCH_URL, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  });
  const feats = data?.features || [];
  if (!feats.length) return null;
  return feats.map((f, i) => mapStacFeature(f, i, 'earth_search_aws'));
}

// ── 3. Microsoft Planetary Computer (free, no auth for browse) ──────────
async function tryPlanetaryComputer() {
  const { from, to } = isoWindow();
  const body = {
    bbox: JAPAN_BBOX,
    datetime: `${from}/${to}`,
    collections: ['sentinel-2-l2a'],
    limit: SCENE_LIMIT,
    query: { 'eo:cloud_cover': { lt: CLOUD_MAX } },
  };
  const data = await fetchJson(PLANETARY_COMPUTER_URL, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  });
  const feats = data?.features || [];
  if (!feats.length) return null;
  return feats.map((f, i) => mapStacFeature(f, i, 'planetary_computer'));
}

// ── 4. Copernicus Data Space OData (free, no auth for browse) ───────────
async function tryCdseOData() {
  const { from, to } = isoWindow();
  const [w, s, e, n] = JAPAN_BBOX;
  const polygon = `POLYGON((${w} ${s},${e} ${s},${e} ${n},${w} ${n},${w} ${s}))`;
  const filter = [
    `Collection/Name eq 'SENTINEL-2'`,
    `OData.CSC.Intersects(area=geography'SRID=4326;${polygon}')`,
    `ContentDate/Start ge ${from}`,
    `ContentDate/Start le ${to}`,
    `Attributes/OData.CSC.DoubleAttribute/any(att:att/Name eq 'cloudCover' and att/OData.CSC.DoubleAttribute/Value lt ${CLOUD_MAX}.0)`,
    `contains(Name,'MSIL2A')`,
  ].join(' and ');
  const url = `${CDSE_ODATA_URL}?$filter=${encodeURIComponent(filter)}&$top=${SCENE_LIMIT}&$orderby=ContentDate/Start desc`;
  const data = await fetchJson(url, { headers: { Accept: 'application/json' } });
  const items = data?.value || [];
  if (!items.length) return null;
  return items.map((it, i) => {
    const geom = it.GeoFootprint || null;
    const centroid = centroidFromGeom(geom);
    const cc = it.Attributes?.find?.((a) => a.Name === 'cloudCover')?.Value ?? null;
    return {
      type: 'Feature',
      geometry: { type: 'Point', coordinates: centroid },
      properties: {
        id: `S2_${it.Id || i}`,
        scene_id: it.Name,
        datetime: it.ContentDate?.Start,
        cloud_cover: cc,
        platform: 'Sentinel-2',
        country: 'JP',
        bbox_geom: geom,
        preview: `https://catalogue.dataspace.copernicus.eu/odata/v1/Products(${it.Id})/$value`,
        source: 'cdse_odata',
      },
    };
  });
}

// ── 5. Seeded MGRS tile centroids (offline fallback) ────────────────────
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
  const providers = [
    { name: 'sentinel_hub_catalog', fn: trySentinelHub },
    { name: 'earth_search_aws', fn: tryEarthSearch },
    { name: 'planetary_computer', fn: tryPlanetaryComputer },
    { name: 'cdse_odata', fn: tryCdseOData },
  ];

  let features = null;
  let liveSource = null;
  for (const p of providers) {
    try {
      const result = await p.fn();
      if (result && result.length > 0) {
        features = result;
        liveSource = p.name;
        break;
      }
    } catch { /* try next provider */ }
  }

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
      description: 'Sentinel-2 L2A scene footprints over Japan (last 10 days, <40% cloud)',
    },
    metadata: {},
  };
}
