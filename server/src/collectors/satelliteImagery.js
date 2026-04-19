/**
 * Satellite Imagery Collector (multi-provider, free-first)
 *
 * Returns scene centroids for Japan with preview_url / tile_url that the map
 * popup renders as a live image/tile feed.
 *
 * Provider chain (each adds to the result set — this collector aggregates,
 * not first-wins):
 *   1. Himawari-9 AHI (NICT / RAMMB, no auth, 10-min full-disk)
 *   2. MODIS daily mosaic via NASA GIBS WMTS (no auth)
 *   3. VIIRS daily mosaic via NASA GIBS WMTS (no auth)
 *   4. [added in later tasks] Landsat 8/9, GOES-18, ALOS, CORONA
 *
 * Seeded fallback emitted when all live providers fail.
 */

import {
  JAPAN_BBOX,
  IMAGERY_SEED_CENTROIDS,
} from './_satelliteSeeds.js';

const NOW_ISO = () => new Date().toISOString();
const TODAY_YMD = () => new Date().toISOString().slice(0, 10);

async function fetchJson(url, opts = {}, timeoutMs = 10000) {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeoutMs);
  try {
    const res = await fetch(url, { ...opts, signal: ctrl.signal });
    if (!res.ok) return null;
    return await res.json();
  } catch { return null; }
  finally { clearTimeout(timer); }
}

// ── 1. Himawari-9 (NICT + RAMMB) ──────────────────────────────────────────
// NICT publishes a REST "latest" listing for their tile service at
// https://himawari.asia/img/D531106/latest.json which returns { date: "YYYY-MM-DD HH:MM:SS" }
// The tile URL template is known.
async function tryHimawari() {
  const latest = await fetchJson(
    'https://himawari.asia/img/D531106/latest.json',
    {},
    8000
  );
  const date = latest?.date || null;
  const iso = date ? new Date(date.replace(' ', 'T') + 'Z').toISOString() : NOW_ISO();
  // Tile URL (JMA/Himawari true color band). NICT publishes tiles under
  // https://himawari.asia/img/D531106/<N>d/<YYYY>/<MM>/<DD>/<HHMMSS>_<x>_<y>.png
  // We expose a tile template for the popup to use.
  const tileTemplate = date
    ? `https://himawari.asia/img/D531106/8d/${date.slice(0,4)}/${date.slice(5,7)}/${date.slice(8,10)}/${date.slice(11,13)}${date.slice(14,16)}00_{x}_{y}.png`
    : null;

  // Also include a RAMMB SLIDER-style preview URL (JPEG thumbnail of Japan region).
  // RAMMB: https://rammb-slider.cira.colostate.edu/data/imagery/<YYYYMMDD>/himawari---full_disk/geocolor/<YYYYMMDDHHMMSS>/04/005_004.png
  // We only emit one "centroid" feature for the Japan region, not a tile grid.
  return [{
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [138.0, 36.0] }, // ~central Japan
    properties: {
      id: `IMG_HIMAWARI_${(date || iso).replace(/\D/g, '')}`,
      platform: 'Himawari-9',
      sensor: 'AHI',
      scene_id: date || iso,
      datetime: iso,
      cloud_cover: null,
      preview_url: date
        ? `https://himawari.asia/img/D531106/8d/${date.slice(0,4)}/${date.slice(5,7)}/${date.slice(8,10)}/${date.slice(11,13)}${date.slice(14,16)}00_3_3.png`
        : null,
      tile_url: tileTemplate,
      archive_era: 'real-time',
      source: 'nict_himawari',
      country: 'JP',
    },
  }];
}

// ── 2. MODIS via NASA GIBS WMTS ───────────────────────────────────────────
// GIBS exposes daily mosaics as tile layers. We emit one feature per sat
// (Terra + Aqua), each with a tile_url template pointing at today's mosaic.
function gibsModis() {
  const day = TODAY_YMD();
  return [
    {
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [138.0, 36.0] },
      properties: {
        id: `IMG_MODIS_TERRA_${day}`,
        platform: 'Terra',
        sensor: 'MODIS',
        scene_id: `MODIS_Terra_${day}`,
        datetime: `${day}T00:00:00Z`,
        cloud_cover: null,
        preview_url: `https://gibs.earthdata.nasa.gov/image-download?TIME=${day}&extent=${JAPAN_BBOX.join(',')}&epsg=4326&layers=MODIS_Terra_CorrectedReflectance_TrueColor&opacities=1&worldfile=false&format=image/jpeg&width=600&height=400`,
        tile_url: `https://gibs.earthdata.nasa.gov/wmts/epsg4326/best/MODIS_Terra_CorrectedReflectance_TrueColor/default/${day}/250m/{z}/{y}/{x}.jpg`,
        archive_era: 'real-time',
        source: 'nasa_gibs',
        country: 'JP',
      },
    },
    {
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [138.0, 36.0] },
      properties: {
        id: `IMG_MODIS_AQUA_${day}`,
        platform: 'Aqua',
        sensor: 'MODIS',
        scene_id: `MODIS_Aqua_${day}`,
        datetime: `${day}T00:00:00Z`,
        cloud_cover: null,
        preview_url: `https://gibs.earthdata.nasa.gov/image-download?TIME=${day}&extent=${JAPAN_BBOX.join(',')}&epsg=4326&layers=MODIS_Aqua_CorrectedReflectance_TrueColor&opacities=1&worldfile=false&format=image/jpeg&width=600&height=400`,
        tile_url: `https://gibs.earthdata.nasa.gov/wmts/epsg4326/best/MODIS_Aqua_CorrectedReflectance_TrueColor/default/${day}/250m/{z}/{y}/{x}.jpg`,
        archive_era: 'real-time',
        source: 'nasa_gibs',
        country: 'JP',
      },
    },
  ];
}

// ── 3. VIIRS via NASA GIBS WMTS ───────────────────────────────────────────
function gibsViirs() {
  const day = TODAY_YMD();
  return [{
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [138.0, 36.0] },
    properties: {
      id: `IMG_VIIRS_SNPP_${day}`,
      platform: 'Suomi NPP',
      sensor: 'VIIRS',
      scene_id: `VIIRS_SNPP_${day}`,
      datetime: `${day}T00:00:00Z`,
      cloud_cover: null,
      preview_url: `https://gibs.earthdata.nasa.gov/image-download?TIME=${day}&extent=${JAPAN_BBOX.join(',')}&epsg=4326&layers=VIIRS_SNPP_CorrectedReflectance_TrueColor&opacities=1&worldfile=false&format=image/jpeg&width=600&height=400`,
      tile_url: `https://gibs.earthdata.nasa.gov/wmts/epsg4326/best/VIIRS_SNPP_CorrectedReflectance_TrueColor/default/${day}/250m/{z}/{y}/{x}.jpg`,
      archive_era: 'real-time',
      source: 'nasa_gibs',
      country: 'JP',
    },
  }];
}

// ── 4. Landsat 8/9 via Microsoft Planetary Computer STAC (no auth) ──────
async function tryLandsatPC() {
  const now = new Date();
  const from = new Date(Date.now() - 14 * 86400e3).toISOString();
  const to = now.toISOString();
  const body = {
    bbox: JAPAN_BBOX,
    datetime: `${from}/${to}`,
    collections: ['landsat-c2-l2'],
    limit: 20,
    query: { 'eo:cloud_cover': { lt: 50 } },
  };
  const data = await fetchJson(
    'https://planetarycomputer.microsoft.com/api/stac/v1/search',
    {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    }
  );
  const feats = data?.features || [];
  if (!feats.length) return null;
  return feats.map((f, i) => {
    const geom = f.geometry || null;
    const ring = geom?.coordinates?.[0] || [];
    const [cx, cy] = ring.length
      ? ring.reduce((a, p) => [a[0] + p[0], a[1] + p[1]], [0, 0]).map((v) => v / ring.length)
      : [138, 36];
    const thumb = f.assets?.rendered_preview?.href
      || f.assets?.thumbnail?.href
      || null;
    return {
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [cx, cy] },
      properties: {
        id: `IMG_LANDSAT_${f.id || i}`,
        platform: f.properties?.platform || 'Landsat-9',
        sensor: 'OLI',
        scene_id: f.id,
        datetime: f.properties?.datetime,
        cloud_cover: f.properties?.['eo:cloud_cover'] ?? null,
        preview_url: thumb,
        tile_url: null,
        bbox_geom: geom,
        archive_era: 'real-time',
        source: 'planetary_computer',
        country: 'JP',
      },
    };
  });
}

// ── 5. NOAA GOES-18 (West Pacific edge) via RAMMB SLIDER ─────────────────
function rammbGoes() {
  const now = new Date();
  const y = now.getUTCFullYear();
  const m = String(now.getUTCMonth() + 1).padStart(2, '0');
  const d = String(now.getUTCDate()).padStart(2, '0');
  // SLIDER publishes a tile archive; we emit a latest-image preview URL.
  const stamp = `${y}${m}${d}`;
  return [{
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [150.0, 35.0] }, // east of Japan
    properties: {
      id: `IMG_GOES18_${stamp}`,
      platform: 'GOES-18',
      sensor: 'ABI',
      scene_id: `GOES18_${stamp}`,
      datetime: now.toISOString(),
      cloud_cover: null,
      preview_url: `https://rammb-slider.cira.colostate.edu/data/imagery/${stamp}/goes-18---full_disk/geocolor/latest/04/latest.png`,
      tile_url: null,
      archive_era: 'real-time',
      source: 'rammb_slider',
      country: 'JP',
    },
  }];
}

// ── 6. ALOS-2 / PALSAR-2 — JAXA G-Portal (auth-gated). Seed only here;
//     live ingestion requires JAXA_GPORTAL_TOKEN and is not implemented
//     because the portal's browse API requires an authenticated session.
function alos2Seed() {
  const day = TODAY_YMD();
  return [{
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [138.0, 36.0] },
    properties: {
      id: `IMG_ALOS2_SEED_${day}`,
      platform: 'ALOS-2',
      sensor: 'PALSAR-2',
      scene_id: `ALOS2_${day}`,
      datetime: `${day}T00:00:00Z`,
      cloud_cover: null,
      preview_url: 'https://www.eorc.jaxa.jp/ALOS-2/en/img_up/dis_pal2_sample.png',
      tile_url: null,
      archive_era: 'real-time',
      source: 'jaxa_alos2_seed',
      country: 'JP',
      note: 'Seed only — JAXA G-Portal browse requires auth.',
    },
  }];
}

// ── 7. CORONA + historical Landsat 1–5 via USGS EarthExplorer M2M ──────
// Token-gated; returns null when USGS_M2M_TOKEN missing.
async function tryUsgsHistorical() {
  const token = process.env.USGS_M2M_TOKEN;
  if (!token) return null;

  const datasets = ['corona2', 'landsat_mss_c2_l1']; // CORONA + Landsat 1-5 MSS
  const out = [];
  for (const dataset of datasets) {
    const body = {
      datasetName: dataset,
      spatialFilter: {
        filterType: 'mbr',
        lowerLeft:  { latitude: JAPAN_BBOX[1], longitude: JAPAN_BBOX[0] },
        upperRight: { latitude: JAPAN_BBOX[3], longitude: JAPAN_BBOX[2] },
      },
      maxResults: 20,
    };
    const data = await fetchJson(
      'https://m2m.cr.usgs.gov/api/api/json/stable/scene-search',
      {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'X-Auth-Token': token,
        },
        body: JSON.stringify(body),
      },
      12000
    );
    const results = data?.data?.results || [];
    for (const r of results) {
      const cx = r.spatialCoverage?.centroid?.longitude
        ?? (r.spatialBounds?.coordinates?.[0]?.[0]?.[0] ?? 138);
      const cy = r.spatialCoverage?.centroid?.latitude
        ?? (r.spatialBounds?.coordinates?.[0]?.[0]?.[1] ?? 36);
      out.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [cx, cy] },
        properties: {
          id: `IMG_USGS_${dataset}_${r.entityId}`,
          platform: dataset === 'corona2' ? 'CORONA' : 'Landsat 1-5',
          sensor: dataset === 'corona2' ? 'KH-4B/KH-9' : 'MSS',
          scene_id: r.entityId,
          datetime: r.temporalCoverage?.endDate || r.publishDate || null,
          cloud_cover: r.cloudCover ?? null,
          preview_url: r.browse?.[0]?.browsePath || null,
          tile_url: null,
          archive_era: 'historical',
          source: 'usgs_m2m',
          country: 'JP',
        },
      });
    }
  }
  return out.length ? out : null;
}

// ── Seed fallback ─────────────────────────────────────────────────────────
function generateSeed() {
  return IMAGERY_SEED_CENTROIDS.map((t, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [t.lon, t.lat] },
    properties: {
      id: `IMG_SEED_${String(i + 1).padStart(4, '0')}`,
      platform: 'generic',
      sensor: 'generic',
      scene_id: t.region,
      datetime: NOW_ISO(),
      cloud_cover: null,
      preview_url: null,
      tile_url: null,
      archive_era: 'real-time',
      source: 'satellite_imagery_seed',
      region: t.region,
      country: 'JP',
    },
  }));
}

export default async function collectSatelliteImagery() {
  const all = [];
  const liveSources = [];

  const providers = [
    { name: 'nict_himawari',    fn: tryHimawari },
    { name: 'nasa_gibs_modis',  fn: async () => gibsModis() },
    { name: 'nasa_gibs_viirs',  fn: async () => gibsViirs() },
    { name: 'planetary_computer_landsat', fn: tryLandsatPC },
    { name: 'rammb_slider_goes18', fn: async () => rammbGoes() },
    { name: 'jaxa_alos2_seed',  fn: async () => alos2Seed() },
    { name: 'usgs_m2m_historical', fn: tryUsgsHistorical },
  ];

  for (const p of providers) {
    try {
      const features = await p.fn();
      if (features && features.length > 0) {
        all.push(...features);
        liveSources.push(p.name);
      }
    } catch { /* try next */ }
  }

  const live = all.length > 0;
  const features = live ? all : generateSeed();

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'satellite-imagery',
      fetchedAt: NOW_ISO(),
      recordCount: features.length,
      live,
      live_source: live ? liveSources.join('+') : 'satellite_imagery_seed',
      description: 'Live + archival satellite imagery over Japan (Himawari-9, MODIS, VIIRS; Landsat/GOES/ALOS/CORONA added by extension tasks)',
    },
    metadata: {},
  };
}
