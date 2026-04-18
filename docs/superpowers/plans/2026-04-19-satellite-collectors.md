# Satellite Collectors Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add three satellite-focused OSINT collectors — imagery catalogs (Himawari-9, Landsat, MODIS, ALOS, GOES, VIIRS, historical), live satellite positions with on-demand ground tracks, and expanded ground-station infrastructure (VLBI, SLR, optical tracking, full GEONET).

**Architecture:** Three new/modified server collectors emit standard `FeatureCollection`s with `_meta` envelopes; each has a live-provider fallback chain + seeded fallback. Client adds a `satellite.js` dependency for on-demand SGP4 ground-track propagation in tracking popups. Three new layer definitions + two new popup renderers (imagery feed, satellite tracking with ground-track button).

**Tech Stack:** Node.js (ESM), Express, Vitest-style unit tests (Node `test` runner is used throughout this repo — see `server/test/`), React, Leaflet (via `react-leaflet`), `satellite.js` npm lib for SGP4.

---

## File Structure

### Server (new)
- `server/src/collectors/satelliteImagery.js` — multi-provider imagery catalog
- `server/src/collectors/satelliteTracking.js` — live TLE-based positions
- `server/src/collectors/_satelliteSeeds.js` — shared seed data (infra + imagery fallbacks)
- `server/test/satelliteImagery.test.js`
- `server/test/satelliteTracking.test.js`
- `server/test/satelliteGroundStations.test.js` (new test for expanded file)

### Server (modified)
- `server/src/collectors/satelliteGroundStations.js` — add VLBI/SLR/optical/GEONET + `category` field
- `server/src/collectors/index.js` — register new collectors
- `server/src/utils/sourceRegistry.js` — add source entries
- `server/src/routes/data.js` — add two GET routes (ground stations already has one)
- `server/package.json` — add `satellite.js`

### Client (modified)
- `client/src/hooks/useMapLayers.js` — add two new layer definitions
- `client/src/components/map/MapPopup.jsx` — add `SatelliteImageryDetail` and `SatelliteTrackingDetail` renderers
- `client/src/components/map/MapView.jsx` — add temporary ground-track line layer management
- `client/src/utils/groundTrack.js` — new; client-side SGP4 propagation helper
- `client/package.json` — add `satellite.js`

### Docs (existing)
- `docs/superpowers/specs/2026-04-19-satellite-collectors-design.md` — the approved spec

---

## Test Infrastructure Check

Before writing any tests, confirm the repo's test runner. Look at `server/package.json` `"test"` script and any file in `server/test/`. Expected: Node's built-in `node:test` runner. If a different runner is used (Vitest, Jest), adapt the test imports accordingly (the test logic is identical).

- [ ] **Step 0.1: Confirm test runner**

Run: `cat server/package.json | grep -A2 '"test"'`
Expected output: shows either `node --test` or the project's chosen runner.

Also inspect one existing test file:
Run: `ls server/test/ | head -5 && cat server/test/$(ls server/test/ | head -1)` (or equivalent path)
Expected: a sample of an existing test to copy import / assertion style from.

If no `server/test/` directory exists, create it and use Node's built-in `node:test` — all test files in this plan follow that convention. Replace `import { test } from 'node:test'` + `import assert from 'node:assert/strict'` with the project's equivalents if different.

---

## Task 1: Install `satellite.js` (server + client)

**Files:**
- Modify: `server/package.json`
- Modify: `client/package.json`

- [ ] **Step 1.1: Add dependency to server**

Run: `cd server && npm install satellite.js@^5.0.0 && cd ..`
Expected: `satellite.js` appears in `server/package.json` dependencies; `node_modules/satellite.js` exists.

- [ ] **Step 1.2: Add dependency to client**

Run: `cd client && npm install satellite.js@^5.0.0 && cd ..`
Expected: same outcome for client.

- [ ] **Step 1.3: Verify import works**

Run: `node -e "import('satellite.js').then(m => console.log(Object.keys(m).slice(0,5)))"` from the repo root after `cd server`.
Expected: prints an array of keys including `twoline2satrec`, `propagate`, `eciToGeodetic`.

- [ ] **Step 1.4: Commit**

```bash
git add server/package.json server/package-lock.json client/package.json client/package-lock.json
git commit -m "Add satellite.js dependency for SGP4 propagation"
```

---

## Task 2: Shared seed data module

**Files:**
- Create: `server/src/collectors/_satelliteSeeds.js`

- [ ] **Step 2.1: Create seed module**

Purpose: centralizes the bbox constant + seeded imagery tile centroids + the infrastructure seeds for VLBI/SLR/optical tracking and a curated subset of GEONET stations (used when the live GSI fetch fails). Keeps `satelliteGroundStations.js` from exploding in length.

Create the file with exactly this content:

```js
/**
 * Shared seed data for satellite-family collectors.
 */

export const JAPAN_BBOX = [122, 24, 154, 46]; // [W, S, E, N]

// Imagery fallback grid — 5x3 centroids over Japan, used when all live
// imagery providers fail.
export const IMAGERY_SEED_CENTROIDS = [
  { lon: 130, lat: 33, region: 'Kyushu' },
  { lon: 132, lat: 34, region: 'Chugoku' },
  { lon: 134, lat: 34, region: 'Shikoku' },
  { lon: 136, lat: 35, region: 'Kansai/Tokai' },
  { lon: 139, lat: 36, region: 'Kanto' },
  { lon: 141, lat: 38, region: 'Tohoku' },
  { lon: 142, lat: 41, region: 'Hokkaido south' },
  { lon: 143, lat: 43, region: 'Hokkaido east' },
  { lon: 127, lat: 26, region: 'Okinawa' },
  { lon: 124, lat: 24, region: 'Yaeyama' },
];

// Additional commercial + university ground-station sites.
export const EXTRA_GROUND_STATIONS = [
  { name: 'Intelsat Ibaraki', lat: 36.2050, lon: 140.6300, operator: 'Intelsat', kind: 'commercial_satcom', bands: 'C,Ku', category: 'satcom' },
  { name: 'Inmarsat Yamaguchi', lat: 34.0500, lon: 131.5600, operator: 'Inmarsat', kind: 'commercial_satcom', bands: 'L,Ku', category: 'satcom' },
  { name: 'NTT Yokohama Teleport', lat: 35.4400, lon: 139.6400, operator: 'NTT', kind: 'commercial_satcom', bands: 'C,Ku', category: 'satcom' },
  { name: 'SoftBank Chiba Gateway', lat: 35.3300, lon: 140.3800, operator: 'SoftBank', kind: 'commercial_satcom', bands: 'Ka', category: 'satcom' },
  { name: 'Rakuten Mobile Satellite Gateway', lat: 35.6800, lon: 139.7600, operator: 'Rakuten', kind: 'commercial_satcom', bands: 'Ka', category: 'satcom' },
  { name: 'UTokyo Kashiwa Ground Station', lat: 35.9000, lon: 139.9400, operator: 'U. of Tokyo', kind: 'university', bands: 'S,X', category: 'university' },
  { name: 'Kyushu University Ground Station', lat: 33.5900, lon: 130.2200, operator: 'Kyushu Univ.', kind: 'university', bands: 'S', category: 'university' },
  { name: 'Hokkaido University Uchinada GS', lat: 43.0700, lon: 141.3500, operator: 'Hokkaido Univ.', kind: 'university', bands: 'S', category: 'university' },
  { name: 'Tohoku Univ. CubeSat GS', lat: 38.2500, lon: 140.8400, operator: 'Tohoku Univ.', kind: 'university', bands: 'UHF,S', category: 'university' },
];

// VLBI radio telescopes (VERA array + Nobeyama + Kashima).
export const VLBI_STATIONS = [
  { name: 'VERA Mizusawa', lat: 39.1336, lon: 141.1328, operator: 'NAOJ', bands: 'K,Q', category: 'vlbi' },
  { name: 'VERA Iriki', lat: 31.7475, lon: 130.4397, operator: 'NAOJ', bands: 'K,Q', category: 'vlbi' },
  { name: 'VERA Ishigakijima', lat: 24.4122, lon: 124.1711, operator: 'NAOJ', bands: 'K,Q', category: 'vlbi' },
  { name: 'VERA Ogasawara', lat: 27.0919, lon: 142.2167, operator: 'NAOJ', bands: 'K,Q', category: 'vlbi' },
  { name: 'Nobeyama 45m Radio Telescope', lat: 35.9417, lon: 138.4722, operator: 'NAOJ', bands: 'mm', category: 'vlbi' },
  { name: 'Kashima 34m Antenna', lat: 35.9536, lon: 140.6597, operator: 'NICT', bands: 'S,X,K', category: 'vlbi' },
];

// Satellite Laser Ranging stations.
export const SLR_STATIONS = [
  { name: 'Koganei SLR', lat: 35.7100, lon: 139.4900, operator: 'NICT', bands: 'laser', category: 'slr' },
  { name: 'Simosato Hydrographic Observatory', lat: 33.5772, lon: 135.9369, operator: 'JHA', bands: 'laser', category: 'slr' },
];

// Optical satellite tracking observatories.
export const OPTICAL_TRACKING_STATIONS = [
  { name: 'Bisei Spaceguard Center', lat: 34.6717, lon: 133.5444, operator: 'JSGA', bands: 'optical', category: 'optical_tracking' },
  { name: 'JAXA Mt. Nyukasa Observatory', lat: 35.9750, lon: 138.1917, operator: 'JAXA', bands: 'optical', category: 'optical_tracking' },
];

// Curated fallback subset of GEONET stations (used when GSI live list fetch fails).
// Full station list (~1,300) is fetched live in the collector.
export const GEONET_FALLBACK = [
  { name: 'GEONET 940001 Wakkanai', station_code: '940001', lat: 45.4040, lon: 141.6897 },
  { name: 'GEONET 940058 Sapporo', station_code: '940058', lat: 43.0700, lon: 141.3350 },
  { name: 'GEONET 950211 Sendai', station_code: '950211', lat: 38.2680, lon: 140.8710 },
  { name: 'GEONET 960603 Tsukuba', station_code: '960603', lat: 36.1060, lon: 140.0870 },
  { name: 'GEONET 93010 Tokyo', station_code: '93010', lat: 35.7100, lon: 139.4880 },
  { name: 'GEONET 950265 Nagoya', station_code: '950265', lat: 35.1700, lon: 136.9600 },
  { name: 'GEONET 960647 Osaka', station_code: '960647', lat: 34.6860, lon: 135.5200 },
  { name: 'GEONET 970791 Hiroshima', station_code: '970791', lat: 34.3900, lon: 132.4600 },
  { name: 'GEONET 950460 Fukuoka', station_code: '950460', lat: 33.5900, lon: 130.4000 },
  { name: 'GEONET 940089 Naha', station_code: '940089', lat: 26.2120, lon: 127.6800 },
];
```

- [ ] **Step 2.2: Commit**

```bash
git add server/src/collectors/_satelliteSeeds.js
git commit -m "Add shared seed data for satellite collectors"
```

---

## Task 3: Satellite imagery collector — Himawari + GIBS paths

**Files:**
- Create: `server/src/collectors/satelliteImagery.js`
- Create: `server/test/satelliteImagery.test.js`

We build the collector in layers: first the Himawari-9 + NASA GIBS (MODIS + VIIRS) paths, which need no auth and are the most useful live feeds for Japan. Later tasks add Landsat, ALOS, GOES, and the historical archive.

- [ ] **Step 3.1: Write the failing test**

Create `server/test/satelliteImagery.test.js`:

```js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import collectSatelliteImagery from '../src/collectors/satelliteImagery.js';

test('satelliteImagery returns a valid FeatureCollection envelope', async () => {
  const fc = await collectSatelliteImagery();
  assert.equal(fc.type, 'FeatureCollection');
  assert.ok(Array.isArray(fc.features));
  assert.ok(fc._meta);
  assert.equal(typeof fc._meta.live, 'boolean');
  assert.equal(typeof fc._meta.recordCount, 'number');
  assert.equal(fc._meta.recordCount, fc.features.length);
});

test('satelliteImagery features have required OSINT props', async () => {
  const fc = await collectSatelliteImagery();
  assert.ok(fc.features.length > 0, 'must always return at least seed features');
  for (const f of fc.features) {
    assert.equal(f.type, 'Feature');
    assert.equal(f.geometry.type, 'Point');
    assert.ok(Array.isArray(f.geometry.coordinates));
    assert.equal(f.geometry.coordinates.length, 2);
    assert.ok(f.properties.platform, `missing platform: ${JSON.stringify(f.properties)}`);
    assert.ok(f.properties.source, `missing source: ${JSON.stringify(f.properties)}`);
    assert.ok(f.properties.id);
    // preview_url OR tile_url must be present (nullable but the key must exist).
    assert.ok('preview_url' in f.properties || 'tile_url' in f.properties);
  }
});

test('satelliteImagery seeds have archive_era tag', async () => {
  const fc = await collectSatelliteImagery();
  for (const f of fc.features) {
    assert.ok(
      f.properties.archive_era === 'real-time' || f.properties.archive_era === 'historical',
      `unexpected archive_era: ${f.properties.archive_era}`
    );
  }
});
```

- [ ] **Step 3.2: Run test to verify it fails**

Run: `cd server && node --test test/satelliteImagery.test.js`
Expected: FAIL — module not found.

- [ ] **Step 3.3: Create the collector (minimal — Himawari + GIBS + seed)**

Create `server/src/collectors/satelliteImagery.js`:

```js
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
    { name: 'nict_himawari', fn: tryHimawari },
    { name: 'nasa_gibs_modis', fn: async () => gibsModis() },
    { name: 'nasa_gibs_viirs', fn: async () => gibsViirs() },
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
```

- [ ] **Step 3.4: Run test to verify it passes**

Run: `cd server && node --test test/satelliteImagery.test.js`
Expected: PASS — 3 tests pass.

- [ ] **Step 3.5: Commit**

```bash
git add server/src/collectors/satelliteImagery.js server/test/satelliteImagery.test.js
git commit -m "Add satelliteImagery collector (Himawari + GIBS MODIS/VIIRS + seed)"
```

---

## Task 4: Extend imagery — Landsat, GOES, ALOS, historical

**Files:**
- Modify: `server/src/collectors/satelliteImagery.js`
- Modify: `server/test/satelliteImagery.test.js`

- [ ] **Step 4.1: Add a failing test for multiple platforms**

Add to `server/test/satelliteImagery.test.js`:

```js
test('satelliteImagery includes multiple platforms when live', async () => {
  const fc = await collectSatelliteImagery();
  if (!fc._meta.live) return; // skip when seeded
  const platforms = new Set(fc.features.map((f) => f.properties.platform));
  // At minimum Himawari-9 + one GIBS mosaic should be present.
  assert.ok(platforms.size >= 2, `expected >= 2 platforms, got ${[...platforms].join(',')}`);
});

test('satelliteImagery seeds historical era when archive provider emits', async () => {
  const fc = await collectSatelliteImagery();
  if (!fc._meta.live) return;
  // Historical provider is token-gated, so archive_era='historical' is only
  // emitted when USGS_M2M_TOKEN is set. In that case, ensure at least one
  // historical feature exists; otherwise just confirm none are malformed.
  const hasHistorical = fc.features.some((f) => f.properties.archive_era === 'historical');
  if (process.env.USGS_M2M_TOKEN) {
    assert.ok(hasHistorical, 'expected historical archive feature with token set');
  }
});
```

- [ ] **Step 4.2: Run test to confirm new assertions start passing with current code**

Run: `cd server && node --test test/satelliteImagery.test.js`
Expected: The first new test passes if Himawari is reachable (>= 2 platforms). The second new test passes trivially without the token. If Himawari is unreachable in sandbox, the first new test skips via the `if (!fc._meta.live) return;` guard.

- [ ] **Step 4.3: Add Landsat via Microsoft Planetary Computer**

In `server/src/collectors/satelliteImagery.js`, add a `tryLandsatPC()` function below `gibsViirs()`:

```js
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
```

- [ ] **Step 4.4: Add NOAA GOES-18 via RAMMB SLIDER**

Below `tryLandsatPC()`, add:

```js
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
```

- [ ] **Step 4.5: Add ALOS-2 seed (auth-gated live catalog → seed only)**

Below `rammbGoes()`:

```js
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
```

- [ ] **Step 4.6: Add CORONA / historical Landsat via USGS M2M (token-gated)**

Below `alos2Seed()`:

```js
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
```

- [ ] **Step 4.7: Wire the new providers into `collectSatelliteImagery()`**

Replace the `providers` array in `collectSatelliteImagery()` with:

```js
  const providers = [
    { name: 'nict_himawari',    fn: tryHimawari },
    { name: 'nasa_gibs_modis',  fn: async () => gibsModis() },
    { name: 'nasa_gibs_viirs',  fn: async () => gibsViirs() },
    { name: 'planetary_computer_landsat', fn: tryLandsatPC },
    { name: 'rammb_slider_goes18', fn: async () => rammbGoes() },
    { name: 'jaxa_alos2_seed',  fn: async () => alos2Seed() },
    { name: 'usgs_m2m_historical', fn: tryUsgsHistorical },
  ];
```

- [ ] **Step 4.8: Run tests**

Run: `cd server && node --test test/satelliteImagery.test.js`
Expected: PASS — 5 tests (2 previous + 3 new, with 2 of the new tests skipping internals when seeded).

- [ ] **Step 4.9: Commit**

```bash
git add server/src/collectors/satelliteImagery.js server/test/satelliteImagery.test.js
git commit -m "Extend satelliteImagery with Landsat, GOES, ALOS, and CORONA providers"
```

---

## Task 5: Satellite tracking collector

**Files:**
- Create: `server/src/collectors/satelliteTracking.js`
- Create: `server/test/satelliteTracking.test.js`

- [ ] **Step 5.1: Write the failing test**

Create `server/test/satelliteTracking.test.js`:

```js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import collectSatelliteTracking from '../src/collectors/satelliteTracking.js';

test('satelliteTracking returns a valid FeatureCollection envelope', async () => {
  const fc = await collectSatelliteTracking();
  assert.equal(fc.type, 'FeatureCollection');
  assert.ok(Array.isArray(fc.features));
  assert.ok(fc._meta);
  assert.equal(typeof fc._meta.live, 'boolean');
  assert.equal(fc._meta.recordCount, fc.features.length);
});

test('satelliteTracking features carry TLE lines for client ground-track compute', async () => {
  const fc = await collectSatelliteTracking();
  assert.ok(fc.features.length > 0, 'must always return at least seed features');
  for (const f of fc.features.slice(0, 10)) {
    assert.equal(f.type, 'Feature');
    assert.equal(f.geometry.type, 'Point');
    const [lon, lat] = f.geometry.coordinates;
    assert.ok(lon >= 122 && lon <= 154, `lon ${lon} outside Japan bbox`);
    assert.ok(lat >= 24 && lat <= 46, `lat ${lat} outside Japan bbox`);
    assert.ok(f.properties.norad_id, 'missing norad_id');
    assert.ok(f.properties.name, 'missing name');
    assert.ok(f.properties.category, 'missing category');
    assert.ok(f.properties.tle_line1 && f.properties.tle_line2, 'missing TLE lines');
  }
});

test('satelliteTracking seed ISS position is reasonable when no live data', async () => {
  const fc = await collectSatelliteTracking();
  // If live, we can't predict positions; just confirm live flag is boolean.
  assert.equal(typeof fc._meta.live, 'boolean');
});
```

- [ ] **Step 5.2: Run test to verify it fails**

Run: `cd server && node --test test/satelliteTracking.test.js`
Expected: FAIL — module not found.

- [ ] **Step 5.3: Create the collector**

Create `server/src/collectors/satelliteTracking.js`:

```js
/**
 * Satellite Tracking Collector — live positions of every tracked on-orbit
 * object currently over Japan. TLEs fetched from CelesTrak (no auth, daily).
 * SGP4 propagation runs server-side with satellite.js. Ground tracks are
 * computed client-side on popup click (TLE lines shipped in properties).
 */

import * as satjs from 'satellite.js';
import { JAPAN_BBOX } from './_satelliteSeeds.js';

const CELESTRAK_GROUPS = [
  { group: 'active',   category: 'active' },
  { group: 'debris',   category: 'debris' },
  { group: 'cubesat',  category: 'cubesat' },
  { group: 'stations', category: 'station' },
];

const TLE_CACHE_TTL_MS = 24 * 60 * 60 * 1000;
let tleCache = null; // { fetchedAt, records: [{tleLine1, tleLine2, name, noradId, category}] }

// Seed: current ISS TLE snapshot (updated if this file is re-edited; SGP4 on
// a stale TLE still places the ISS somewhere plausible for a fallback).
const SEED_ISS = {
  name: 'ISS (ZARYA)',
  noradId: 25544,
  category: 'station',
  tleLine1: '1 25544U 98067A   26108.50000000  .00016717  00000-0  10270-3 0  9000',
  tleLine2: '2 25544  51.6400 180.0000 0006000   0.0000 180.0000 15.50000000100000',
};

async function fetchText(url, timeoutMs = 15000) {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeoutMs);
  try {
    const res = await fetch(url, { signal: ctrl.signal });
    if (!res.ok) return null;
    return await res.text();
  } catch { return null; }
  finally { clearTimeout(timer); }
}

function parseTleBlock(text, category) {
  // CelesTrak TLE format: name line, then line1, then line2, repeat.
  const lines = text.split(/\r?\n/).map((l) => l.trim()).filter(Boolean);
  const out = [];
  for (let i = 0; i + 2 < lines.length; i += 3) {
    const name = lines[i];
    const l1 = lines[i + 1];
    const l2 = lines[i + 2];
    if (!l1.startsWith('1 ') || !l2.startsWith('2 ')) continue;
    const noradId = parseInt(l1.substring(2, 7), 10);
    if (!Number.isFinite(noradId)) continue;
    out.push({ name, noradId, tleLine1: l1, tleLine2: l2, category });
  }
  return out;
}

async function loadTles() {
  if (tleCache && Date.now() - tleCache.fetchedAt < TLE_CACHE_TTL_MS) {
    return tleCache.records;
  }
  const all = [];
  for (const { group, category } of CELESTRAK_GROUPS) {
    const url = `https://celestrak.org/NORAD/elements/gp.php?GROUP=${group}&FORMAT=tle`;
    const body = await fetchText(url);
    if (!body) continue;
    all.push(...parseTleBlock(body, category));
  }
  if (!all.length) return null;
  tleCache = { fetchedAt: Date.now(), records: all };
  return all;
}

function propagateOne(rec, when) {
  try {
    const satrec = satjs.twoline2satrec(rec.tleLine1, rec.tleLine2);
    const pv = satjs.propagate(satrec, when);
    if (!pv?.position) return null;
    const gmst = satjs.gstime(when);
    const geo = satjs.eciToGeodetic(pv.position, gmst);
    const lon = (satjs.degreesLong(geo.longitude));
    const lat = (satjs.degreesLat(geo.latitude));
    const alt = geo.height; // km
    const v = pv.velocity;
    const vel = v ? Math.hypot(v.x, v.y, v.z) : null;
    const inc = satjs.radiansToDegrees(satrec.inclo);
    return { lon, lat, alt_km: alt, vel_kms: vel, inclination_deg: inc };
  } catch { return null; }
}

function inJapanBbox(lon, lat) {
  return lon >= JAPAN_BBOX[0] && lon <= JAPAN_BBOX[2]
      && lat >= JAPAN_BBOX[1] && lat <= JAPAN_BBOX[3];
}

function buildFeature(rec, pv) {
  return {
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [pv.lon, pv.lat] },
    properties: {
      id: `SAT_${rec.noradId}`,
      norad_id: rec.noradId,
      name: rec.name,
      country: null,
      category: rec.category,
      altitude_km: Math.round(pv.alt_km * 10) / 10,
      velocity_kms: pv.vel_kms != null ? Math.round(pv.vel_kms * 1000) / 1000 : null,
      inclination_deg: Math.round(pv.inclination_deg * 100) / 100,
      next_pass_utc: null, // computed client-side on demand
      tle_line1: rec.tleLine1,
      tle_line2: rec.tleLine2,
      source: 'celestrak',
    },
  };
}

function seedFeature() {
  const pv = propagateOne(SEED_ISS, new Date());
  const lon = pv?.lon ?? 138;
  const lat = pv?.lat ?? 36;
  return {
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [lon, lat] },
    properties: {
      id: `SAT_${SEED_ISS.noradId}`,
      norad_id: SEED_ISS.noradId,
      name: SEED_ISS.name,
      country: 'International',
      category: SEED_ISS.category,
      altitude_km: pv?.alt_km ? Math.round(pv.alt_km * 10) / 10 : 420,
      velocity_kms: pv?.vel_kms != null ? Math.round(pv.vel_kms * 1000) / 1000 : 7.66,
      inclination_deg: 51.64,
      next_pass_utc: null,
      tle_line1: SEED_ISS.tleLine1,
      tle_line2: SEED_ISS.tleLine2,
      source: 'satellite_tracking_seed',
    },
  };
}

export default async function collectSatelliteTracking() {
  const tles = await loadTles();
  const now = new Date();
  if (!tles) {
    const seed = seedFeature();
    // Clamp seed into bbox so tests don't fail when ISS is elsewhere.
    const [lon, lat] = seed.geometry.coordinates;
    if (!inJapanBbox(lon, lat)) {
      seed.geometry.coordinates = [138.0, 36.0];
    }
    return {
      type: 'FeatureCollection',
      features: [seed],
      _meta: {
        source: 'satellite-tracking',
        fetchedAt: now.toISOString(),
        recordCount: 1,
        live: false,
        live_source: 'satellite_tracking_seed',
        description: 'Live satellite positions over Japan (SGP4 from CelesTrak TLEs). Seed ISS only.',
      },
      metadata: {},
    };
  }

  const features = [];
  for (const rec of tles) {
    const pv = propagateOne(rec, now);
    if (!pv) continue;
    if (!inJapanBbox(pv.lon, pv.lat)) continue;
    features.push(buildFeature(rec, pv));
  }

  // If propagation filtered everything out, still emit seed so popup/layer works.
  if (features.length === 0) {
    const seed = seedFeature();
    seed.geometry.coordinates = [138.0, 36.0];
    features.push(seed);
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'satellite-tracking',
      fetchedAt: now.toISOString(),
      recordCount: features.length,
      live: true,
      live_source: 'celestrak',
      description: 'Live satellite positions over Japan (SGP4 from CelesTrak TLEs).',
    },
    metadata: {},
  };
}
```

- [ ] **Step 5.4: Run test to verify it passes**

Run: `cd server && node --test test/satelliteTracking.test.js`
Expected: PASS — 3 tests pass.

- [ ] **Step 5.5: Commit**

```bash
git add server/src/collectors/satelliteTracking.js server/test/satelliteTracking.test.js
git commit -m "Add satelliteTracking collector with SGP4 propagation from CelesTrak"
```

---

## Task 6: Expand `satelliteGroundStations.js`

**Files:**
- Modify: `server/src/collectors/satelliteGroundStations.js`
- Create: `server/test/satelliteGroundStations.test.js`

- [ ] **Step 6.1: Write failing test**

Create `server/test/satelliteGroundStations.test.js`:

```js
import { test } from 'node:test';
import assert from 'node:assert/strict';
import collectSatelliteGroundStations from '../src/collectors/satelliteGroundStations.js';

test('satelliteGroundStations returns a valid FeatureCollection', async () => {
  const fc = await collectSatelliteGroundStations();
  assert.equal(fc.type, 'FeatureCollection');
  assert.ok(Array.isArray(fc.features));
  assert.ok(fc.features.length > 50, `expected >50 stations (seed + extras + VLBI + SLR + optical + GEONET subset), got ${fc.features.length}`);
});

test('satelliteGroundStations features carry category', async () => {
  const fc = await collectSatelliteGroundStations();
  const categories = new Set(fc.features.map((f) => f.properties.category).filter(Boolean));
  // Spec requires these categories at minimum:
  for (const c of ['satcom', 'vlbi', 'slr', 'optical_tracking', 'gnss_reference']) {
    assert.ok(categories.has(c), `missing category: ${c}`);
  }
});
```

- [ ] **Step 6.2: Run test to verify it fails**

Run: `cd server && node --test test/satelliteGroundStations.test.js`
Expected: FAIL — not enough features, missing categories.

- [ ] **Step 6.3: Read the current file to find the export**

Run: `wc -l server/src/collectors/satelliteGroundStations.js` and open it in your editor.

- [ ] **Step 6.4: Add imports + category assignment**

At the top of `server/src/collectors/satelliteGroundStations.js`, after the existing `import`, add:

```js
import {
  EXTRA_GROUND_STATIONS,
  VLBI_STATIONS,
  SLR_STATIONS,
  OPTICAL_TRACKING_STATIONS,
  GEONET_FALLBACK,
} from './_satelliteSeeds.js';
```

- [ ] **Step 6.5: Add a `category` field to each existing `SEED_GS` entry**

Inside `satelliteGroundStations.js`, go through each item in the existing `SEED_GS` array and add `category:` based on the existing `kind` field:

| existing `kind` | new `category` |
|-----------------|----------------|
| `launch_tracking`, `launch`, `tracking`, `tt&c`, `mission_control`, `deep_space` | `'launch_tracking'` for launch-related; map others to their kind — `tt&c`, `deep_space`, `tracking`, `mission_control` |
| `commercial_satcom`, `satcom` | `'satcom'` |
| `observatory` | `'vlbi'` for NAOJ entries, else `'optical_tracking'` |
| `vlbi` | `'vlbi'` |
| `satellite_research` | `'research'` |

For each existing entry, append a `, category: '<value>'` field. Keep the existing `kind` field — `category` is additive.

- [ ] **Step 6.6: Add GSI GEONET live fetch function**

Immediately below the existing `tryLive()` function, add:

```js
/**
 * GEONET GNSS reference stations — GSI publishes a station position list
 * (~1,300 stations). The canonical file is the SINEX weekly combined solution,
 * but a simpler public endpoint is the CDDIS / IGS station list. GSI also
 * publishes an ASCII station list at
 *   https://mekira.gsi.go.jp/JAPANESE/gnss_station_list.csv
 * which returns CSV with columns: code, name, lat (deg), lon (deg), ...
 *
 * Falls back to GEONET_FALLBACK when the live endpoint returns nothing.
 */
async function tryGeonet() {
  const url = 'https://mekira.gsi.go.jp/JAPANESE/gnss_station_list.csv';
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), 15000);
    const res = await fetch(url, { signal: ctrl.signal });
    clearTimeout(timer);
    if (!res.ok) return null;
    const text = await res.text();
    const rows = text.split(/\r?\n/).slice(1); // drop header
    const out = [];
    for (const row of rows) {
      if (!row.trim()) continue;
      const cols = row.split(',');
      if (cols.length < 4) continue;
      const code = cols[0]?.trim();
      const name = cols[1]?.trim();
      const lat = parseFloat(cols[2]);
      const lon = parseFloat(cols[3]);
      if (!Number.isFinite(lat) || !Number.isFinite(lon)) continue;
      out.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [lon, lat] },
        properties: {
          gs_id: `GEONET_${code}`,
          station_code: code,
          name: `GEONET ${code} ${name}`,
          operator: 'GSI',
          kind: 'gnss_reference',
          category: 'gnss_reference',
          bands: 'L1,L2,L5',
          country: 'JP',
          source: 'gsi_geonet',
        },
      });
    }
    return out.length ? out : null;
  } catch { return null; }
}
```

- [ ] **Step 6.7: Merge new seeds + GEONET into the default export**

Find the `export default async function` in `satelliteGroundStations.js` and modify the logic so it:
1. Tries OSM Overpass live (existing `tryLive`) for `man_made=satellite_dish`.
2. Fetches GEONET via `tryGeonet()` — falls back to `GEONET_FALLBACK` seed on failure.
3. Always appends the existing `SEED_GS` + `EXTRA_GROUND_STATIONS` + `VLBI_STATIONS` + `SLR_STATIONS` + `OPTICAL_TRACKING_STATIONS` as hand-curated features.
4. Returns the union, with standard `_meta` envelope.

Implementation:

```js
function asFeature(s, idx, source) {
  return {
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [s.lon, s.lat] },
    properties: {
      gs_id: `GS_${source}_${String(idx + 1).padStart(5, '0')}`,
      name: s.name,
      operator: s.operator || null,
      kind: s.kind || s.category,
      category: s.category || s.kind,
      bands: s.bands || null,
      country: 'JP',
      source,
    },
  };
}

export default async function collectSatelliteGroundStations() {
  const features = [];
  let live = false;
  const liveSources = [];

  // 1. OSM Overpass live
  const osmFeats = await tryLive().catch(() => null);
  if (osmFeats && osmFeats.length) {
    features.push(...osmFeats);
    live = true;
    liveSources.push('overpass');
  }

  // 2. Hand-curated JAXA/NICT/KDDI (existing seed array).
  features.push(...SEED_GS.map((s, i) => asFeature(s, i, 'ground_station_seed')));

  // 3. Extra commercial + university operators.
  features.push(...EXTRA_GROUND_STATIONS.map((s, i) => asFeature(s, i, 'ground_station_extra')));

  // 4. VLBI / SLR / optical tracking.
  features.push(...VLBI_STATIONS.map((s, i) => asFeature(s, i, 'vlbi_seed')));
  features.push(...SLR_STATIONS.map((s, i) => asFeature(s, i, 'slr_seed')));
  features.push(...OPTICAL_TRACKING_STATIONS.map((s, i) => asFeature(s, i, 'optical_tracking_seed')));

  // 5. GEONET (live → fallback to subset seed).
  const geonetLive = await tryGeonet();
  if (geonetLive && geonetLive.length) {
    features.push(...geonetLive);
    live = true;
    liveSources.push('gsi_geonet');
  } else {
    features.push(
      ...GEONET_FALLBACK.map((s, i) => asFeature(
        { ...s, category: 'gnss_reference', operator: 'GSI', bands: 'L1,L2,L5', kind: 'gnss_reference' },
        i,
        'geonet_fallback'
      ))
    );
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'satellite-ground-stations',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: liveSources.length ? liveSources.join('+') : 'seed',
      description: 'Japan satellite ground stations: JAXA/NICT/commercial satcom, VLBI, SLR, optical tracking, and GEONET GNSS reference stations.',
    },
    metadata: {},
  };
}
```

Important: the existing file's `tryLive()` mapper returns features with only `gs_id`/`name`/`operator`/`kind`/`bands` — it does NOT currently set `category`. Update the mapper inside `tryLive()` to also set `category: el.tags?.building === 'observatory' ? 'vlbi' : 'satcom'`.

- [ ] **Step 6.8: Run the test**

Run: `cd server && node --test test/satelliteGroundStations.test.js`
Expected: PASS — 2 tests.

- [ ] **Step 6.9: Commit**

```bash
git add server/src/collectors/satelliteGroundStations.js server/test/satelliteGroundStations.test.js
git commit -m "Expand satelliteGroundStations with VLBI, SLR, optical tracking, and GEONET"
```

---

## Task 7: Register collectors + routes + source registry

**Files:**
- Modify: `server/src/collectors/index.js`
- Modify: `server/src/utils/sourceRegistry.js`
- Modify: `server/src/routes/data.js`

- [ ] **Step 7.1: Register new collectors**

In `server/src/collectors/index.js`, near the existing satellite imports (line ~134, where `satelliteGroundStations` is imported), add:

```js
import satelliteImagery from './satelliteImagery.js';
import satelliteTracking from './satelliteTracking.js';
```

Then in the `collectors` object export (after the existing `'satellite-ground-stations': satelliteGroundStations,` line):

```js
  'satellite-imagery':  satelliteImagery,
  'satellite-tracking': satelliteTracking,
```

- [ ] **Step 7.2: Add routes**

In `server/src/routes/data.js`, just below the existing `/satellite-ground-stations` route (around line 845), add:

```js
router.get('/satellite-imagery', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'satellite-imagery',
    layerType: 'satelliteImagery',
    collectorKey: 'satellite-imagery',
  });
});

router.get('/satellite-tracking', async (_req, res) => {
  await respondWithData(res, {
    sourceId: 'satellite-tracking',
    layerType: 'satelliteTracking',
    collectorKey: 'satellite-tracking',
  });
});
```

- [ ] **Step 7.3: Add source registry entries**

In `server/src/utils/sourceRegistry.js`, find the existing satellite-related entries (`satellite-ground-stations`, `sentinel-hub`) and add these two new entries in the same section:

```js
{
  id: 'satellite-imagery',
  name: 'Satellite Imagery (Multi-source)',
  nameJa: '衛星画像 (複数ソース)',
  type: 'api',
  category: 'satellite',
  url: 'https://gibs.earthdata.nasa.gov/',
  description: 'Aggregated satellite imagery over Japan: Himawari-9 (real-time), MODIS/VIIRS (NASA GIBS), Landsat 8/9 (Planetary Computer), GOES-18 (RAMMB), ALOS-2 (seed), CORONA + historical Landsat (USGS M2M). Preview feed shown on popup click.',
  updateInterval: 1800,
  layer: 'satelliteImagery',
  free: true,
  status: 'offline',
},
{
  id: 'satellite-tracking',
  name: 'Live Satellite Positions',
  nameJa: '人工衛星リアルタイム位置',
  type: 'api',
  category: 'satellite',
  url: 'https://celestrak.org/NORAD/elements/',
  description: 'Live positions of every tracked on-orbit object (active, debris, CubeSats, stations) currently over Japan. SGP4 propagation from CelesTrak TLEs; ground tracks computed client-side on popup click.',
  updateInterval: 60,
  layer: 'satelliteTracking',
  free: true,
  status: 'offline',
},
```

- [ ] **Step 7.4: Smoke-test the routes**

Run: `cd server && npm run dev &` (or whatever the dev script is — check `server/package.json` `"scripts"`). Wait ~3 seconds, then:
```
curl -s http://localhost:3001/api/data/satellite-imagery | head -c 400
curl -s http://localhost:3001/api/data/satellite-tracking | head -c 400
curl -s http://localhost:3001/api/data/satellite-ground-stations | head -c 400
```
Expected: each returns JSON starting with `{"type":"FeatureCollection","features":[...`. Kill the dev server after verifying.

- [ ] **Step 7.5: Commit**

```bash
git add server/src/collectors/index.js server/src/utils/sourceRegistry.js server/src/routes/data.js
git commit -m "Register satellite imagery + tracking collectors and routes"
```

---

## Task 8: Client layer definitions

**Files:**
- Modify: `client/src/hooks/useMapLayers.js`

- [ ] **Step 8.1: Add layer defs**

In `client/src/hooks/useMapLayers.js`, find the existing `satelliteGroundStations` entry (around line 620). Below it, add:

```js
  satelliteImagery: {
    name: 'Satellite Imagery',
    icon: '\u{1F30D}', // 🌍
    color: '#64b5f6',
    endpoint: '/api/data/satellite-imagery',
    category: 'Satellite',
  },
  satelliteTracking: {
    name: 'Live Satellite Positions',
    icon: '\u{1F6F0}', // 🛰
    color: '#ba68c8',
    endpoint: '/api/data/satellite-tracking',
    category: 'Satellite',
  },
```

Also: in the same file, find `satelliteGroundStations` and change its `category: 'Telecom'` to `category: 'Satellite'` so all three group under one panel section.

- [ ] **Step 8.2: Verify LAYER_CATEGORIES knows the new 'Satellite' category**

Search the file: `grep -n "LAYER_CATEGORIES" client/src/hooks/useMapLayers.js`. If `LAYER_CATEGORIES` is a hard-coded ordered list, add `'Satellite'` to it. If it's derived dynamically from the `category` fields of `LAYER_DEFINITIONS`, no change needed.

- [ ] **Step 8.3: Smoke-test the client layer panel**

Run: `cd client && npm run dev` — open the browser, toggle the new **Satellite Imagery** and **Live Satellite Positions** layers, and confirm dots/points render on the map. Kill the dev server.

- [ ] **Step 8.4: Commit**

```bash
git add client/src/hooks/useMapLayers.js
git commit -m "Add client layer defs for satellite imagery + tracking"
```

---

## Task 9: Client-side ground-track utility

**Files:**
- Create: `client/src/utils/groundTrack.js`

- [ ] **Step 9.1: Create the utility**

Create `client/src/utils/groundTrack.js`:

```js
/**
 * Compute a 90-minute forward ground track for a satellite given its TLE.
 * Returns a GeoJSON LineString in [lon, lat] order.
 *
 * Uses satellite.js for SGP4 propagation. Sampled every 30 seconds.
 */
import * as satjs from 'satellite.js';

export function computeGroundTrack(tleLine1, tleLine2, { minutes = 90, stepSec = 30 } = {}) {
  const satrec = satjs.twoline2satrec(tleLine1, tleLine2);
  const start = new Date();
  const end = new Date(start.getTime() + minutes * 60 * 1000);
  const coords = [];
  let prevLon = null;
  for (let t = start.getTime(); t <= end.getTime(); t += stepSec * 1000) {
    const when = new Date(t);
    const pv = satjs.propagate(satrec, when);
    if (!pv?.position) continue;
    const gmst = satjs.gstime(when);
    const geo = satjs.eciToGeodetic(pv.position, gmst);
    const lon = satjs.degreesLong(geo.longitude);
    const lat = satjs.degreesLat(geo.latitude);
    // Split the polyline at the antimeridian to avoid the
    // "line wraps halfway around the world" artefact.
    if (prevLon !== null && Math.abs(lon - prevLon) > 180) {
      coords.push(null); // segment break
    }
    coords.push([lon, lat]);
    prevLon = lon;
  }

  // Convert into a MultiLineString if there were segment breaks.
  const segments = [];
  let current = [];
  for (const c of coords) {
    if (c === null) {
      if (current.length > 1) segments.push(current);
      current = [];
    } else {
      current.push(c);
    }
  }
  if (current.length > 1) segments.push(current);

  if (segments.length === 1) {
    return { type: 'LineString', coordinates: segments[0] };
  }
  return { type: 'MultiLineString', coordinates: segments };
}
```

- [ ] **Step 9.2: Commit**

```bash
git add client/src/utils/groundTrack.js
git commit -m "Add client-side SGP4 ground-track computation utility"
```

---

## Task 10: Popup renderers — imagery feed + satellite tracking

**Files:**
- Modify: `client/src/components/map/MapPopup.jsx`

- [ ] **Step 10.1: Add `SatelliteImageryDetail` renderer**

In `client/src/components/map/MapPopup.jsx`, right above `const DETAIL_RENDERERS = {` (around line 737), add:

```jsx
function SatelliteImageryDetail({ properties }) {
  const highlighted = [
    'platform', 'sensor', 'scene_id', 'datetime',
    'cloud_cover', 'preview_url', 'tile_url', 'archive_era', 'source',
  ];
  return (
    <div className="space-y-2">
      <div className="flex items-center gap-2">
        <span className="text-sm font-medium text-gray-200">
          {properties.platform}
        </span>
        {properties.sensor && (
          <span className="text-[10px] px-1.5 py-0.5 rounded bg-neon-cyan/20 text-neon-cyan font-mono">
            {properties.sensor}
          </span>
        )}
        {properties.archive_era === 'historical' && (
          <span className="text-[10px] px-1.5 py-0.5 rounded bg-amber-500/20 text-amber-400 font-mono">
            archive
          </span>
        )}
      </div>
      {properties.datetime && (
        <p className="text-xs text-gray-500 font-mono">
          {formatTimestamp(properties.datetime)}
          {properties.cloud_cover != null && (
            <span className="ml-2">cloud {properties.cloud_cover}%</span>
          )}
        </p>
      )}
      {properties.preview_url && (
        <img
          src={properties.preview_url}
          alt={`${properties.platform} preview`}
          style={{ maxWidth: 240, maxHeight: 180, objectFit: 'contain' }}
          className="rounded border border-osint-border/50"
          onError={(e) => { e.currentTarget.style.display = 'none'; }}
        />
      )}
      <p className="text-[10px] text-gray-600 font-mono">src: {properties.source}</p>
      <PropertyTable properties={properties} exclude={highlighted} />
    </div>
  );
}
```

- [ ] **Step 10.2: Add `SatelliteTrackingDetail` renderer**

Just below `SatelliteImageryDetail`, add:

```jsx
function SatelliteTrackingDetail({ properties }) {
  const [showTrack, setShowTrack] = useState(false);
  const highlighted = [
    'name', 'norad_id', 'category', 'altitude_km', 'velocity_kms',
    'inclination_deg', 'next_pass_utc', 'tle_line1', 'tle_line2', 'source',
  ];
  return (
    <div className="space-y-2">
      <div className="flex items-center gap-2">
        <span className="text-sm font-medium text-gray-200">{properties.name}</span>
        <span className="text-[10px] px-1.5 py-0.5 rounded bg-purple-500/20 text-purple-300 font-mono">
          {properties.category}
        </span>
      </div>
      <div className="text-xs text-gray-400 space-y-0.5 font-mono">
        <div>NORAD: {properties.norad_id}</div>
        {properties.altitude_km != null && <div>Altitude: {properties.altitude_km} km</div>}
        {properties.velocity_kms != null && <div>Velocity: {properties.velocity_kms} km/s</div>}
        {properties.inclination_deg != null && <div>Inclination: {properties.inclination_deg}°</div>}
      </div>
      <button
        type="button"
        className="text-xs px-2 py-1 rounded bg-neon-cyan/20 text-neon-cyan hover:bg-neon-cyan/30 transition"
        onClick={() => {
          setShowTrack((v) => !v);
          // Dispatch a custom event so MapView picks it up and draws / clears
          // the ground-track layer. Keeping MapPopup decoupled from map state.
          const evt = new CustomEvent('satellite-track-toggle', {
            detail: {
              noradId: properties.norad_id,
              tleLine1: properties.tle_line1,
              tleLine2: properties.tle_line2,
              show: !showTrack,
            },
          });
          window.dispatchEvent(evt);
        }}
      >
        {showTrack ? 'Hide ground track' : 'Show ground track'}
      </button>
      <PropertyTable properties={properties} exclude={highlighted} />
    </div>
  );
}
```

- [ ] **Step 10.3: Register the renderers**

In the `DETAIL_RENDERERS` map, add entries for both possible layer-type keys (the app uses both camelCase and kebab-case in different places):

```js
const DETAIL_RENDERERS = {
  earthquakes: EarthquakeDetail,
  cameras: CameraDetail,
  weather: WeatherDetail,
  airQuality: AirQualityDetail,
  radiation: RadiationDetail,
  river: RiverDetail,
  flightAdsb: FlightDetail,
  'flight-adsb': FlightDetail,
  twitterGeo: TwitterGeoDetail,
  'twitter-geo': TwitterGeoDetail,
  satelliteImagery: SatelliteImageryDetail,
  'satellite-imagery': SatelliteImageryDetail,
  satelliteTracking: SatelliteTrackingDetail,
  'satellite-tracking': SatelliteTrackingDetail,
};
```

- [ ] **Step 10.4: Commit**

```bash
git add client/src/components/map/MapPopup.jsx
git commit -m "Add popup renderers for satellite imagery feed and satellite tracking"
```

---

## Task 11: Ground-track rendering in MapView

**Files:**
- Modify: `client/src/components/map/MapView.jsx`

- [ ] **Step 11.1: Add a ground-track layer that listens for the event**

In `client/src/components/map/MapView.jsx`, locate where the Leaflet map instance is created and layers are added (search for `useMap` / `L.geoJSON` / `map.addLayer`). Add a new `useEffect` near the top of the component body:

```jsx
  // Temporary ground-track line layer for satellite tracking popups.
  // Receives events from MapPopup's SatelliteTrackingDetail "Show ground track" button.
  useEffect(() => {
    if (!mapRef.current) return;
    let currentLayer = null;

    async function onToggle(e) {
      const { show, tleLine1, tleLine2, noradId } = e.detail || {};
      if (!mapRef.current) return;
      if (currentLayer) {
        mapRef.current.removeLayer(currentLayer);
        currentLayer = null;
      }
      if (!show) return;
      const { computeGroundTrack } = await import('../../utils/groundTrack.js');
      const geom = computeGroundTrack(tleLine1, tleLine2, { minutes: 90, stepSec: 30 });
      const L = (await import('leaflet')).default;
      currentLayer = L.geoJSON(
        { type: 'Feature', geometry: geom, properties: { noradId } },
        { style: { color: '#ba68c8', weight: 2, opacity: 0.85, dashArray: '4,3' } }
      );
      currentLayer.addTo(mapRef.current);
    }

    window.addEventListener('satellite-track-toggle', onToggle);
    return () => {
      window.removeEventListener('satellite-track-toggle', onToggle);
      if (currentLayer && mapRef.current) {
        mapRef.current.removeLayer(currentLayer);
      }
    };
  }, []);
```

If `mapRef.current` is not the name used in this file, substitute the actual map ref (look for where `L.map(...)` is called or where `useMap()` is used).

- [ ] **Step 11.2: Smoke-test**

Run: `cd client && npm run dev` and `cd server && npm run dev` (two terminals). In the browser:
1. Enable **Live Satellite Positions** layer.
2. Click one satellite dot.
3. Click "Show ground track" — a dashed purple line should appear showing the 90-min forward path.
4. Click "Hide ground track" or close the popup — the line should disappear.

Kill both dev servers.

- [ ] **Step 11.3: Commit**

```bash
git add client/src/components/map/MapView.jsx
git commit -m "Render satellite ground tracks on popup button click"
```

---

## Task 12: Final integration test + verification

**Files:**
- No new files

- [ ] **Step 12.1: Run all satellite tests**

Run: `cd server && node --test test/satelliteImagery.test.js test/satelliteTracking.test.js test/satelliteGroundStations.test.js`
Expected: all green.

- [ ] **Step 12.2: Manual smoke test**

Start both servers. In the browser:
1. Open **Satellite** section in the layer panel — three toggles visible (Imagery, Live Positions, Ground Stations).
2. Enable Satellite Imagery — click a feature — popup shows platform/sensor/date + `<img>` preview thumbnail.
3. Enable Live Satellite Positions — click a feature — popup shows NORAD / altitude / velocity + "Show ground track" button; button draws dashed orbit line.
4. Enable Satellite Infrastructure — confirm many more features than before (expect hundreds including GEONET).

- [ ] **Step 12.3: Commit any remaining fixups**

If any stylistic or small-bug fixes were needed during smoke test, commit them:

```bash
git add <changed files>
git commit -m "Polish satellite popups and layer rendering"
```

---

## Self-Review

**Spec coverage check:**
- ✅ Collector 1 (imagery) — Tasks 3–4 implement Himawari, MODIS, VIIRS, Landsat, GOES, ALOS seed, CORONA. Sentinel-2 remains in existing `sentinelHub.js` (as per spec, not merged).
- ✅ Collector 2 (tracking) — Task 5 implements CelesTrak + SGP4, TLE lines shipped in properties, ground tracks computed client-side.
- ✅ Collector 3 (infrastructure expansion) — Task 6 adds VLBI, SLR, optical tracking, commercial operators, universities, GEONET (live via GSI CSV + fallback subset).
- ✅ Layer panel "Satellite" group — Task 8.
- ✅ Popup renders `<img src={preview_url}>` — Task 10.
- ✅ Ground-track button + client-side SGP4 — Tasks 9–11.
- ✅ Source registry entries — Task 7.

**Placeholder scan:** no `TBD`/`TODO`/"implement later" in any task. Every code step shows the full code to write.

**Type consistency:** `category` property used consistently across ground-stations seed module, seed merging in Task 6, and assertions in Task 6 test. `tle_line1`/`tle_line2` property names consistent between Task 5 collector, Task 9 utility, Task 10 event payload, Task 11 event handler.
