# Persistent Satellite Ground Tracks + Sentinel-1 GRD Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Persistently render every tracked satellite's 90-min forward ground track as a thick, color-matched line on the map (replacing the click-triggered toggle), and add Sentinel-1 GRD grayscale-VV imagery as a new provider in the imagery collector (CDSE → Planetary Computer → Earth Search fallback).

**Architecture:**
- **Client:** new `satelliteColor(noradId)` helper + new `useSatelliteTracks` hook that derives a GeoJSON line FeatureCollection from the already-loaded `satelliteTracking` features. A dedicated MapLibre `satellite-tracks` source/layer renders them under the markers. The old `satellite-track-toggle` event handler and the "Show ground track" popup button are deleted.
- **Server:** a new `trySentinel1()` function is added to `server/src/collectors/satelliteImagery.js`, wired into the `providers` array. It tries CDSE OData → Planetary Computer STAC → Earth Search STAC in order, first non-empty wins. Returns GeoJSON features with `platform: sentinel-1a|sentinel-1c`, `polarization: VV`, and a grayscale tile URL.

**Tech Stack:** React 18, MapLibre GL JS, `satellite.js` (SGP4), Node 20+, ESM, Vitest (client), fetch (server).

**Spec:** `docs/superpowers/specs/2026-04-21-satellite-tracks-sentinel1-design.md`

---

## File Structure

**New files:**
- `client/src/utils/satelliteColor.js` — hash-to-palette helper shared by marker and track layers.
- `client/src/hooks/useSatelliteTracks.js` — React hook that derives track FeatureCollection.
- `client/src/utils/__tests__/satelliteColor.test.js` — unit test for color determinism.
- `client/src/hooks/__tests__/useSatelliteTracks.test.jsx` — unit test for the hook.
- `server/src/collectors/__tests__/satelliteImagery.sentinel1.test.js` — integration test for Sentinel-1 provider chain.

**Modified files:**
- `client/src/components/map/MapView.jsx` — add `satellite-tracks` source+layer, render `satelliteTracking` markers with `satelliteColor`, delete old track-toggle handler (lines 4188–4230).
- `client/src/components/map/MapPopup.jsx` — strip the "Show ground track" button and unmount-cleanup effect from `SatelliteTrackingDetail` (lines 908–959).
- `server/src/collectors/satelliteImagery.js` — add `trySentinel1()` + entry in `providers` array.

---

## Task 1: Add deterministic color helper

**Files:**
- Create: `client/src/utils/satelliteColor.js`
- Test: `client/src/utils/__tests__/satelliteColor.test.js`

**Context:** Both the marker layer and the track layer must use the *same* color for a given satellite. Rather than baking color into the server response, derive it client-side from the NORAD ID. The palette is 24 visually distinct hues chosen to be distinguishable on a dark basemap.

- [ ] **Step 1: Write the failing test**

Create `client/src/utils/__tests__/satelliteColor.test.js`:

```js
import { describe, it, expect } from 'vitest';
import { satelliteColor } from '../satelliteColor.js';

describe('satelliteColor', () => {
  it('returns a hex color for a numeric NORAD id', () => {
    const c = satelliteColor(25544);
    expect(c).toMatch(/^#[0-9a-f]{6}$/i);
  });

  it('is deterministic — same id returns same color', () => {
    expect(satelliteColor(25544)).toBe(satelliteColor(25544));
    expect(satelliteColor('25544')).toBe(satelliteColor(25544));
  });

  it('distributes across the palette (different ids often get different colors)', () => {
    const colors = new Set();
    for (let i = 0; i < 100; i += 1) colors.add(satelliteColor(10000 + i));
    expect(colors.size).toBeGreaterThan(10);
  });

  it('falls back to a default color when id is null/undefined', () => {
    expect(satelliteColor(null)).toMatch(/^#[0-9a-f]{6}$/i);
    expect(satelliteColor(undefined)).toMatch(/^#[0-9a-f]{6}$/i);
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd client && npx vitest run src/utils/__tests__/satelliteColor.test.js`
Expected: FAIL — `Failed to load url ../satelliteColor.js`

- [ ] **Step 3: Write minimal implementation**

Create `client/src/utils/satelliteColor.js`:

```js
/**
 * Deterministic per-satellite color. Both the marker layer and the
 * ground-track line layer call this so a satellite's streak always
 * matches its dot on the map.
 *
 * The palette is 24 saturated hues that read well on the dark basemap.
 */
const PALETTE = [
  '#e53935', '#d81b60', '#8e24aa', '#5e35b1',
  '#3949ab', '#1e88e5', '#039be5', '#00acc1',
  '#00897b', '#43a047', '#7cb342', '#c0ca33',
  '#fdd835', '#ffb300', '#fb8c00', '#f4511e',
  '#6d4c41', '#546e7a', '#ef5350', '#ec407a',
  '#ab47bc', '#5c6bc0', '#26a69a', '#9ccc65',
];

const FALLBACK = '#ba68c8';

export function satelliteColor(noradId) {
  if (noradId === null || noradId === undefined || noradId === '') return FALLBACK;
  const n = typeof noradId === 'number' ? noradId : parseInt(String(noradId), 10);
  if (!Number.isFinite(n)) return FALLBACK;
  // FNV-1a-ish small hash, enough for palette distribution.
  let h = 0x811c9dc5;
  const s = String(n);
  for (let i = 0; i < s.length; i += 1) {
    h ^= s.charCodeAt(i);
    h = (h * 0x01000193) >>> 0;
  }
  return PALETTE[h % PALETTE.length];
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd client && npx vitest run src/utils/__tests__/satelliteColor.test.js`
Expected: PASS — 4 tests.

- [ ] **Step 5: Commit**

```bash
git add client/src/utils/satelliteColor.js client/src/utils/__tests__/satelliteColor.test.js
git commit -m "feat: add satelliteColor() hash-to-palette helper"
```

---

## Task 2: Write `useSatelliteTracks` hook (failing test first)

**Files:**
- Create: `client/src/hooks/useSatelliteTracks.js`
- Test: `client/src/hooks/__tests__/useSatelliteTracks.test.jsx`

**Context:** The hook takes the `satelliteTracking` FeatureCollection (as already loaded by `useMapLayers`) and returns a derived FeatureCollection of line features. Each feature has `{ satellite_id, satellite_name, color }` in properties. Lines are computed via the existing `computeGroundTrack()` helper at `client/src/utils/groundTrack.js` and refreshed every 60 s.

The hook must be tolerant of features that lack TLE lines — they are silently skipped (the server sometimes returns satellites without TLE during a partial outage).

- [ ] **Step 1: Write the failing test**

Create `client/src/hooks/__tests__/useSatelliteTracks.test.jsx`:

```jsx
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { renderHook } from '@testing-library/react';
import { useSatelliteTracks } from '../useSatelliteTracks.js';

// A real TLE (ISS, 2024-ish) — exercises the SGP4 path end-to-end.
const ISS_TLE_1 = '1 25544U 98067A   24001.50000000  .00016717  00000-0  10270-3 0  9007';
const ISS_TLE_2 = '2 25544  51.6400 208.9163 0006317  69.6530  25.7298 15.50377579000000';

function makeFc(features) {
  return { type: 'FeatureCollection', features };
}

describe('useSatelliteTracks', () => {
  beforeEach(() => { vi.useFakeTimers(); });
  afterEach(() => { vi.useRealTimers(); });

  it('returns a FeatureCollection of line features, one per satellite with TLE', () => {
    const input = makeFc([
      {
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [0, 0] },
        properties: {
          norad_id: 25544, name: 'ISS (ZARYA)',
          tle_line1: ISS_TLE_1, tle_line2: ISS_TLE_2,
        },
      },
    ]);
    const { result } = renderHook(() => useSatelliteTracks(input));
    expect(result.current.type).toBe('FeatureCollection');
    expect(result.current.features).toHaveLength(1);
    const f = result.current.features[0];
    expect(['LineString', 'MultiLineString']).toContain(f.geometry.type);
    expect(f.properties.satellite_id).toBe(25544);
    expect(f.properties.satellite_name).toBe('ISS (ZARYA)');
    expect(f.properties.color).toMatch(/^#[0-9a-f]{6}$/i);
  });

  it('skips features without TLE', () => {
    const input = makeFc([
      {
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [0, 0] },
        properties: { norad_id: 99999, name: 'No TLE sat' },
      },
    ]);
    const { result } = renderHook(() => useSatelliteTracks(input));
    expect(result.current.features).toHaveLength(0);
  });

  it('returns an empty FeatureCollection when input is null/empty', () => {
    const { result: r1 } = renderHook(() => useSatelliteTracks(null));
    expect(r1.current.features).toHaveLength(0);
    const { result: r2 } = renderHook(() => useSatelliteTracks(makeFc([])));
    expect(r2.current.features).toHaveLength(0);
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd client && npx vitest run src/hooks/__tests__/useSatelliteTracks.test.jsx`
Expected: FAIL — module not found.

- [ ] **Step 3: Write minimal implementation**

Create `client/src/hooks/useSatelliteTracks.js`:

```js
import { useEffect, useMemo, useState } from 'react';
import { computeGroundTrack } from '../utils/groundTrack.js';
import { satelliteColor } from '../utils/satelliteColor.js';

const REFRESH_MS = 60 * 1000;

function buildTracks(fc) {
  if (!fc || !Array.isArray(fc.features)) return [];
  const out = [];
  for (const feat of fc.features) {
    const p = feat?.properties || {};
    if (!p.tle_line1 || !p.tle_line2) continue;
    try {
      const geom = computeGroundTrack(p.tle_line1, p.tle_line2, { minutes: 90, stepSec: 30 });
      if (!geom) continue;
      out.push({
        type: 'Feature',
        geometry: geom,
        properties: {
          satellite_id: p.norad_id,
          satellite_name: p.name,
          color: satelliteColor(p.norad_id),
        },
      });
    } catch { /* skip broken TLE */ }
  }
  return out;
}

export function useSatelliteTracks(satelliteFc) {
  const initial = useMemo(() => buildTracks(satelliteFc), [satelliteFc]);
  const [features, setFeatures] = useState(initial);

  // Recompute immediately when input changes.
  useEffect(() => { setFeatures(buildTracks(satelliteFc)); }, [satelliteFc]);

  // Refresh every 60 s so the forward-track stays current.
  useEffect(() => {
    if (!satelliteFc || !satelliteFc.features?.length) return undefined;
    const id = setInterval(() => {
      setFeatures(buildTracks(satelliteFc));
    }, REFRESH_MS);
    return () => clearInterval(id);
  }, [satelliteFc]);

  return useMemo(() => ({ type: 'FeatureCollection', features }), [features]);
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd client && npx vitest run src/hooks/__tests__/useSatelliteTracks.test.jsx`
Expected: PASS — 3 tests.

- [ ] **Step 5: Commit**

```bash
git add client/src/hooks/useSatelliteTracks.js client/src/hooks/__tests__/useSatelliteTracks.test.jsx
git commit -m "feat: useSatelliteTracks hook derives forward ground tracks"
```

---

## Task 3: Render persistent tracks layer in `MapView.jsx`

**Files:**
- Modify: `client/src/components/map/MapView.jsx`

**Context:** Wire `useSatelliteTracks` into MapView. Add a `satellite-tracks` GeoJSON source and a MapLibre `line` layer that pulls `line-color` from the feature property. The layer is added when the satelliteTracking layer is visible and removed when it is not. The effect must re-run when the derived FeatureCollection changes so the polylines refresh every 60 s.

The layer must be inserted below the satellite icon/circle layer so markers stay visually on top. MapView already loads `satelliteTracking` GeoJSON through `useMapLayers`; read it from `layers.satelliteTracking.data` (same shape as every other layer).

- [ ] **Step 1: Add the import and hook call at the top of the component**

Find the existing imports at the top of `client/src/components/map/MapView.jsx` and add:

```js
import { useSatelliteTracks } from '../../hooks/useSatelliteTracks.js';
```

Inside the `MapView` component function body, near the other hooks, add:

```js
const satelliteTrackFc = useSatelliteTracks(
  layers?.satelliteTracking?.visible ? layers.satelliteTracking.data : null
);
```

- [ ] **Step 2: Add the persistent tracks effect — new block, place it right before the old track-toggle effect (currently at line ~4188)**

```js
// Persistent per-satellite ground tracks. One 90-min forward orbit per
// tracked satellite, colour-matched to its marker via satelliteColor().
// Sits below the icon layer so markers stay on top.
useEffect(() => {
  const map = mapRef.current;
  if (!map || !mapReady) return undefined;
  const SRC = 'satellite-tracks';
  const LYR = 'satellite-tracks-line';

  function remove() {
    if (!map.style) return;
    if (map.getLayer(LYR)) map.removeLayer(LYR);
    if (map.getSource(SRC)) map.removeSource(SRC);
  }

  const empty = !satelliteTrackFc || !satelliteTrackFc.features?.length;
  if (empty) { remove(); return undefined; }

  if (!map.getSource(SRC)) {
    map.addSource(SRC, { type: 'geojson', data: satelliteTrackFc });
    map.addLayer({
      id: LYR,
      type: 'line',
      source: SRC,
      paint: {
        'line-color': ['get', 'color'],
        'line-width': 3,
        'line-opacity': 0.85,
      },
    });
  } else {
    const src = map.getSource(SRC);
    if (src && src.type === 'geojson') src.setData(satelliteTrackFc);
  }

  return () => { /* layer persists across re-renders; removed when data empties */ };
}, [mapReady, satelliteTrackFc]);
```

- [ ] **Step 3: Delete the old click-triggered track handler**

Open `client/src/components/map/MapView.jsx` and remove the entire block currently at lines 4188–4230 (the `useEffect` whose comment starts with "Temporary ground-track line layer for satellite tracking popups" and ends with the `return () => { window.removeEventListener('satellite-track-toggle'…` block).

The next block after deletion should be the "Satellite imagery 'bake on map'" effect (currently line 4232+).

- [ ] **Step 4: Run the client build + lint**

Run: `cd client && npm run build`
Expected: build succeeds with no new warnings.

Run: `cd client && npx vitest run`
Expected: all tests pass (previous suite plus the two new ones from Tasks 1–2).

- [ ] **Step 5: Commit**

```bash
git add client/src/components/map/MapView.jsx
git commit -m "feat: render persistent satellite ground tracks layer"
```

---

## Task 4: Strip the click-triggered track button from the popup

**Files:**
- Modify: `client/src/components/map/MapPopup.jsx`

**Context:** With persistent tracks in place, the per-satellite "Show ground track" button and its unmount-cleanup effect are dead weight. Delete them, keeping everything else in `SatelliteTrackingDetail` (name, category badge, orbital properties table).

- [ ] **Step 1: Replace the whole `SatelliteTrackingDetail` component**

In `client/src/components/map/MapPopup.jsx`, locate the function starting at line 908 (`function SatelliteTrackingDetail({ properties }) {`) and ending at line 959 (the closing `}`) and replace the entire function with:

```jsx
function SatelliteTrackingDetail({ properties }) {
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
      <PropertyTable properties={properties} exclude={highlighted} />
    </div>
  );
}
```

- [ ] **Step 2: Remove the now-unused `useState` / `useEffect` imports if they were only used here**

Check the top of `MapPopup.jsx`. If `useState` and `useEffect` are still used by other components in this file (they almost certainly are — `BusStopDetail`, `SatelliteImageryDetail`, etc.), leave the import alone. If they are unused, remove them from the import list.

- [ ] **Step 3: Run the client build**

Run: `cd client && npm run build`
Expected: build succeeds, no warnings about unused imports.

- [ ] **Step 4: Commit**

```bash
git add client/src/components/map/MapPopup.jsx
git commit -m "refactor: remove click-triggered ground-track button"
```

---

## Task 5: Use `satelliteColor` for the tracking marker layer

**Files:**
- Modify: `client/src/components/map/MapView.jsx`

**Context:** The `satelliteTracking` layer currently falls through to the generic icon renderer (fixed purple `#ba68c8`). Swap the marker's color to come from the NORAD id via the same helper the track line uses, so the dot and its streak always match.

`satelliteTracking` uses the icon-substituted symbol layer. The color for symbol-rendered icons is controlled by `icon-color` when the icon is a SDF image, but the project's icon pipeline uses full-color PNGs, so a tinted icon isn't available. Instead, switch `satelliteTracking` to a custom `circle` layer (like `satelliteGroundStations` at line 2946) so we can style the circle directly.

- [ ] **Step 1: Add a new switch case for `satelliteTracking` in the renderer**

In `client/src/components/map/MapView.jsx`, find the `addLayerToMapInner` function (around line 446). Locate the `case 'satelliteGroundStations':` block. Immediately after its `break;` statement (around line 2983), add a new case that renders the tracking layer as a coloured circle. The key change vs. ground stations: `circle-color` comes from the NORAD-id-based palette using a MapLibre `match` expression against the feature property.

Because there can be hundreds of satellites and we don't want a 300-branch `match` expression, compute a per-feature `_color` property at hook time instead, and use `['get', '_color']` at render time. To do that, amend `useMapLayers` output — see Step 2. The renderer block:

```jsx
case 'satelliteTracking':
  map.addLayer({
    id: mainLayerId,
    type: 'circle',
    source: sourceId,
    paint: {
      'circle-radius': 6,
      'circle-color': ['coalesce', ['get', '_color'], '#ba68c8'],
      'circle-opacity': opacity * 0.9,
      'circle-stroke-width': 2,
      'circle-stroke-color': '#ffffff',
      'circle-stroke-opacity': opacity * 0.7,
    },
  });
  break;
```

Place the `case` block inside the existing `switch (layerId) { … }` in `addLayerToMapInner`, next to `satelliteGroundStations`.

- [ ] **Step 2: Stamp `_color` onto each tracking feature before it hits the layer**

Find the `useSatelliteTracks` hook call added in Task 3. Right above it, transform the tracking FeatureCollection so every point feature carries its own colour. Add a new `useMemo` just after the hook call imports:

```js
const coloredSatelliteTrackingFc = useMemo(() => {
  const fc = layers?.satelliteTracking?.data;
  if (!fc || !Array.isArray(fc.features)) return fc;
  return {
    ...fc,
    features: fc.features.map((f) => ({
      ...f,
      properties: {
        ...(f.properties || {}),
        _color: satelliteColor(f.properties?.norad_id),
      },
    })),
  };
}, [layers?.satelliteTracking?.data]);
```

Add the matching import at the top of the file:

```js
import { satelliteColor } from '../../utils/satelliteColor.js';
```

Then find the place where `layers.satelliteTracking.data` is passed into `addLayerToMap` (grep for `satelliteTracking`) and substitute `coloredSatelliteTrackingFc` there — the exact line depends on whether MapView routes through a generic loop or per-layer call. If it's a generic loop, wrap the layers object:

```js
const effectiveLayers = useMemo(() => {
  if (!layers?.satelliteTracking) return layers;
  return {
    ...layers,
    satelliteTracking: {
      ...layers.satelliteTracking,
      data: coloredSatelliteTrackingFc,
    },
  };
}, [layers, coloredSatelliteTrackingFc]);
```

and use `effectiveLayers` in place of `layers` for the rendering pass. Also pass `effectiveLayers.satelliteTracking.data` into `useSatelliteTracks` so both layers share the same colour source of truth:

```js
const satelliteTrackFc = useSatelliteTracks(
  effectiveLayers?.satelliteTracking?.visible ? effectiveLayers.satelliteTracking.data : null
);
```

- [ ] **Step 3: Run the client build**

Run: `cd client && npm run build`
Expected: build succeeds.

- [ ] **Step 4: Manual smoke — start dev server**

Run: `cd client && npm run dev`

Open the map, turn on "Live Satellite Positions". Expected: dots are now varied colours (red/blue/green/orange/etc. per satellite), and each has a same-coloured forward-track line extending ~1/4 of the way around the globe. Click a dot — popup opens with the satellite name and orbital details, no "Show ground track" button.

- [ ] **Step 5: Commit**

```bash
git add client/src/components/map/MapView.jsx
git commit -m "feat: colour-match satellite markers to their ground tracks"
```

---

## Task 6: Add Sentinel-1 GRD provider (CDSE)

**Files:**
- Modify: `server/src/collectors/satelliteImagery.js`
- Test: `server/src/collectors/__tests__/satelliteImagery.sentinel1.test.js`

**Context:** Sentinel-1 is SAR, not optical. We emit grayscale VV-polarization tiles. CDSE is the authoritative source (product truth) but requires nothing more than an unauthenticated OData query for *listing* — the `preview_url` returned by CDSE is public without tokens; the authenticated pieces (token, tile renderer) are only needed for product download, which we don't do here. We ONLY list scenes, same pattern as `s2TryCdseOData()` at line 450.

- [ ] **Step 1: Write the failing test — listing contract**

Create `server/src/collectors/__tests__/satelliteImagery.sentinel1.test.js`:

```js
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';

describe('trySentinel1 — CDSE OData', () => {
  const origFetch = globalThis.fetch;
  beforeEach(() => { vi.resetModules(); });
  afterEach(() => { globalThis.fetch = origFetch; });

  it('maps CDSE OData Products response to GRD features', async () => {
    globalThis.fetch = vi.fn().mockImplementation((url) => {
      if (String(url).includes('catalogue.dataspace.copernicus.eu/odata')) {
        return Promise.resolve({
          ok: true,
          status: 200,
          json: () => Promise.resolve({
            value: [{
              Id: 'abc-123',
              Name: 'S1A_IW_GRDH_1SDV_20260419T093000_20260419T093025_000000_000000_AAAA.SAFE',
              ContentDate: { Start: '2026-04-19T09:30:00Z', End: '2026-04-19T09:30:25Z' },
              GeoFootprint: {
                type: 'Polygon',
                coordinates: [[[139, 35], [140, 35], [140, 36], [139, 36], [139, 35]]],
              },
            }],
          }),
        });
      }
      return Promise.resolve({ ok: false, status: 404, json: () => Promise.resolve({}) });
    });
    const { trySentinel1 } = await import('../satelliteImagery.js');
    const features = await trySentinel1();
    expect(features).not.toBeNull();
    expect(features.length).toBe(1);
    const f = features[0];
    expect(f.type).toBe('Feature');
    expect(f.properties.platform).toBe('sentinel-1a');
    expect(f.properties.product_type).toBe('GRD');
    expect(f.properties.polarization).toBe('VV');
    expect(f.properties.source).toBe('cdse_odata');
    expect(f.properties.scene_id).toContain('S1A_IW_GRDH');
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd server && npx vitest run src/collectors/__tests__/satelliteImagery.sentinel1.test.js`
Expected: FAIL — `trySentinel1 is not a function`.

- [ ] **Step 3: Add `trySentinel1` to the collector — CDSE first**

Open `server/src/collectors/satelliteImagery.js`. Right after the `trySentinel2` function (ends around line 500), add this new section and exported function:

```js
// ── 9. Sentinel-1 GRD (multi-source, first-wins internal fallback) ───────
// Chain: CDSE OData → Planetary Computer STAC → Earth Search (AWS).
// Grayscale VV polarization only. Keeps parity with the Sentinel-2 pattern
// above but the 3 sub-providers are in the explicit order the user picked:
// CDSE authoritative first.
const S1_SCENE_LIMIT = 40;
const S1_WINDOW_DAYS = 14;

function s1IsoWindow(days = S1_WINDOW_DAYS) {
  const to = new Date();
  const from = new Date(Date.now() - days * 86400e3);
  return { from: from.toISOString(), to: to.toISOString() };
}

function s1PlatformFromName(name) {
  const up = String(name || '').toUpperCase();
  if (up.startsWith('S1A')) return 'sentinel-1a';
  if (up.startsWith('S1B')) return 'sentinel-1b';
  if (up.startsWith('S1C')) return 'sentinel-1c';
  return 'sentinel-1';
}

function s1CentroidFromGeom(geom) {
  // Re-use s2's centroid helper shape; s2CentroidFromGeom is in scope.
  return s2CentroidFromGeom(geom);
}

async function s1TryCdseOData() {
  const { from, to } = s1IsoWindow();
  const [w, s, e, n] = JAPAN_BBOX;
  const polygon = `POLYGON((${w} ${s},${e} ${s},${e} ${n},${w} ${n},${w} ${s}))`;
  const filter = [
    `Collection/Name eq 'SENTINEL-1'`,
    `OData.CSC.Intersects(area=geography'SRID=4326;${polygon}')`,
    `ContentDate/Start ge ${from}`,
    `ContentDate/Start le ${to}`,
    `contains(Name,'GRD')`,
  ].join(' and ');
  const url = `https://catalogue.dataspace.copernicus.eu/odata/v1/Products?$filter=${encodeURIComponent(filter)}&$top=${S1_SCENE_LIMIT}&$orderby=ContentDate/Start desc`;
  const data = await fetchJson(url, { headers: { Accept: 'application/json' } });
  const items = data?.value || [];
  if (!items.length) return null;
  return items.map((it, i) => {
    const geom = it.GeoFootprint || null;
    const [cx, cy] = s1CentroidFromGeom(geom);
    return {
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [cx, cy] },
      properties: {
        id: `IMG_S1_${it.Id || i}`,
        platform: s1PlatformFromName(it.Name),
        sensor: 'c-sar',
        product_type: 'GRD',
        polarization: 'VV',
        scene_id: it.Name,
        datetime: it.ContentDate?.Start,
        preview_url: `https://catalogue.dataspace.copernicus.eu/odata/v1/Products(${it.Id})/$value`,
        tile_url: null, // set in the PC branch; CDSE does not expose a public XYZ tiler
        bbox_geom: geom,
        archive_era: 'real-time',
        source: 'cdse_odata',
        country: 'JP',
      },
    };
  });
}

export async function trySentinel1() {
  const chain = [s1TryCdseOData];
  for (const fn of chain) {
    try {
      const r = await fn();
      if (r && r.length) return r;
    } catch { /* try next */ }
  }
  return null;
}
```

Note: this task only wires the CDSE branch. Planetary Computer and Earth Search are added in the next two tasks.

- [ ] **Step 4: Run test to verify it passes**

Run: `cd server && npx vitest run src/collectors/__tests__/satelliteImagery.sentinel1.test.js`
Expected: PASS — 1 test.

- [ ] **Step 5: Commit**

```bash
git add server/src/collectors/satelliteImagery.js server/src/collectors/__tests__/satelliteImagery.sentinel1.test.js
git commit -m "feat: add Sentinel-1 GRD CDSE provider"
```

---

## Task 7: Sentinel-1 Planetary Computer fallback

**Files:**
- Modify: `server/src/collectors/satelliteImagery.js`
- Modify: `server/src/collectors/__tests__/satelliteImagery.sentinel1.test.js`

**Context:** When CDSE is down or empty, fall back to Microsoft Planetary Computer's `sentinel-1-grd` STAC collection. PC exposes a public tile endpoint that renders a VV grayscale PNG — embed it as `tile_url`.

- [ ] **Step 1: Extend the test — PC branch fires when CDSE returns empty**

Append to `server/src/collectors/__tests__/satelliteImagery.sentinel1.test.js`:

```js
describe('trySentinel1 — Planetary Computer fallback', () => {
  const origFetch = globalThis.fetch;
  beforeEach(() => { vi.resetModules(); });
  afterEach(() => { globalThis.fetch = origFetch; });

  it('falls back to Planetary Computer STAC when CDSE is empty', async () => {
    globalThis.fetch = vi.fn().mockImplementation((url) => {
      const u = String(url);
      if (u.includes('catalogue.dataspace.copernicus.eu')) {
        return Promise.resolve({ ok: true, status: 200, json: () => Promise.resolve({ value: [] }) });
      }
      if (u.includes('planetarycomputer.microsoft.com')) {
        return Promise.resolve({
          ok: true,
          status: 200,
          json: () => Promise.resolve({
            features: [{
              id: 'S1A_IW_GRDH_PC_XYZ',
              geometry: { type: 'Polygon', coordinates: [[[139, 35], [140, 35], [140, 36], [139, 36], [139, 35]]] },
              properties: {
                platform: 'sentinel-1a',
                datetime: '2026-04-19T09:30:00Z',
              },
            }],
          }),
        });
      }
      return Promise.resolve({ ok: false, status: 404, json: () => Promise.resolve({}) });
    });
    const { trySentinel1 } = await import('../satelliteImagery.js');
    const features = await trySentinel1();
    expect(features.length).toBe(1);
    expect(features[0].properties.source).toBe('planetary_computer_s1');
    expect(features[0].properties.tile_url).toContain('planetarycomputer.microsoft.com');
    expect(features[0].properties.tile_url).toContain('assets=vv');
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd server && npx vitest run src/collectors/__tests__/satelliteImagery.sentinel1.test.js`
Expected: the new test FAILS (first describe still passes).

- [ ] **Step 3: Add the PC provider and wire it into the chain**

In `server/src/collectors/satelliteImagery.js`, add before `export async function trySentinel1`:

```js
async function s1TryPlanetaryComputer() {
  const { from, to } = s1IsoWindow();
  const body = {
    bbox: JAPAN_BBOX,
    datetime: `${from}/${to}`,
    collections: ['sentinel-1-grd'],
    limit: S1_SCENE_LIMIT,
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
    const [cx, cy] = s1CentroidFromGeom(geom);
    const tile = `https://planetarycomputer.microsoft.com/api/data/v1/item/tiles/WebMercatorQuad/{z}/{x}/{y}?collection=sentinel-1-grd&item=${encodeURIComponent(f.id)}&assets=vv&rescale=-30,0`;
    return {
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [cx, cy] },
      properties: {
        id: `IMG_S1_${f.id || i}`,
        platform: s1PlatformFromName(f.properties?.platform || f.id),
        sensor: 'c-sar',
        product_type: 'GRD',
        polarization: 'VV',
        scene_id: f.id,
        datetime: f.properties?.datetime,
        preview_url: f.assets?.thumbnail?.href || f.assets?.rendered_preview?.href || null,
        tile_url: tile,
        bbox_geom: geom,
        archive_era: 'real-time',
        source: 'planetary_computer_s1',
        country: 'JP',
      },
    };
  });
}
```

Update the chain inside `trySentinel1`:

```js
const chain = [s1TryCdseOData, s1TryPlanetaryComputer];
```

- [ ] **Step 4: Run tests**

Run: `cd server && npx vitest run src/collectors/__tests__/satelliteImagery.sentinel1.test.js`
Expected: both tests PASS.

- [ ] **Step 5: Commit**

```bash
git add server/src/collectors/satelliteImagery.js server/src/collectors/__tests__/satelliteImagery.sentinel1.test.js
git commit -m "feat: Sentinel-1 GRD Planetary Computer fallback"
```

---

## Task 8: Sentinel-1 Earth Search fallback

**Files:**
- Modify: `server/src/collectors/satelliteImagery.js`
- Modify: `server/src/collectors/__tests__/satelliteImagery.sentinel1.test.js`

**Context:** Final fallback. Element84's `sentinel-1-grd` STAC collection. No auth. Emits a `vv` COG asset URL that the client's "bake feed on map" path uses as a raster source.

- [ ] **Step 1: Extend the test — Earth Search branch fires when first two fail**

Append to the existing test file:

```js
describe('trySentinel1 — Earth Search fallback', () => {
  const origFetch = globalThis.fetch;
  beforeEach(() => { vi.resetModules(); });
  afterEach(() => { globalThis.fetch = origFetch; });

  it('falls back to Earth Search when CDSE and Planetary Computer are empty', async () => {
    globalThis.fetch = vi.fn().mockImplementation((url) => {
      const u = String(url);
      if (u.includes('catalogue.dataspace.copernicus.eu')) {
        return Promise.resolve({ ok: true, status: 200, json: () => Promise.resolve({ value: [] }) });
      }
      if (u.includes('planetarycomputer.microsoft.com')) {
        return Promise.resolve({ ok: true, status: 200, json: () => Promise.resolve({ features: [] }) });
      }
      if (u.includes('earth-search.aws.element84.com')) {
        return Promise.resolve({
          ok: true,
          status: 200,
          json: () => Promise.resolve({
            features: [{
              id: 'S1C_IW_GRDH_ES_999',
              geometry: { type: 'Polygon', coordinates: [[[139, 35], [140, 35], [140, 36], [139, 36], [139, 35]]] },
              properties: { platform: 'sentinel-1c', datetime: '2026-04-20T09:30:00Z' },
              assets: { vv: { href: 'https://example/aws/s1c.tif' } },
            }],
          }),
        });
      }
      return Promise.resolve({ ok: false, status: 404, json: () => Promise.resolve({}) });
    });
    const { trySentinel1 } = await import('../satelliteImagery.js');
    const features = await trySentinel1();
    expect(features.length).toBe(1);
    expect(features[0].properties.source).toBe('earth_search_s1');
    expect(features[0].properties.platform).toBe('sentinel-1c');
    expect(features[0].properties.tile_url).toBe('https://example/aws/s1c.tif');
  });
});
```

- [ ] **Step 2: Run test — expect Earth Search test to fail**

Run: `cd server && npx vitest run src/collectors/__tests__/satelliteImagery.sentinel1.test.js`
Expected: Earth Search test FAILS, first two PASS.

- [ ] **Step 3: Add the Earth Search provider**

In `server/src/collectors/satelliteImagery.js`, add before `export async function trySentinel1`:

```js
async function s1TryEarthSearch() {
  const { from, to } = s1IsoWindow();
  const body = {
    bbox: JAPAN_BBOX,
    datetime: `${from}/${to}`,
    collections: ['sentinel-1-grd'],
    limit: S1_SCENE_LIMIT,
  };
  const data = await fetchJson(
    'https://earth-search.aws.element84.com/v1/search',
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
    const [cx, cy] = s1CentroidFromGeom(geom);
    return {
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [cx, cy] },
      properties: {
        id: `IMG_S1_${f.id || i}`,
        platform: s1PlatformFromName(f.properties?.platform || f.id),
        sensor: 'c-sar',
        product_type: 'GRD',
        polarization: 'VV',
        scene_id: f.id,
        datetime: f.properties?.datetime,
        preview_url: f.assets?.thumbnail?.href || null,
        tile_url: f.assets?.vv?.href || null,
        bbox_geom: geom,
        archive_era: 'real-time',
        source: 'earth_search_s1',
        country: 'JP',
      },
    };
  });
}
```

Update the chain inside `trySentinel1`:

```js
const chain = [s1TryCdseOData, s1TryPlanetaryComputer, s1TryEarthSearch];
```

- [ ] **Step 4: Run tests**

Run: `cd server && npx vitest run src/collectors/__tests__/satelliteImagery.sentinel1.test.js`
Expected: all 3 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add server/src/collectors/satelliteImagery.js server/src/collectors/__tests__/satelliteImagery.sentinel1.test.js
git commit -m "feat: Sentinel-1 GRD Earth Search fallback"
```

---

## Task 9: Register Sentinel-1 in the collector provider array

**Files:**
- Modify: `server/src/collectors/satelliteImagery.js`

**Context:** The new `trySentinel1` function exists but isn't called by the top-level `collectSatelliteImagery()`. Add it to the `providers` array so it runs each collection cycle.

- [ ] **Step 1: Add the provider entry**

In `server/src/collectors/satelliteImagery.js`, find the `providers` array inside `collectSatelliteImagery` (around line 529):

```js
const providers = [
  { name: 'nict_himawari',    fn: tryHimawari },
  { name: 'nasa_gibs_modis',  fn: async () => gibsModis() },
  { name: 'nasa_gibs_viirs',  fn: async () => gibsViirs() },
  { name: 'planetary_computer_landsat', fn: tryLandsatPC },
  { name: 'rammb_slider_goes18', fn: async () => rammbGoes() },
  { name: 'jaxa_alos2_seed',  fn: async () => alos2Seed() },
  { name: 'usgs_m2m_historical', fn: tryUsgsHistorical },
  { name: 'sentinel2_multi',  fn: trySentinel2 },
];
```

Add `sentinel1_multi` as the last entry:

```js
{ name: 'sentinel1_multi', fn: trySentinel1 },
```

Also update the `_meta.description` string (same function, ~line 562) to include `Sentinel-1`:

```js
description: 'Live + archival satellite imagery over Japan (Himawari-9, MODIS, VIIRS; Landsat/GOES/ALOS/CORONA/Sentinel-2/Sentinel-1 added by extension tasks)',
```

- [ ] **Step 2: Smoke test the full collector**

Run: `cd server && node -e "import('./src/collectors/satelliteImagery.js').then(m => m.default()).then(r => console.log(JSON.stringify(r._meta)))"`
Expected: output JSON `_meta` shows `live_source` containing `sentinel1_multi` if any provider returned data, or other providers without Sentinel-1 if the S1 chain was empty (both outcomes acceptable — we are testing that it is *called* and doesn't throw).

- [ ] **Step 3: Run all server tests**

Run: `cd server && npx vitest run`
Expected: all tests PASS, no regressions.

- [ ] **Step 4: Commit**

```bash
git add server/src/collectors/satelliteImagery.js
git commit -m "feat: register Sentinel-1 provider in imagery collector"
```

---

## Task 10: Verify Sentinel-1A/1C show up on the tracking layer

**Files:** (no code changes — verification only)

**Context:** The spec asserts that Sentinel-1A (NORAD 39634) and Sentinel-1C (NORAD 62261) already arrive via CelesTrak's `active` group ingested by `server/src/collectors/satelliteTracking.js`. Confirm.

- [ ] **Step 1: Start the dev server**

Run: `cd server && npm run dev`

- [ ] **Step 2: Query the tracking endpoint**

In a second shell:

```bash
curl -s http://localhost:3000/api/data/satellite-tracking | grep -oi 'sentinel-1[abc]' | sort -u
```

Expected output:

```
SENTINEL-1A
SENTINEL-1C
```

(ordering and case may vary depending on CelesTrak's catalogue spelling — the point is that both 1A and 1C appear).

- [ ] **Step 3: If one is missing**

If Sentinel-1C is missing, it means CelesTrak's `active.txt` hasn't been refreshed since its Dec 2024 launch registered upstream. Check the server log for `celestrak` fetch errors. This does not require code change — CelesTrak is authoritative for whether a satellite is considered "active". Document the finding in the final commit message.

- [ ] **Step 4: No commit**

Verification-only task; nothing to commit unless Step 3 turned up a follow-up.

---

## Task 11: End-to-end visual verification

**Files:** (no code changes — manual verification)

- [ ] **Step 1: Start both servers**

Run: `cd server && npm run dev` and in a second shell `cd client && npm run dev`

- [ ] **Step 2: Open the map in a browser**

Navigate to the dev URL (`http://localhost:5173` by default).

- [ ] **Step 3: Enable "Live Satellite Positions"**

Toggle the layer. Confirm:
- Dots are multi-coloured (not all the same purple).
- Each dot has a same-coloured thick line extending outward, representing ~90 min of forward orbit.
- Antimeridian-crossing tracks break cleanly instead of wrapping.

- [ ] **Step 4: Click a satellite dot**

Confirm:
- Popup opens with satellite name, NORAD id, altitude, velocity, inclination.
- No "Show ground track" button (it was removed).

- [ ] **Step 5: Wait 60 seconds with the layer on**

Confirm the tracks visibly advance (the head of each line moves ~1/90th of its length forward).

- [ ] **Step 6: Enable "Satellite Imagery"**

Confirm at least one Sentinel-1 scene centroid appears (platform prefix `sentinel-1`). Click it — `scene_id` starts with `S1A_` or `S1C_`, `polarization: VV`, `product_type: GRD`.

- [ ] **Step 7: Bake a Sentinel-1 scene**

In the imagery popup, toggle "Bake feed on map". Confirm a grayscale raster overlay appears over the scene footprint.

- [ ] **Step 8: No commit**

Manual verification only.

---

## Self-Review Pass

Checked against the spec:

- Persistent ground tracks, thick (3px), colour-matched → Task 1 (color helper), Task 2 (hook), Task 3 (layer), Task 5 (marker colour) ✓
- 90-minute forward orbit → existing `computeGroundTrack` reused with `{ minutes: 90, stepSec: 30 }` (Task 2) ✓
- Click-triggered button removed → Task 3 (handler) + Task 4 (button) ✓
- Sentinel-1 GRD provider → Tasks 6-8 (CDSE, PC, Earth Search) + Task 9 (registration) ✓
- Grayscale VV only → every feature carries `polarization: 'VV'`; tile URLs use `assets=vv` (PC) or `assets.vv.href` (Earth Search) ✓
- CDSE → Planetary Computer → Earth Search order → chain literal in Tasks 6/7/8 ✓
- Sentinel-1A/1C show up in tracking → Task 10 verification ✓
- Testing (unit + integration + visual) → Tasks 1, 2, 6, 7, 8 unit/integration; Task 11 visual ✓

No placeholders found. Type/function names consistent: `satelliteColor`, `useSatelliteTracks`, `trySentinel1`, `s1TryCdseOData` / `s1TryPlanetaryComputer` / `s1TryEarthSearch`, `s1IsoWindow`, `s1PlatformFromName`, `s1CentroidFromGeom`, `S1_SCENE_LIMIT`, `S1_WINDOW_DAYS` used consistently across Tasks 6-8.
