# Satellite Collectors — Design

**Date:** 2026-04-19
**Status:** Approved, pending implementation plan

## Goal

Add three new satellite-focused OSINT data sources to JapanOSINT, complementing the existing `sentinelHub.js` (Sentinel-2 scene footprints) and `satelliteGroundStations.js` (JAXA/NICT/KDDI antennas):

1. **Satellite imagery** — multi-provider collector for live + archival scenes over Japan, with embedded preview feeds shown on popup click.
2. **Live satellite positions** — every tracked object currently overhead Japan (including debris and CubeSats), with ground tracks computed on demand.
3. **Satellite infrastructure expansion** — adds more operators, VLBI/SLR/optical tracking sites, and all ~1,300 GEONET GNSS reference stations to the existing ground stations collector.

## Collector 1 — `server/src/collectors/satelliteImagery.js`

### Purpose

Unified imagery catalog collector. Each feature = scene centroid with acquisition metadata and a preview URL that the map popup renders as an embedded image (`<img src={preview_url}>`) — the "feed on click."

### Sources (free-first, multi-provider fallback chain)

| # | Platform / Sensor | Provider endpoint | Auth | Archive era |
|---|-------------------|-------------------|------|-------------|
| 1 | Himawari-9 AHI (geostationary, 10-min full-disk) | NICT sci-cloud WMS; RAMMB SLIDER tile URLs | none | real-time |
| 2 | Landsat 8/9 OLI | USGS M2M STAC; Microsoft Planetary Computer fallback | M2M needs token; PC public | real-time |
| 3 | MODIS Terra/Aqua | NASA GIBS WMTS daily mosaic; LAADS DAAC | none | real-time + archive to 2000 |
| 4 | ALOS-2 / PALSAR-2 (L-band SAR) | JAXA G-Portal catalog | auth-gated → seed fallback | real-time |
| 5 | NOAA GOES-18 (West Pacific edge) | RAMMB SLIDER tiles | none | real-time |
| 6 | VIIRS (Suomi NPP, NOAA-20) | NASA GIBS WMTS | none | real-time |
| 7 | CORONA (declassified 1960s–72) + historical Landsat 1–5 MSS (1972–1999) | USGS EarthExplorer M2M browse | token required → seed fallback | historical |

Sentinel-2 stays in its existing `sentinelHub.js` collector — not merged.

### Feature shape

```js
{
  type: 'Feature',
  geometry: { type: 'Point', coordinates: [lon, lat] }, // scene centroid
  properties: {
    id: 'IMG_<provider>_<sceneId>',
    platform: 'Himawari-9',      // satellite
    sensor: 'AHI',                // instrument
    scene_id: '<provider-native id>',
    datetime: '2026-04-19T14:20:00Z',
    cloud_cover: 12,              // percent, or null
    preview_url: 'https://…/thumbnail.jpg',
    tile_url: 'https://…/{z}/{x}/{y}.png',  // if provider is tile-based
    bbox_geom: {...},             // original scene polygon, optional
    archive_era: 'real-time' | 'historical',
    source: 'nict_wms' | 'planetary_computer' | …,
    country: 'JP',
  }
}
```

### Fallback behavior

- Per-provider try/fail: on HTTP error or empty result, fall through to next.
- Final fallback: seeded Japan-bbox grid centroids with `source: 'satellite_imagery_seed'`, `live: false`.
- Standard `_meta` envelope: `{source, fetchedAt, recordCount, live, live_source, description}`.

### Cadence

30 minutes. Himawari-9 updates every 10 min but aggregating across 7 providers at higher frequency wastes quota.

## Collector 2 — `server/src/collectors/satelliteTracking.js`

### Purpose

Show every tracked on-orbit object (active sats + debris + CubeSats) currently passing over Japan, as live-moving Point features. On popup click, the client computes and renders the next 90-minute ground track.

### Source

- **CelesTrak GP** (`https://celestrak.org/NORAD/elements/gp.php?GROUP=active&FORMAT=json`) — no auth, ~11,000 TLEs, refreshed daily.
- Supplemental groups: `debris`, `cubesat`, `stations` (ISS, CSS).
- TLEs cached server-side for 24h.

### Computation

- npm dependency: **`satellite.js`** for SGP4 propagation.
- On each tick, propagate every TLE to `Date.now()`, emit a Point feature for every object whose subsatellite point falls inside Japan bbox `[122, 24, 154, 46]`.
- Expected feature count per tick: 100–300 (bbox is large enough to catch most LEO passes).

### Ground tracks

Shipping 1,000+ polylines per tick is too heavy. Instead:
- Each feature's properties carry `tle_line1` and `tle_line2`.
- Client renders current-position Points normally.
- On popup **"Show ground track"** click, the client uses the same `satellite.js` (bundled in `client/`) to compute the next 90-min ground track locally and render it as a temporary GeoJSON line layer that clears when the popup closes.
- Keeps server payload small and avoids redundant polyline recomputation.

### Feature shape

```js
{
  type: 'Feature',
  geometry: { type: 'Point', coordinates: [lon, lat] },
  properties: {
    id: 'SAT_<norad_id>',
    norad_id: 25544,
    name: 'ISS (ZARYA)',
    country: 'International',
    category: 'active' | 'debris' | 'cubesat' | 'station',
    altitude_km: 421.3,
    velocity_kms: 7.66,
    inclination_deg: 51.64,
    next_pass_utc: '2026-04-19T23:14:00Z',  // over Japan bbox
    tle_line1: '1 25544U …',
    tle_line2: '2 25544 …',
    source: 'celestrak',
  }
}
```

### Cadence

60 seconds. Positions move fast; slower refresh makes points feel static.

## Collector 3 — Expansion of `server/src/collectors/satelliteGroundStations.js`

### Added categories

New `category` property distinguishes each station type:
- `satcom` (existing commercial: KDDI, JSAT, NTT, SoftBank, Rakuten, Intelsat Ibaraki, Inmarsat Yamaguchi)
- `university` (UTokyo Kashiwa, Kyushu Uni, Hokkaido Uni Uchinada, Tohoku Uni)
- `vlbi` (VERA array: Mizusawa, Iriki, Ishigakijima, Ogasawara; Nobeyama 45m; Kashima 34m)
- `slr` (Koganei NICT, Simosato JHA)
- `optical_tracking` (Bisei Spaceguard Center, JAXA Mt. Nyukasa)
- `launch` / `deep_space` / `tt&c` / `tracking` / `mission_control` (existing JAXA)
- `gnss_reference` (GEONET — ~1,300 GSI stations)

### GEONET ingestion

GSI (Geospatial Information Authority of Japan) publishes the full GEONET station list. Collector:
1. Fetches the GSI station list (CSV or JSON from GSI's public API).
2. If fetch fails, falls back to a seeded subset of ~50 flagship stations.
3. Each station → Feature with `category: 'gnss_reference'`, operator `GSI`, and antenna-site metadata.

### Feature shape addition

Existing shape plus:
```js
properties: {
  …existing fields,
  category: 'satcom' | 'vlbi' | 'slr' | … | 'gnss_reference',
  station_code: '940001',      // GEONET 6-digit code where applicable
}
```

### Cadence

24 hours. Infrastructure rarely moves.

## Client Integration

### Layer panel (`client/src/components/map/LayerPanel.jsx`)

New **"Satellite"** group with three toggles:
- Satellite Imagery (scene footprints)
- Live Satellite Positions (moving points)
- Satellite Infrastructure (ground stations, VLBI, GEONET)

### Popups (`client/src/components/map/MapPopup.jsx`)

**Imagery popup** (renders live feed thumbnail):
```
Platform: Himawari-9    Sensor: AHI
Date: 2026-04-19 14:20 UTC    Cloud: 12%
<img src={preview_url} style="max-width: 240px" />
Source: nict_wms    Archive: real-time
```

For tile-based providers (NICT Himawari, NASA GIBS), the popup renders a small embedded tile preview centered on the scene centroid instead of a static `<img>`.

**Tracking popup:**
```
Name: ISS (ZARYA)    NORAD: 25544
Altitude: 421 km    Velocity: 7.66 km/s
Next pass: 23:14 JST
[button: Show ground track]  ← client-side SGP4, 90-min forward track
```

Clicking the button adds a temporary line layer; closing the popup removes it.

**Infrastructure popup:** existing format plus a `category` badge.

### New client dependency

`satellite.js` (~40 KB gzipped) added to `client/package.json`. Used only when a tracking popup's ground-track button is clicked.

## Wiring

- **`server/src/collectors/index.js`:** export `satelliteImagery`, `satelliteTracking`; `satelliteGroundStations` already exported (expanded in place).
- **`server/src/utils/sourceRegistry.js`:** add entries with category `satellite`, cadences 30 min / 60 s / 24 h respectively.
- **`server/src/routes/data.js`:** no changes — dynamic pickup from the collector registry.
- **Env vars (optional, degrade to seed if missing):**
  - `USGS_M2M_TOKEN` — CORONA + historical Landsat
  - `JAXA_GPORTAL_TOKEN` — ALOS-2/PALSAR-2 live catalog

## Failure Model

Every collector returns the standard envelope:
```js
{
  type: 'FeatureCollection',
  features,
  _meta: { source, fetchedAt, recordCount, live, live_source, description },
  metadata: {},
}
```

If all live providers fail, each collector returns a seeded fallback with `live: false`. Clients already render seeded data; no UI changes needed to handle this path.

## Testing

- **Unit:** TLE parsing + SGP4 propagation returns expected ISS altitude (~400–420 km).
- **Unit:** imagery provider chain falls through correctly on 500 errors and empty responses.
- **Integration:** all three collectors return valid GeoJSON with features inside Japan bbox.
- **Manual:** popup `<img>` loads for each imagery source; ground-track line renders on tracking-popup click and clears on close.

## Out of Scope

- Client-side rendering of satellite orbit animation (moving dots between refreshes) — collector returns snapshot positions only.
- Georeferenced overlay of full Himawari imagery onto the map as a tile layer (only preview thumbnails in popups).
- Automatic alerting when a specific satellite passes overhead.
- Downloading full scene imagery (only preview/browse URLs are exposed).
