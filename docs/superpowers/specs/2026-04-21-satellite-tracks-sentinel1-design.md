# Persistent Satellite Ground Tracks + Sentinel-1 GRD Imagery

**Date:** 2026-04-21
**Status:** Design — approved for implementation planning

## Goals

1. Render every tracked satellite's forward ground track as a persistent thick line on the map, color-matched to its marker. Replace the current button-triggered single-satellite overlay.
2. Add Sentinel-1 GRD as a new provider in the satellite-imagery collector, grayscale VV, with fallback chain CDSE → Planetary Computer → Earth Search.
3. Sentinel-1A / Sentinel-1C show up automatically as tracked positions (they are already in CelesTrak's `active` group; no code change needed, only verification).

## Non-goals

- Field-of-view / swath footprint rendering (explicitly dropped).
- Past-track / multi-orbit history.
- Sentinel-1 SLC products, VH polarization, or false-color composites.
- Server-side pre-computation of ground tracks.

## Ground Tracks

### Architecture

Client-side computation. Every satellite feature already carries `tle_line1` / `tle_line2` from the server. `satellite.js` + the existing `computeGroundTrack()` helper in `client/src/utils/groundTrack.js` produce a 90-minute forward polyline (30-second sampling, antimeridian-safe MultiLineString output). That helper is untouched.

A new React hook `useSatelliteTracks` derives a GeoJSON `FeatureCollection` of LineString/MultiLineString features from the tracked-satellite FeatureCollection. Each derived feature carries:

```json
{
  "type": "Feature",
  "geometry": { "type": "LineString" | "MultiLineString", "coordinates": [...] },
  "properties": {
    "satellite_id": "<NORAD id>",
    "satellite_name": "<name>",
    "color": "#RRGGBB"
  }
}
```

Color is deterministic per satellite: a shared helper `satelliteColor(noradId)` returns a hex string by hashing the NORAD ID into a fixed palette of ~24 visually distinct hues. Both the marker layer and the track layer call this helper, so track and marker always match. If the marker layer currently uses a static color, it is updated to call this helper as part of this work (minor change in `MapView.jsx`).

A dedicated MapLibre `GeoJSONSource` named `satellite-tracks` feeds a `line` layer with:

- `line-color`: `["get", "color"]`
- `line-width`: `3`
- `line-opacity`: `0.85`

The layer is inserted **below** the satellite icon layer so markers stay on top.

### Refresh cadence

A single `setInterval` in `useSatelliteTracks` recomputes tracks every 60 seconds. Each recompute is chunked via `requestIdleCallback` (16 satellites per idle slice) so the main thread is never blocked for more than a few milliseconds.

### Performance budget

- ~200 satellites × 180 samples = 36k points total.
- Well inside MapLibre's GeoJSON line-layer comfort zone (it handles >100k without stutter).
- Memory: each Float64 coordinate pair is 16 B → ~600 KB of geometry. Acceptable.

### Code removed

The click-triggered ground track is deleted once tracks are persistent:

- `MapView.jsx:4107-4149` — the `satellite-track-toggle` event handler.
- `MapPopup.jsx:906-957` — the "Show ground track" button and its local state in `SatelliteTrackingDetail`.

The popup still opens on click and still shows TLE / orbital details — only the toggle button goes away.

## Sentinel-1 GRD Imagery

### Provider chain

A new `trySentinel1()` function added to `server/src/collectors/satelliteImagery.js`. Registered in the `providers` array after the existing `sentinel2_multi`:

```js
{ name: 'sentinel1_multi', fn: trySentinel1 },
```

The function tries three sources in order, first win skips the rest:

1. **Copernicus Data Space Ecosystem (CDSE) OData**
   - Endpoint: `https://catalogue.dataspace.copernicus.eu/odata/v1/Products`
   - Filter: `Collection/Name eq 'SENTINEL-1' and contains(Name, 'GRD') and ContentDate/Start ge <now-14d> and OData.CSC.Intersects(area=geography'SRID=4326;POLYGON((<japan bbox>))')`
   - Token: refresh via the existing CDSE helper already used by Sentinel-2.
2. **Microsoft Planetary Computer STAC**
   - Endpoint: `https://planetarycomputer.microsoft.com/api/stac/v1/search`
   - Collection: `sentinel-1-grd`
   - Body: `{ collections: ['sentinel-1-grd'], bbox: [<japan>], datetime: '<now-14d>/..', limit: 60 }`
   - No auth.
3. **AWS Earth Search (Element84)**
   - Endpoint: `https://earth-search.aws.element84.com/v1/search`
   - Collection: `sentinel-1-grd`
   - Same STAC search body shape.
   - No auth.

### Feature schema

Each returned GeoJSON `Feature` carries:

```json
{
  "type": "Feature",
  "geometry": "<footprint polygon>",
  "properties": {
    "id": "S1_<provider>_<scene_id>",
    "platform": "sentinel-1a" | "sentinel-1c",
    "sensor": "c-sar",
    "product_type": "GRD",
    "polarization": "VV",
    "scene_id": "<provider scene id>",
    "datetime": "<ISO8601>",
    "preview_url": "<quicklook>",
    "tile_url": "<XYZ/WMTS URL with VV rendering>",
    "source": "cdse" | "planetary_computer" | "earth_search",
    "archive_era": "real-time"
  }
}
```

### Visualization

Grayscale VV single-band. Tile URL shape per provider:

- **CDSE**: WMTS layer `SENTINEL-1-GRD`, style `GRAYSCALE-VV`, single-tile URL embedded in `tile_url`.
- **Planetary Computer**: `https://planetarycomputer.microsoft.com/api/data/v1/item/tiles/{z}/{x}/{y}?collection=sentinel-1-grd&item=<id>&assets=vv&rescale=-30,0`.
- **Earth Search**: `vv` COG asset URL. The existing `MapView` "bake feed on map" path adds a raster source pointing at the COG tile endpoint.

### Caching

Same pattern as other providers: results flow through the normal imagery collector output and are served by `/api/data/satellite-imagery`. No new cache store.

## Tracking-side impact

Sentinel-1A (NORAD 39634) and Sentinel-1C (NORAD 62261) are in CelesTrak's `active` group, which is already ingested by `server/src/collectors/satelliteTracking.js`. No code change needed. Verification step after deploy: `curl /api/data/satellite-tracking | grep -i sentinel-1`.

Sentinel-1B (NORAD 41456) is decommissioned; it will not appear.

## Files touched

| File | Change |
|------|--------|
| `client/src/utils/groundTrack.js` | No change |
| `client/src/hooks/useSatelliteTracks.js` | **New** — derives track FeatureCollection from satellite positions |
| `client/src/hooks/useMapLayers.js` | Expose tracks alongside positions |
| `client/src/components/map/MapView.jsx` | Add `satellite-tracks` source + line layer; remove lines 4107-4149 |
| `client/src/components/map/MapPopup.jsx` | Remove "Show ground track" button and local state in `SatelliteTrackingDetail` |
| `server/src/collectors/satelliteImagery.js` | Add `trySentinel1()`; register in providers array |

## Testing

- **Unit**: add a test for `useSatelliteTracks` that feeds a two-satellite FeatureCollection and asserts the derived LineString count and color determinism.
- **Integration**: smoke-test `/api/data/satellite-imagery` after server changes, assert at least one feature has `platform: sentinel-1a` or `sentinel-1c`.
- **Visual**: dev server — confirm persistent colored orbits render, markers still clickable, popup no longer shows the removed button, Sentinel-1 imagery bakes as grayscale raster.

## Rollback

Ground tracks: revert the three client files; the button-triggered path is self-contained and harmless to leave as the sole entry point again.

Sentinel-1: remove `trySentinel1` from the providers array; the rest of the collector is unchanged.
