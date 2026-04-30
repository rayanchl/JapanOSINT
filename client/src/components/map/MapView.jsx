import React, { useRef, useEffect, useState, useCallback, useMemo } from 'react';
import maplibregl from 'maplibre-gl';
import 'maplibre-gl/dist/maplibre-gl.css';
import { MapboxOverlay } from '@deck.gl/mapbox';
import { Tile3DLayer } from '@deck.gl/geo-layers';
import { TextLayer } from '@deck.gl/layers';
import { Tiles3DLoader } from '@loaders.gl/3d-tiles';
import { Matrix4 } from '@math.gl/core';
import { LAYER_DEFINITIONS } from '../../hooks/useMapLayers';
import useLiveVehicles from '../../hooks/useLiveVehicles';
import { getLayerIcon } from '../../utils/layerIcons';
import { rasterizeIcon } from '../../utils/iconRaster';
import { createRailwayLineTags } from '../../utils/railwayLineTags';
import { useSatelliteTracks } from '../../hooks/useSatelliteTracks.js';
import { satelliteColor } from '../../utils/satelliteColor.js';

// Per-layer handles for DOM-based add-ons (e.g. railway line tags) that
// live outside MapLibre's layer system and must be torn down alongside
// addLayerToMap / removeLayerFromMap.
const layerAddonHandles = new Map();
function clearLayerAddons(layerId) {
  const h = layerAddonHandles.get(layerId);
  if (h) {
    try { h.destroy(); } catch (_) { /* ignore */ }
    layerAddonHandles.delete(layerId);
  }
}

// OSM is the primary basemap. GSI is kept as a Japan-specific alternative
// (cartography detail) and OSM Standard gives full-color OSM rendering.
// ─── Icon rendering ─────────────────────────────────────────────────
// Each layer is represented by a flat react-icons glyph (Material Design
// or FontAwesome) tinted with the layer color, rasterized once and
// registered with MapLibre via addImage.

const ICON_IMAGE_SIZE = 64;

function layerIconImageId(layerId) {
  return `icon-${layerId}`;
}

async function registerLayerIcons(map) {
  await Promise.all(
    Object.entries(LAYER_DEFINITIONS).map(async ([layerId, def]) => {
      const imgId = layerIconImageId(layerId);
      if (!map.hasImage(imgId)) {
        const Icon = getLayerIcon(layerId);
        const tint = darkenHex(def?.color || '#ffffff', 0.8);
        const imageData = await rasterizeIcon(Icon, tint, ICON_IMAGE_SIZE);
        if (imageData && !map.hasImage(imgId)) {
          map.addImage(imgId, imageData, { pixelRatio: 2 });
        }
      }
    }),
  );

  // Red variant for military aircraft. Same glyph as flightAdsb, but tinted red.
  if (!map.hasImage('icon-flightAdsb-mil')) {
    const Icon = getLayerIcon('flightAdsb');
    const imageData = await rasterizeIcon(Icon, '#ff3344', ICON_IMAGE_SIZE);
    if (imageData && !map.hasImage('icon-flightAdsb-mil')) {
      map.addImage('icon-flightAdsb-mil', imageData, { pixelRatio: 2 });
    }
  }

  // Ground-position cross sprite: a small "+" marker placed at the
  // feature's true lng/lat so users can see where the icon "belongs"
  // even though the icon itself is offset upward. 8-display-px arms,
  // 3-display-px stroke (centered), semi-transparent gray. Baked at
  // pixelRatio 2 inside a 64×64 buffer so it shares the icon sprite
  // format (matches the placement math MapLibre uses for other icons).
  if (!map.hasImage('ground-cross')) {
    const w = 64;
    const h = 64;
    const data = new Uint8ClampedArray(w * h * 4);
    const cx = 32;
    const cy = 32;
    const armPx = 16;      // 16 buffer px = 8 display px each side of center
    const strokePx = 6;    // 6 buffer px = 3 display px thickness
    const halfStroke = Math.floor(strokePx / 2);
    const paint = (x, y) => {
      if (x < 0 || x >= w || y < 0 || y >= h) return;
      const i = (y * w + x) * 4;
      data[i + 0] = 90;
      data[i + 1] = 90;
      data[i + 2] = 90;
      data[i + 3] = 240;
    };
    for (let d = -armPx; d <= armPx; d++) {
      for (let s = -halfStroke; s < strokePx - halfStroke; s++) {
        paint(cx + d, cy + s); // horizontal arm, centered on cy
        paint(cx + s, cy + d); // vertical arm, centered on cx
      }
    }
    map.addImage('ground-cross', { width: w, height: h, data }, { pixelRatio: 2 });
  }

  // Rounded-rectangle icon for live-transit trains. White pixels so it can
  // be tinted with `icon-color` via `sdf: true` at runtime. Baked at
  // pixelRatio 2 like ground-cross. Aspect roughly 2:1 (train shape).
  if (!map.hasImage('live-train-rect')) {
    const w = 64;   // width buffer
    const h = 32;   // height buffer (2:1)
    const r = 12;   // corner radius in buffer px (more rounded)
    const data = new Uint8ClampedArray(w * h * 4);
    for (let y = 0; y < h; y++) {
      for (let x = 0; x < w; x++) {
        // Compute distance to nearest corner; inside the rounded rect if
        // either within the inner rect or the corner quarter-circle.
        const inHorizCore = x >= r && x < w - r;
        const inVertCore = y >= r && y < h - r;
        let inside = false;
        if (inHorizCore || inVertCore) {
          inside = true;
        } else {
          // In a corner region — clamp to nearest corner center.
          const cx = x < r ? r : (w - 1 - r);
          const cy = y < r ? r : (h - 1 - r);
          const dx = x - cx;
          const dy = y - cy;
          inside = dx * dx + dy * dy <= r * r;
        }
        if (inside) {
          const i = (y * w + x) * 4;
          data[i + 0] = 255;
          data[i + 1] = 255;
          data[i + 2] = 255;
          data[i + 3] = 255;
        }
      }
    }
    // `sdf: true` lets MapLibre tint the pixels via icon-color at paint
    // time. The pure-white pixel values act as the coverage mask.
    map.addImage('live-train-rect', { width: w, height: h, data }, { pixelRatio: 2, sdf: true });
  }
}

// Fixed icon size — flat 2D react-icons. Bumped 20% from the previous 0.5.
const UNIFORM_ICON_SIZE = 0.6;

// Darken a `#rrggbb` color by multiplying each channel. Used so rasterized
// pin icons render deeper than the legend swatch color.
function darkenHex(hex, factor = 0.8) {
  if (typeof hex !== 'string') return hex;
  const m = hex.trim().match(/^#?([0-9a-f]{6})$/i);
  if (!m) return hex;
  const n = parseInt(m[1], 16);
  const r = Math.round(((n >> 16) & 0xff) * factor);
  const g = Math.round(((n >> 8) & 0xff) * factor);
  const b = Math.round((n & 0xff) * factor);
  const to2 = (v) => v.toString(16).padStart(2, '0');
  return `#${to2(r)}${to2(g)}${to2(b)}`;
}

// Fallback bake footprint for full-disk/mosaic products without a per-scene
// polygon (Himawari, MODIS, VIIRS, GOES, ALOS). MapLibre wants
// [top-left, top-right, bottom-right, bottom-left] in lon/lat.
const JAPAN_BAKE_CORNERS = [[122, 46], [154, 46], [154, 24], [122, 24]];

const slugifyBakeId = (s) => String(s).replace(/[^A-Za-z0-9_-]+/g, '_').slice(0, 120);
const bakeSourceIdFor = (sceneId) => `satellite-bake-source-${slugifyBakeId(sceneId)}`;
const bakeLayerIdFor = (sceneId) => `satellite-bake-layer-${slugifyBakeId(sceneId)}`;

function imageCoordsFromGeom(geom) {
  // MapLibre's queryRenderedFeatures() JSON-stringifies non-primitive
  // feature properties, so bbox_geom arrives at the popup as a string.
  let g = geom;
  if (typeof g === 'string') {
    try { g = JSON.parse(g); } catch { g = null; }
  }
  const ring = g?.type === 'Polygon'
    ? g.coordinates?.[0]
    : g?.type === 'MultiPolygon'
      ? g.coordinates?.[0]?.[0]
      : null;
  if (!ring?.length) return JAPAN_BAKE_CORNERS;
  let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
  for (const pt of ring) {
    const x = pt?.[0], y = pt?.[1];
    if (!Number.isFinite(x) || !Number.isFinite(y)) continue;
    if (x < minX) minX = x;
    if (x > maxX) maxX = x;
    if (y < minY) minY = y;
    if (y > maxY) maxY = y;
  }
  if (!Number.isFinite(minX) || !Number.isFinite(maxX)) return JAPAN_BAKE_CORNERS;
  return [[minX, maxY], [maxX, maxY], [maxX, minY], [minX, minY]];
}

// Layers whose features carry a heading/track and should have the icon
// rotated to match direction of travel.
const ROTATING_LAYERS = new Set(['flightAdsb', 'maritimeAis', 'marineTraffic', 'vesselFinder']);

// Every icon floats 30 px above its true lng/lat so the pin stands up.
// Aircraft get an altitude-scaled bonus on top; that's handled separately
// in the flightAdsb case.
const ICON_BASE_OFFSET_PX = 30;
// For non-aircraft layers we use the dropline-30 sprite at icon-size 1.0,
// keeping the stem 3 px wide regardless of layer or zoom.
// Small "+" marker drawn at the feature's true lng/lat. Replaces the
// old vertical dropline stem — same pitch-fade behavior, just a cross
// instead of a line.
const DROPLINE_BASE_IMAGE = 'ground-cross';

// Droplines only make sense in a tilted/3D view — from straight overhead a
// vertical line has no visible length. We fade stem opacity with pitch.
// 0°–15° → hidden, 15°–45° → linear fade in, 45°+ → fully visible.
// Each stem layer stores its base opacity in `droplineBaseOpacity.<layerId>`
// so the fade can multiply without losing the paint-time value.
const droplineBaseOpacity = new Map();

function pitchFade(pitchDeg) {
  if (pitchDeg <= 15) return 0;
  if (pitchDeg >= 45) return 1;
  return (pitchDeg - 15) / 30;
}

function applyDroplineFade(map) {
  if (!map || !map.getStyle) return;
  const factor = pitchFade(map.getPitch());
  for (const [layerId, baseOpacity] of droplineBaseOpacity.entries()) {
    if (!map.getLayer(layerId)) continue;
    try {
      map.setPaintProperty(layerId, 'icon-opacity', baseOpacity * factor);
    } catch {
      // Layer may have been removed between list build and set — ignore.
    }
  }
}

function registerDroplineLayer(layerId, baseOpacity) {
  droplineBaseOpacity.set(layerId, baseOpacity);
}

function unregisterDroplineLayer(layerId) {
  droplineBaseOpacity.delete(layerId);
}

// Layers that should NOT have their circle configs swapped for emoji-icon
// symbol layers. Rendering stays as plain line-colored dots.
const SKIP_ICON_SUBSTITUTION = new Set([
  'unifiedTrains', 'unifiedSubways', 'unifiedBuses', 'satelliteTracking',
  // Station dots + footprints: circle / fill should render as-is, not as
  // the generic layer-icon pin.
  'unifiedStations', 'unifiedStationFootprints',
]);

// Replace any `type: 'circle'` layer config with an equivalent
// `type: 'symbol'` layer that renders the registered layer icon.
function convertCircleConfigToSymbol(config, iconImageId, fallbackOpacity, layerId) {
  const paint = config.paint || {};
  const iconOpacity =
    paint['circle-opacity'] != null ? paint['circle-opacity'] : fallbackOpacity;

  // Rotated icons (planes, ships) must pivot around their own CENTER so
  // they don't swing horizontally away from the stem. Static icons keep
  // the bottom anchor so they sit crisply on top of the stem.
  const rotating = ROTATING_LAYERS.has(layerId);
  const layout = {
    'icon-image': iconImageId,
    'icon-size': UNIFORM_ICON_SIZE,
    'icon-allow-overlap': true,
    'icon-ignore-placement': true,
    'icon-anchor': rotating ? 'center' : 'bottom',
  };

  if (rotating) {
    layout['icon-rotate'] = [
      'coalesce',
      ['get', 'heading'],
      ['get', 'true_track'],
      ['get', 'heading_deg'],
      ['get', 'course'],
      0,
    ];
    layout['icon-rotation-alignment'] = 'map';
    layout['icon-pitch-alignment'] = 'map';
  }

  // When anchored at center, translate up by (offset + half icon height) so
  // the icon's visual bottom still rests at the stem top. Icon rendered size
  // is UNIFORM_ICON_SIZE * ICON_IMAGE_SIZE / 2 (pixelRatio 2) = 0.6*64/2 = ~19 px;
  // half of that is ~10 px.
  const centerExtra = rotating ? (UNIFORM_ICON_SIZE * ICON_IMAGE_SIZE) / 4 : 0;
  const translateY = -(ICON_BASE_OFFSET_PX + centerExtra);

  return {
    id: config.id,
    type: 'symbol',
    source: config.source,
    ...(config.filter ? { filter: config.filter } : {}),
    layout,
    paint: {
      'icon-opacity': iconOpacity,
      'icon-translate': [0, translateY],
      'icon-translate-anchor': 'viewport',
    },
  };
}

// Each entry carries a display `name` plus a `style` that MapLibre accepts —
// either a full inline style spec (for raster basemaps) or a URL string that
// points at a hosted vector style JSON. Insertion order is the order shown
// in the switcher; the initial style loaded on mount is set by the
// `currentStyle` useState default.
const MAP_STYLES = {
  carto_dark_matter: {
    name: 'CARTO Dark Matter',
    style: 'https://basemaps.cartocdn.com/gl/dark-matter-gl-style/style.json',
  },
  carto_dark_matter_nolabels: {
    name: 'CARTO Dark Matter (no labels)',
    style: 'https://basemaps.cartocdn.com/gl/dark-matter-nolabels-gl-style/style.json',
  },
  carto_voyager: {
    name: 'CARTO Voyager',
    style: 'https://basemaps.cartocdn.com/gl/voyager-gl-style/style.json',
  },
  osm_dark: {
    name: 'OSM Dark',
    style: {
      version: 8,
      sources: {
        osm: {
          type: 'raster',
          tiles: ['https://tile.openstreetmap.org/{z}/{x}/{y}.png'],
          tileSize: 256,
          attribution: '&copy; <a href="https://www.openstreetmap.org/copyright" target="_blank">OpenStreetMap</a> contributors',
          maxzoom: 19,
        },
      },
      layers: [
        {
          id: 'osm-tiles',
          type: 'raster',
          source: 'osm',
          paint: {
            'raster-saturation': -0.8,
            'raster-brightness-max': 0.35,
            'raster-contrast': 0.3,
          },
        },
      ],
    },
  },
  osm_standard: {
    name: 'OSM Standard',
    style: {
      version: 8,
      sources: {
        osm: {
          type: 'raster',
          tiles: ['https://tile.openstreetmap.org/{z}/{x}/{y}.png'],
          tileSize: 256,
          attribution: '&copy; <a href="https://www.openstreetmap.org/copyright" target="_blank">OpenStreetMap</a> contributors',
          maxzoom: 19,
        },
      },
      layers: [
        { id: 'osm-tiles', type: 'raster', source: 'osm' },
      ],
    },
  },
  gsi_pale: {
    name: 'GSI Pale (Dark)',
    style: {
      version: 8,
      sources: {
        gsi: {
          type: 'raster',
          tiles: ['https://cyberjapandata.gsi.go.jp/xyz/pale/{z}/{x}/{y}.png'],
          tileSize: 256,
          attribution: '<a href="https://maps.gsi.go.jp/" target="_blank">GSI Japan</a>',
          maxzoom: 18,
        },
      },
      layers: [
        {
          id: 'gsi-tiles',
          type: 'raster',
          source: 'gsi',
          paint: {
            'raster-saturation': -0.5,
            'raster-brightness-max': 0.4,
            'raster-contrast': 0.2,
          },
        },
      ],
    },
  },
};

// ── PLATEAU 3D buildings (deck.gl Tile3DLayer overlay) ───────────────
// The MLIT PLATEAU project ships per-city Cesium 3D Tiles for buildings
// across 300+ Japanese cities. We pull the catalog from /api/plateau/tilesets
// (server-side GraphQL proxy, cached 24h), spawn one Tile3DLayer per city
// tileset, and feed them all into a single MapboxOverlay attached to the
// MapLibre map. When the layer is on, we also hide the basemap's flat
// `building` fills so the 3D extrusions don't double up on top of 2D prints.

// Module-scoped fetch dedupe so toggling on/off in the same session doesn't
// re-hit /api/plateau/tilesets every time. Resolves to the array of tilesets.
let _plateauTilesetsPromise = null;
function loadPlateauTilesets() {
  if (!_plateauTilesetsPromise) {
    _plateauTilesetsPromise = fetch('/api/plateau/tilesets')
      .then((r) => {
        if (!r.ok) throw new Error(`tilesets HTTP ${r.status}`);
        return r.json();
      })
      .then((j) => Array.isArray(j?.tilesets) ? j.tilesets : [])
      .catch((err) => {
        // One failure shouldn't poison the cache forever — clear so the
        // next toggle retries.
        _plateauTilesetsPromise = null;
        throw err;
      });
  }
  return _plateauTilesetsPromise;
}

// Tweak this to shove PLATEAU buildings up (positive) or down (negative)
// along the local ellipsoid normal at each tileset's center, in metres.
// PLATEAU tiles are anchored on the WGS84 ellipsoid using true ellipsoidal
// height; MapLibre's basemap is at sea level (the geoid). Across Japan the
// geoid–ellipsoid separation is ~+30 to +43 m, so a negative offset of that
// magnitude pulls foundations down onto the basemap plane.
//   -37  ≈ Tokyo geoid undulation (good first guess)
//   -42  ≈ Hokkaido / Tohoku
//   -30  ≈ Okinawa
// Eyeball it and adjust until buildings sit flush on the ground.
const PLATEAU_Z_OFFSET_M = -40;

// Theme-matched uniform color (RGB 0-255). Picked muted so 3D extrusions
// read as architecture, not a data layer competing for attention.
const PLATEAU_TINT_BY_STYLE = {
  carto_dark_matter: [70, 78, 92],
  carto_dark_matter_nolabels: [70, 78, 92],
  carto_voyager: [200, 180, 150],
  osm_dark: [80, 86, 96],
  osm_standard: [185, 175, 160],
  gsi_pale: [170, 165, 155],
};

// Override every loaded glTF material's baseColorFactor with the theme tint.
// PLATEAU LOD1 ships untextured meshes whose default material is white, which
// reads as a solid white blob on dark basemaps. We walk the gltf parse result
// in-place; deck.gl's scenegraph layer picks up the change on its first draw.
function tintGltfMaterials(gltf, tint /* [r,g,b] 0-255 */) {
  if (!gltf?.materials?.length) return;
  const r = tint[0] / 255, g = tint[1] / 255, b = tint[2] / 255;
  for (const mat of gltf.materials) {
    if (!mat.pbrMetallicRoughness) mat.pbrMetallicRoughness = {};
    // Keep textured LOD2 materials intact — only repaint the white untextured
    // ones. baseColorTexture presence = textured.
    if (mat.pbrMetallicRoughness.baseColorTexture) continue;
    mat.pbrMetallicRoughness.baseColorFactor = [r, g, b, 1];
    // Slight roughness so faces still catch deck.gl's default lighting.
    if (mat.pbrMetallicRoughness.roughnessFactor == null) {
      mat.pbrMetallicRoughness.roughnessFactor = 0.85;
    }
    if (mat.pbrMetallicRoughness.metallicFactor == null) {
      mat.pbrMetallicRoughness.metallicFactor = 0;
    }
  }
}

// Walk the current MapLibre style and toggle visibility on any layer
// representing flat building footprints. CARTO vector styles ship layer
// ids `building` and `building-top`; we also match anything sourced from
// a `building` source-layer to be safe across future basemap swaps.
// Raster basemaps (OSM, GSI) have no such layer — the helper no-ops.
function setBasemapBuildingsHidden(map, hide) {
  if (!map || !map.getStyle) return;
  let style;
  try { style = map.getStyle(); } catch { return; }
  if (!style?.layers) return;
  const vis = hide ? 'none' : 'visible';
  for (const lyr of style.layers) {
    const isBuilding =
      /^building/i.test(lyr.id) ||
      lyr['source-layer'] === 'building' ||
      lyr['source-layer'] === 'building_label';
    if (!isBuilding) continue;
    try { map.setLayoutProperty(lyr.id, 'visibility', vis); } catch { /* layer gone */ }
  }
}

// ── Re-channel basemap labels through deck.gl ──────────────────────────────
// MapLibre's native symbol layers draw labels on the 2D map plane, so they
// disappear behind PLATEAU 3D extrusions. We harvest every symbol layer's
// rendered features in the current viewport and redraw them through a
// deck.gl TextLayer at PLATEAU_LABEL_ALTITUDE_M, which the same MapboxOverlay
// composites on top of the building meshes.
const PLATEAU_LABEL_ALTITUDE_M = 200;

function getBasemapSymbolLayerIds(map) {
  if (!map?.getStyle) return [];
  let style;
  try { style = map.getStyle(); } catch { return []; }
  if (!style?.layers) return [];
  return style.layers.filter((l) => l.type === 'symbol').map((l) => l.id);
}

function setBasemapSymbolsHidden(map, hide) {
  const ids = getBasemapSymbolLayerIds(map);
  const vis = hide ? 'none' : 'visible';
  for (const id of ids) {
    try { map.setLayoutProperty(id, 'visibility', vis); } catch { /* layer gone */ }
  }
}

// Pull a label string out of a feature regardless of which property the
// basemap stores it in. CARTO/MapLibre styles normalize on `name_int`/`name`,
// OSM-style sources may use `name:en`/`name:ja`/`ref`, etc.
function pickLabelText(props) {
  if (!props) return null;
  const candidates = [
    'name_int', 'name:latin', 'name_en', 'name:en', 'name', 'name:ja',
    'ref', 'shield', 'house_num',
  ];
  for (const k of candidates) {
    const v = props[k];
    if (typeof v === 'string' && v.trim()) return v.trim();
  }
  return null;
}

// Pick a representative point for a feature. For Point geometries it's the
// coordinate itself; for lines/polygons we use the first vertex (good enough
// for billboard text since road / area labels normally come pre-anchored).
function pickAnchor(geom) {
  if (!geom) return null;
  if (geom.type === 'Point') return geom.coordinates;
  if (geom.type === 'LineString' || geom.type === 'MultiPoint') return geom.coordinates[0];
  if (geom.type === 'Polygon') return geom.coordinates[0]?.[0];
  if (geom.type === 'MultiLineString' || geom.type === 'MultiPolygon') {
    return geom.coordinates[0]?.[0]?.[0] ?? geom.coordinates[0]?.[0];
  }
  return null;
}

// Harvest every visible symbol-layer feature in the viewport, dedupe by
// label text + rounded position, and return TextLayer-ready data.
function harvestBasemapLabels(map) {
  if (!map?.queryRenderedFeatures) return [];
  const symbolIds = getBasemapSymbolLayerIds(map);
  if (symbolIds.length === 0) return [];
  let feats;
  try { feats = map.queryRenderedFeatures({ layers: symbolIds }); }
  catch { return []; }
  const seen = new Set();
  const out = [];
  for (const f of feats) {
    const text = pickLabelText(f.properties);
    if (!text) continue;
    const anchor = pickAnchor(f.geometry);
    if (!anchor || !Number.isFinite(anchor[0]) || !Number.isFinite(anchor[1])) continue;
    // Dedupe key: label + ~110 m grid bucket so two identical labels close
    // together don't overlap. Different positions for same label survive.
    const key = `${text}|${anchor[0].toFixed(3)}|${anchor[1].toFixed(3)}`;
    if (seen.has(key)) continue;
    seen.add(key);
    out.push({
      text,
      position: [anchor[0], anchor[1], PLATEAU_LABEL_ALTITUDE_M],
    });
  }
  return out;
}

// Filter unified station dots + footprints to the currently-enabled modes.
// Each dot feature carries `line_mode` (one of 'train' | 'subway' | 'bus'),
// so the filter is a direct `in` over the enabled modes set. Footprints
// carry `mode_set` (array) — filter matches when any footprint mode is
// enabled.
export function applyUnifiedStationsModeFilter(map, enabledModes) {
  if (!map) return;
  const modes = Array.from(enabledModes);
  const modeFilter = modes.length === 0
    ? ['==', ['literal', 1], 2] // impossible match — hides all
    : ['in', ['get', 'line_mode'], ['literal', modes]];
  for (const id of ['layer-unifiedStations', 'layer-unifiedStations-label']) {
    if (map.getLayer(id)) map.setFilter(id, modeFilter);
  }
  const modeSetExpr = ['coalesce', ['get', 'mode_set'], ['literal', []]];
  const footprintFilter = modes.length === 0
    ? ['==', ['literal', 1], 2]
    : ['any', ...modes.map((m) => ['in', m, modeSetExpr])];
  const footprintId = 'layer-unifiedStationFootprints';
  if (map.getLayer(footprintId)) map.setFilter(footprintId, footprintFilter);
}

function addLayerToMap(map, layerId, geojson, layerDef, opacity) {
  const sourceId = `source-${layerId}`;
  const mainLayerId = `layer-${layerId}`;

  // Remove main layer and any sub-layers we created (heat, extrude, line,
  // dropline, label, ring1..4, fallback).
  const SUFFIXES = [
    '', '-heat', '-extrude', '-line', '-dropline', '-label',
    '-fallback', '-ring1', '-ring2', '-ring3', '-ring4',
  ];
  for (const s of SUFFIXES) {
    const id = `${mainLayerId}${s}`;
    if (map.getLayer(id)) map.removeLayer(id);
  }
  // Aircraft: altitude-bucket icon sub-layers created in the flightAdsb case.
  if (layerId === 'flightAdsb' && map.getStyle) {
    const allLayers = (map.getStyle().layers || []).map((l) => l.id);
    for (const id of allLayers) {
      if (
        id.startsWith(`${mainLayerId}-b`) ||
        id.startsWith(`${mainLayerId}-mil-b`)
      ) {
        map.removeLayer(id);
      }
    }
  }
  // Generic ground-cross (one per layer).
  unregisterDroplineLayer(`${mainLayerId}-dropline`);
  if (map.getSource(sourceId)) map.removeSource(sourceId);
  clearLayerAddons(layerId);

  if (!geojson || !geojson.features || geojson.features.length === 0) return;

  // tolerance: 0 disables MapLibre's internal Douglas-Peucker simplification
  // on the GeoJSON source. Default is 0.375 vector-tile pixels, which at
  // zoom 10–13 strips out 40 cm–1.6 m of our server-side Chaikin arc points
  // and makes rail polylines render as raw OSM angles. Our server has
  // already run RDP at ~10 cm so the client has nothing useful left to prune.
  map.addSource(sourceId, { type: 'geojson', data: geojson, tolerance: 0 });

  // Intercept `map.addLayer` so that every `type: 'circle'` layer produced
  // by the switch statement below gets transparently replaced with a symbol
  // layer that renders the layer's category icon (plane, boat, police, …).
  // Non-circle layers (heatmap, fill-extrusion, line, raster) pass through
  // unchanged. Skipped for transport layers that want plain colored dots.
  const iconImageId = layerIconImageId(layerId);
  const hasIcon = typeof map.hasImage === 'function' && map.hasImage(iconImageId)
    && !SKIP_ICON_SUBSTITUTION.has(layerId);
  const originalAddLayer = map.addLayer.bind(map);
  if (hasIcon) {
    map.addLayer = (config, beforeId) => {
      if (config && config.type === 'circle') {
        // Ground cross first (so the icon renders on top of it).
        // pitch/rotation alignment: 'map' makes the cross behave as if
        // painted on the ground — it foreshortens when the map tilts
        // and rotates with compass, instead of facing the viewer.
        const stemOpacity =
          (config.paint && config.paint['circle-opacity']) != null
            ? config.paint['circle-opacity'] * 0.5
            : opacity * 0.5;
        const stemLayerId = `${config.id}-dropline`;
        const fadeFactor = pitchFade(map.getPitch());
        originalAddLayer(
          {
            id: stemLayerId,
            type: 'symbol',
            source: config.source,
            ...(config.filter ? { filter: config.filter } : {}),
            layout: {
              'icon-image': DROPLINE_BASE_IMAGE,
              'icon-anchor': 'center',
              'icon-size': 1,
              'icon-rotation-alignment': 'map',
              'icon-pitch-alignment': 'map',
              'icon-allow-overlap': true,
              'icon-ignore-placement': true,
            },
            paint: { 'icon-opacity': stemOpacity * fadeFactor },
          },
          beforeId,
        );
        registerDroplineLayer(stemLayerId, stemOpacity);
        // Then the icon itself (converted).
        return originalAddLayer(
          convertCircleConfigToSymbol(config, iconImageId, opacity, layerId),
          beforeId,
        );
      }
      return originalAddLayer(config, beforeId);
    };
  }

  try {
    addLayerToMapInner(map, layerId, layerDef, opacity, sourceId, mainLayerId);
  } finally {
    if (hasIcon) {
      map.addLayer = originalAddLayer;
    }
  }
}

function addLayerToMapInner(map, layerId, layerDef, opacity, sourceId, mainLayerId) {
  switch (layerId) {
    case 'earthquakes':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'magnitude'], ['get', 'mag'], 3],
            1, 4,
            3, 8,
            5, 16,
            7, 28,
            9, 40,
          ],
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'magnitude'], ['get', 'mag'], 3],
            1, '#ffeb3b',
            3, '#ff9800',
            5, '#ff5722',
            7, '#f44336',
            9, '#b71c1c',
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ff4444',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    case 'weather':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 8,
          'circle-color': layerDef.color,
          'circle-opacity': opacity * 0.7,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.3,
        },
      });
      break;

    case 'transport':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 4,
          'circle-color': layerDef.color,
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.3,
        },
      });
      break;

    case 'unifiedTrains':
    case 'unifiedSubways':
    case 'unifiedBuses': {
      // Per-feature line_color (stamped by collectors from OSM colour tag
      // or a deterministic hash fallback). Old cached features without the
      // property coalesce to the layer default.
      const perFeatureColor = ['coalesce', ['get', 'line_color'], layerDef.color];
      // Rail-type layers get rounded joins/caps to hide direction-change
      // kinks inside each LineString and butt transitions where adjacent
      // OSM way fragments meet. Buses intentionally keep the default
      // miter/butt so bus routes render as the raw MLIT lines.
      const isRail = layerId !== 'unifiedBuses';
      // Tracks first so station icons render on top of them.
      map.addLayer({
        id: `${mainLayerId}-line`,
        type: 'line',
        source: sourceId,
        filter: ['==', ['geometry-type'], 'LineString'],
        ...(isRail
          ? { layout: { 'line-join': 'round', 'line-cap': 'round' } }
          : {}),
        paint: {
          'line-color': perFeatureColor,
          // Rail tracks render +2 px thicker than bus so they read as
          // rails from a distance; bus keeps the old thin stroke.
          'line-width': isRail
            ? [
                'interpolate', ['linear'], ['zoom'],
                5, 2.4,
                10, 3.2,
                14, 4.4,
              ]
            : [
                'interpolate', ['linear'], ['zoom'],
                5, 0.4,
                10, 1.2,
                14, 2.4,
              ],
          'line-opacity': opacity * 0.7,
        },
      });
      // Per-mode station Points are NOT rendered here — the canonical
      // cross-mode station dot comes from the `unifiedStations` layer
      // (one feature per physical place, spanning train + subway + tram +
      // monorail). This case renders the LINE geometry only. Add a
      // degenerate mainLayerId circle so popup click-routing still has a
      // hittable target if the unified-stations layer is disabled — the
      // filter `== 'nothing'` ensures nothing actually draws.
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        filter: ['==', ['geometry-type'], 'nothing'],
        paint: { 'circle-radius': 0, 'circle-opacity': 0 },
      });
      // Line-name label. Rendered as native MapLibre symbol layer along
      // each LineString, offset off the line and drawn with a thick colored
      // halo so the label reads as a solid pill in the line color with
      // white glyphs.
      map.addLayer({
        id: `${mainLayerId}-label`,
        type: 'symbol',
        source: sourceId,
        filter: ['==', ['geometry-type'], 'LineString'],
        minzoom: 10,
        layout: {
          'text-field': [
            'coalesce',
            ['get', 'line_ref'],
            ['get', 'line'],
            ['get', 'name'],
            ['get', 'name_ja'],
            '',
          ],
          'symbol-placement': 'line',
          'text-offset': [1.2, -1.2],
          'text-size': 11,
          'text-padding': 2,
          'text-allow-overlap': false,
          'text-ignore-placement': false,
          'symbol-spacing': 250,
        },
        paint: {
          'text-color': '#ffffff',
          'text-halo-color': perFeatureColor,
          'text-halo-width': 3,
          'text-halo-blur': 0.5,
          'text-opacity': opacity,
        },
      });
      break;
    }

    case 'unifiedAisShips':
    case 'unifiedPortInfra':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        filter: ['==', ['geometry-type'], 'Point'],
        paint: {
          'circle-radius': 4,
          'circle-color': layerDef.color,
          'circle-opacity': opacity * 0.9,
        },
      });
      break;

    case 'unifiedStations': {
      // Apple-Maps station dots: one filled colored circle per (cluster,
      // line) pair, positioned ON that line's track geometry (server
      // snapped it to the nearest segment). Data is already one Point
      // per dot — no client-side stacking. Filter by currently-enabled
      // transit modes via applyUnifiedStationsModeFilter below.
      const dotRadius = [
        'interpolate', ['linear'], ['zoom'],
        9, 2.2,
        12, 3.2,
        14, 4.5,
        17, 6.5,
      ];
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': dotRadius,
          'circle-color': ['coalesce', ['get', 'line_color'], layerDef.color],
          'circle-stroke-color': '#ffffff',
          'circle-stroke-width': 1,
          'circle-opacity': opacity,
          'circle-stroke-opacity': opacity,
        },
      });
      // Station name label — one PER STATION, not per dot. Filter to the
      // "first dot" we see for each cluster. The simplest way: render the
      // label on every dot but rely on MapLibre's text-allow-overlap=false
      // to suppress duplicates — plus a per-cluster deterministic priority
      // so the same dot wins placement each render.
      map.addLayer({
        id: `${mainLayerId}-label`,
        type: 'symbol',
        source: sourceId,
        minzoom: 12,
        layout: {
          'text-field': ['coalesce', ['get', 'name'], ['get', 'name_ja'], ''],
          'text-font': ['Noto Sans Regular'],
          'text-size': [
            'interpolate', ['linear'], ['zoom'],
            12, 10,
            14, 12,
            17, 14,
          ],
          'text-anchor': 'top',
          'text-offset': [0, 0.8],
          'text-padding': 4,
          'text-allow-overlap': false,
          'text-optional': true,
        },
        paint: {
          'text-color': '#ffffff',
          'text-halo-color': '#0e1117',
          'text-halo-width': 1.4,
          'text-halo-blur': 0.4,
          'text-opacity': opacity,
        },
      });
      break;
    }

    case 'unifiedStationFootprints':
      map.addLayer({
        id: mainLayerId,
        type: 'fill',
        source: sourceId,
        minzoom: 15,
        paint: {
          'fill-color': layerDef.color,
          'fill-opacity': opacity * 0.22,
          'fill-outline-color': '#ffffff',
        },
      });
      break;

    case 'airQuality':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 10,
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'aqi'], ['get', 'value'], 50],
            0, '#00ff88',
            50, '#ffeb3b',
            100, '#ff9800',
            150, '#ff4444',
            300, '#8b0000',
          ],
          'circle-opacity': opacity * 0.75,
          'circle-stroke-width': 2,
          'circle-stroke-color': '#000000',
          'circle-stroke-opacity': opacity * 0.3,
        },
      });
      break;

    case 'radiation':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 8,
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'value'], ['get', 'nGy'], 30],
            0, '#00ff88',
            50, '#ffd600',
            100, '#ff8c00',
            200, '#ff4444',
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffd600',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    case 'cameras':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 6,
          'circle-color': [
            'match', ['coalesce', ['get', 'camera_type'], ['get', 'type'], 'other'],
            'traffic', '#ff8c00',
            'volcano', '#e53935',
            'tourist', '#7c4dff',
            'weather', '#40c4ff',
            'surveillance', '#9e9e9e',
            'insecam', '#e91e63',
            'ip_camera', '#d62d20',
            'river', '#0277bd',
            'webcam', '#ce93d8',
            'dork_hit', '#ff6f00',
            'viewpoint', '#66bb6a',
            'aggregator', '#ab47bc',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 2,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    case 'population':
      map.addLayer({
        id: `${mainLayerId}-heat`,
        type: 'heatmap',
        source: sourceId,
        paint: {
          'heatmap-weight': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'population'], ['get', 'density'], ['get', 'value'], 1000],
            0, 0,
            100000, 1,
          ],
          'heatmap-intensity': 1,
          'heatmap-color': [
            'interpolate', ['linear'], ['heatmap-density'],
            0, 'rgba(0,0,0,0)',
            0.2, '#0d47a1',
            0.4, '#1565c0',
            0.6, '#00bcd4',
            0.8, '#4dd0e1',
            1, '#e0f7fa',
          ],
          'heatmap-radius': 30,
          'heatmap-opacity': opacity * 0.7,
        },
      });
      break;

    case 'landPrice':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 6,
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'price'], ['get', 'value'], 100000],
            10000, '#2196f3',
            50000, '#4caf50',
            100000, '#ffeb3b',
            500000, '#ff9800',
            1000000, '#f44336',
          ],
          'circle-opacity': opacity * 0.75,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#000',
          'circle-stroke-opacity': opacity * 0.3,
        },
      });
      break;

    case 'river':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 7,
          'circle-color': '#42a5f5',
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 2,
          'circle-stroke-color': '#1565c0',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    case 'crime':
      map.addLayer({
        id: `${mainLayerId}-heat`,
        type: 'heatmap',
        source: sourceId,
        paint: {
          'heatmap-weight': ['coalesce', ['get', 'count'], ['get', 'incidents'], 1],
          'heatmap-intensity': 1.5,
          'heatmap-color': [
            'interpolate', ['linear'], ['heatmap-density'],
            0, 'rgba(0,0,0,0)',
            0.2, '#311b92',
            0.4, '#d32f2f',
            0.6, '#ff5722',
            0.8, '#ff9800',
            1, '#ffeb3b',
          ],
          'heatmap-radius': 25,
          'heatmap-opacity': opacity * 0.7,
        },
      });
      break;

    case 'social':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 4,
          'circle-color': [
            'match', ['coalesce', ['get', 'platform'], 'other'],
            'twitter', '#1da1f2',
            'mastodon', '#6364ff',
            'reddit', '#ff4500',
            'youtube', '#ff0000',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.7,
          'circle-stroke-width': 0.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.2,
        },
      });
      break;

    // ── Twitter/X geocoded posts ───────────────────────────────
    case 'twitterGeo':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'engagement'], ['get', 'likes'], 1],
            0, 3,
            10, 5,
            100, 8,
            1000, 14,
            10000, 22,
          ],
          'circle-color': '#1da1f2',
          'circle-opacity': opacity * 0.75,
          'circle-stroke-width': 0.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    // ── Facebook check-ins ─────────────────────────────────────
    case 'facebookGeo':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'checkins'], 1],
            0, 4,
            100, 8,
            1000, 14,
            10000, 22,
          ],
          'circle-color': '#4267b2',
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    // ── Marketplace: classifieds ───────────────────────────────
    case 'classifieds':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 4,
          'circle-color': [
            'match', ['coalesce', ['get', 'category'], 'other'],
            'free_items', '#4caf50',
            'barter', '#ff9800',
            'jobs', '#2196f3',
            'services', '#9c27b0',
            'vehicles', '#f44336',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.75,
          'circle-stroke-width': 0.5,
          'circle-stroke-color': '#000',
          'circle-stroke-opacity': opacity * 0.3,
        },
      });
      break;

    // ── Real estate ────────────────────────────────────────────
    case 'realEstate':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'price_jpy'], ['get', 'rent_jpy'], 50000],
            0, 3,
            50000, 5,
            200000, 9,
            1000000, 14,
            50000000, 20,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'listing_type'], 'rent'],
            'rent', '#8bc34a',
            'sale', '#ff7043',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.75,
          'circle-stroke-width': 0.5,
          'circle-stroke-color': '#000',
          'circle-stroke-opacity': opacity * 0.3,
        },
      });
      break;

    // ── Job boards ─────────────────────────────────────────────
    case 'jobBoards':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 3.5,
          'circle-color': [
            'match', ['coalesce', ['get', 'employment_type'], 'other'],
            'part_time', '#a1887f',
            'full_time', '#6d4c41',
            'temporary', '#bcaaa4',
            'gig', '#ff7043',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.7,
          'circle-stroke-width': 0.5,
          'circle-stroke-color': '#fff',
          'circle-stroke-opacity': opacity * 0.2,
        },
      });
      break;

    // ── Shodan IoT devices ─────────────────────────────────────
    case 'shodanIot':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 4,
          'circle-color': [
            'match', ['coalesce', ['get', 'device_type'], 'other'],
            'ip_camera', '#e91e63',
            'router', '#03a9f4',
            'nas', '#4caf50',
            'scada', '#f44336',
            'plc', '#ff5722',
            'printer', '#9e9e9e',
            'database', '#9c27b0',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 0.5,
          'circle-stroke-color': '#000',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    // ── WiFi networks ──────────────────────────────────────────
    case 'wifiNetworks':
      map.addLayer({
        id: `${mainLayerId}-heat`,
        type: 'heatmap',
        source: sourceId,
        paint: {
          'heatmap-weight': 0.5,
          'heatmap-intensity': 1.2,
          'heatmap-color': [
            'interpolate', ['linear'], ['heatmap-density'],
            0, 'rgba(0,0,0,0)',
            0.2, '#003355',
            0.4, '#005577',
            0.6, '#0077aa',
            0.8, '#00aacc',
            1, '#00ffff',
          ],
          'heatmap-radius': 18,
          'heatmap-opacity': opacity * 0.7,
        },
      });
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 2,
          'circle-color': '#00bcd4',
          'circle-opacity': opacity * 0.6,
        },
      });
      break;

    // ── Maritime AIS ───────────────────────────────────────────
    case 'maritimeAis':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'length_m'], 50],
            0, 3,
            50, 5,
            150, 8,
            300, 12,
            400, 16,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'vessel_type'], 'other'],
            'cargo', '#0277bd',
            'tanker', '#ef6c00',
            'fishing', '#558b2f',
            'passenger', '#7c4dff',
            'military', '#c62828',
            'tug', '#5d4037',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 0.5,
          'circle-stroke-color': '#000',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    // ── Flight ADS-B ───────────────────────────────────────────
    case 'flightAdsb': {
      // Aircraft: altitude-scaled icon offset + red variant for military.
      // We skip the generic circle→symbol intercept and manually add:
      //   1. Single ground-cross marker at the feature point (flat on map,
      //      fades with pitch like every other layer's ground cross).
      //   2. 5 altitude buckets × 2 colors (civilian purple, military red)
      //      = 10 icon layers with fixed icon-translate (not expression-
      //      capable so we bucket).
      const ALT_BUCKETS = [
        { minFt: -Infinity, maxFt: 2000,     translateY: -30 },
        { minFt: 2000,      maxFt: 10000,    translateY: -42 },
        { minFt: 10000,     maxFt: 20000,    translateY: -54 },
        { minFt: 20000,     maxFt: 30000,    translateY: -70 },
        { minFt: 30000,     maxFt: Infinity, translateY: -90 },
      ];

      // Ground cross — single layer covering every aircraft, flat on map.
      const aircraftStemBase = opacity * 0.5;
      const aircraftFade = pitchFade(map.getPitch());
      const stemLayerId = `${mainLayerId}-dropline`;
      map.addLayer({
        id: stemLayerId,
        type: 'symbol',
        source: sourceId,
        layout: {
          'icon-image': DROPLINE_BASE_IMAGE,
          'icon-anchor': 'center',
          'icon-size': 1,
          'icon-rotation-alignment': 'map',
          'icon-pitch-alignment': 'map',
          'icon-allow-overlap': true,
          'icon-ignore-placement': true,
        },
        paint: { 'icon-opacity': aircraftStemBase * aircraftFade },
      });
      registerDroplineLayer(stemLayerId, aircraftStemBase);

      // 5 buckets × 2 colors — 10 icon layers.
      for (const b of ALT_BUCKETS) {
        for (const mil of [false, true]) {
          const altExpr =
            b.minFt === -Infinity
              ? ['<', ['coalesce', ['get', 'altitude_ft'], 0], b.maxFt]
              : b.maxFt === Infinity
                ? ['>=', ['coalesce', ['get', 'altitude_ft'], 0], b.minFt]
                : [
                    'all',
                    ['>=', ['coalesce', ['get', 'altitude_ft'], 0], b.minFt],
                    ['<',  ['coalesce', ['get', 'altitude_ft'], 0], b.maxFt],
                  ];
          const milExpr = mil
            ? ['==', ['coalesce', ['get', 'is_military'], false], true]
            : ['!=', ['coalesce', ['get', 'is_military'], false], true];

          // Center anchor so rotation pivots around the icon center (bottom
          // anchor would swing the icon away from the stem). Extra translate
          // of half the rendered icon height so the icon's visual bottom
          // still lines up with the stem top. UNIFORM_ICON_SIZE * sprite 64
          // @ pixelRatio 2 → ~19 px rendered; half ≈ 10 px.
          const aircraftCenterExtra = (UNIFORM_ICON_SIZE * ICON_IMAGE_SIZE) / 4;
          map.addLayer({
            id: `${mainLayerId}${mil ? '-mil' : ''}-b${b.minFt}`,
            type: 'symbol',
            source: sourceId,
            filter: ['all', altExpr, milExpr],
            layout: {
              'icon-image': mil ? 'icon-flightAdsb-mil' : layerIconImageId('flightAdsb'),
              'icon-size': UNIFORM_ICON_SIZE,
              'icon-anchor': 'center',
              'icon-rotate': ['coalesce', ['get', 'heading'], ['get', 'true_track'], 0],
              'icon-rotation-alignment': 'map',
              'icon-pitch-alignment': 'map',
              'icon-allow-overlap': true,
              'icon-ignore-placement': true,
            },
            paint: {
              'icon-opacity': opacity,
              'icon-translate': [0, b.translateY - aircraftCenterExtra],
              'icon-translate-anchor': 'viewport',
            },
          });
        }
      }
      break;
    }

    // ── Full transport (nationwide rail) ───────────────────────
    case 'fullTransport':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'match', ['coalesce', ['get', 'station_type'], 'other'],
            'shinkansen', 7,
            'jr_major', 5,
            'subway', 3.5,
            'private', 3,
            4,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'station_type'], 'other'],
            'shinkansen', '#e53935',
            'jr_major', '#43a047',
            'subway', '#1e88e5',
            'private', '#fb8c00',
            'monorail', '#8e24aa',
            'tram', '#00897b',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 0.5,
          'circle-stroke-color': '#fff',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    // ── Bus terminals ──────────────────────────────────────────
    case 'busRoutes':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 5,
          'circle-color': '#fb8c00',
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#fff',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    // ── Ferry terminals ────────────────────────────────────────
    case 'ferryRoutes':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 6,
          'circle-color': '#039be5',
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#fff',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    // ── Highway IC/JCT/SA/PA ───────────────────────────────────
    case 'highwayTraffic':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'match', ['coalesce', ['get', 'facility_type'], 'IC'],
            'JCT', 7,
            'SA', 6,
            'PA', 4,
            'IC', 5,
            5,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'congestion'], 'free'],
            'jam', '#f44336',
            'congested', '#ff9800',
            'slow', '#ffeb3b',
            '#4caf50',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#000',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    // ── Electrical grid ────────────────────────────────────────
    case 'electricalGrid':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'capacity_mw'], 0],
            0, 4,
            500, 6,
            1500, 9,
            3000, 13,
            6000, 18,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'facility_type'], 'other'],
            'thermal', '#ff5722',
            'hydro', '#03a9f4',
            'hydro_pumped', '#0288d1',
            'geothermal', '#d84315',
            'wind', '#80deea',
            'solar', '#ffd54f',
            'substation', '#9e9e9e',
            'frequency_converter', '#ab47bc',
            'hvdc_converter', '#7e57c2',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#000',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    // ── Gas network ────────────────────────────────────────────
    case 'gasNetwork':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'capacity_kt'], 0],
            0, 4,
            300, 7,
            1000, 11,
            3000, 16,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'facility_type'], 'other'],
            'lng_terminal', '#ff5722',
            'distribution', '#ff9800',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#000',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    // ── Water infrastructure ───────────────────────────────────
    case 'waterInfra':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'capacity_mcm'], 0],
            0, 4,
            10, 6,
            100, 9,
            500, 13,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'facility_type'], 'other'],
            'dam', '#1e88e5',
            'water_treatment', '#29b6f6',
            'sewage', '#7e57c2',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#000',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    // ── Cell towers ────────────────────────────────────────────
    case 'cellTowers':
      map.addLayer({
        id: `${mainLayerId}-heat`,
        type: 'heatmap',
        source: sourceId,
        paint: {
          'heatmap-weight': 0.4,
          'heatmap-intensity': 1.1,
          'heatmap-color': [
            'interpolate', ['linear'], ['heatmap-density'],
            0, 'rgba(0,0,0,0)',
            0.2, '#1a0033',
            0.4, '#330066',
            0.6, '#660099',
            0.8, '#9c27b0',
            1, '#e040fb',
          ],
          'heatmap-radius': 14,
          'heatmap-opacity': opacity * 0.6,
        },
      });
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 2.5,
          'circle-color': [
            'match', ['coalesce', ['get', 'carrier'], 'other'],
            'NTT Docomo', '#d32f2f',
            'au by KDDI', '#fb8c00',
            'SoftBank', '#9e9e9e',
            'Rakuten Mobile', '#bf360c',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.7,
          'circle-stroke-width': 0.3,
          'circle-stroke-color': '#fff',
          'circle-stroke-opacity': opacity * 0.3,
        },
      });
      break;

    // ── Nuclear facilities ─────────────────────────────────────
    case 'nuclearFacilities':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'capacity_mw'], 0],
            0, 6,
            1000, 10,
            3000, 14,
            8000, 20,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'status'], 'other'],
            'active', '#76ff03',
            'restart_approved', '#cddc39',
            'restart_pending', '#ffeb3b',
            'suspended', '#ff9800',
            'decommissioning', '#9e9e9e',
            'commissioning', '#80d8ff',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffd600',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    // ── EV Charging ────────────────────────────────────────────────
    case 'evCharging':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'power_kw'], 7],
            0, 4, 22, 6, 50, 9, 150, 12, 350, 16,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'connector_type'], 'other'],
            'CHAdeMO', '#1565c0',
            'CCS', '#2e7d32',
            'Type2', '#e65100',
            'Tesla', '#c62828',
            'Type1', '#f9a825',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#fff',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    // ── Airport Infrastructure ───────────────────────────────────────
    case 'airportInfra':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'match', ['coalesce', ['get', 'facility_type'], 'other'],
            'aerodrome', 10,
            'military_base', 9,
            'runway', 6,
            'navaid', 4,
            'control_tower', 5,
            'terminal', 7,
            6,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'facility_type'], 'other'],
            'aerodrome', '#546e7a',
            'military_base', '#c62828',
            'runway', '#78909c',
            'navaid', '#ff8f00',
            'control_tower', '#00838f',
            'terminal', '#5c6bc0',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#fff',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    // ── Port Infrastructure ─────────────────────────────────────────
    case 'portInfra':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'match', ['coalesce', ['get', 'port_class'], 'other'],
            'international_strategic', 14,
            'international_hub', 12,
            'important', 9,
            'local', 6,
            'fishing', 5,
            'ferry_terminal', 7,
            6,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'port_class'], 'other'],
            'international_strategic', '#c62828',
            'international_hub', '#e65100',
            'important', '#f9a825',
            'local', '#66bb6a',
            'fishing', '#00acc1',
            'ferry_terminal', '#7c4dff',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#fff',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    // ── Bridge & Tunnel Infrastructure ──────────────────────────────
    case 'bridgeTunnelInfra':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'length_m'], 500],
            0, 4, 1000, 6, 5000, 9, 15000, 13, 50000, 18,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'facility_type'], 'other'],
            'bridge', '#795548',
            'tunnel', '#546e7a',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#fff',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    case 'famousPlaces':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 7,
          'circle-color': [
            'match', ['coalesce', ['get', 'tourism'], ['get', 'historic'], ['get', 'amenity'], ['get', 'leisure'], ['get', 'natural'], 'place'],
            'attraction', '#d81b60',
            'museum', '#8e24aa',
            'viewpoint', '#00acc1',
            'artwork', '#c2185b',
            'theme_park', '#ec407a',
            'zoo', '#43a047',
            'aquarium', '#039be5',
            'gallery', '#ab47bc',
            'castle', '#5d4037',
            'monument', '#6d4c41',
            'memorial', '#757575',
            'ruins', '#8d6e63',
            'archaeological_site', '#6d4c41',
            'place_of_worship', '#e65100',
            'theatre', '#ad1457',
            'arts_centre', '#7b1fa2',
            'park', '#2e7d32',
            'garden', '#388e3c',
            'nature_reserve', '#1b5e20',
            'peak', '#455a64',
            'volcano', '#bf360c',
            'waterfall', '#0277bd',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.9,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.9,
        },
      });
      break;

    // ── Wave 1: Public Safety + Disaster ──────────────────────────
    case 'hospitalMap':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'beds'], 200],
            0, 4,
            300, 6,
            600, 9,
            1000, 13,
            1500, 18,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'hospital_type'], 'general'],
            'university', '#e53935',
            'national', '#ad1457',
            'specialized', '#f06292',
            'public', '#fb8c00',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    case 'aedMap':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 5,
          'circle-color': [
            'match', ['coalesce', ['get', 'location_type'], 'public'],
            'station', '#ff5252',
            'airport', '#ff8a80',
            'stadium', '#ff1744',
            'tourist', '#f06292',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'kobanMap':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'match', ['coalesce', ['get', 'police_type'], 'station'],
            'headquarters', 12,
            'station', 7,
            'koban', 4,
            'chuzaisho', 4,
            5,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'police_type'], 'station'],
            'headquarters', '#0d47a1',
            'station', '#1565c0',
            'koban', '#42a5f5',
            'chuzaisho', '#90caf9',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    case 'fireStationMap':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'match', ['coalesce', ['get', 'station_type'], 'station'],
            'headquarters', 11,
            'station', 6,
            5,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'station_type'], 'station'],
            'headquarters', '#bf360c',
            'station', '#d84315',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffeb3b',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    case 'bosaiShelter':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'capacity'], 1000],
            0, 4,
            5000, 7,
            20000, 11,
            60000, 16,
            100000, 22,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'shelter_type'], 'designated'],
            'evacuation_area', '#00897b',
            'designated', '#26a69a',
            'tsunami_tower', '#0277bd',
            'tsunami_building', '#039be5',
            'assembly_point', '#4db6ac',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    case 'hazardMapPortal':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'max_depth_m'], ['get', 'alert_level'], 5],
            0, 6,
            5, 10,
            15, 16,
            30, 22,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'hazard_type'], 'flood'],
            'tsunami', '#0277bd',
            'volcano', '#bf360c',
            'landslide', '#6d4c41',
            'flood', '#0288d1',
            'liquefaction', '#8d6e63',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.75,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffeb3b',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'jshisSeismic':
      map.addLayer({
        id: `${mainLayerId}-heat`,
        type: 'heatmap',
        source: sourceId,
        maxzoom: 15,
        paint: {
          'heatmap-weight': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'prob_6lower_30yr'], 0.1],
            0, 0,
            0.5, 0.7,
            1, 1,
          ],
          'heatmap-intensity': ['interpolate', ['linear'], ['zoom'], 0, 1, 12, 3],
          'heatmap-color': [
            'interpolate', ['linear'], ['heatmap-density'],
            0, 'rgba(33, 102, 172, 0)',
            0.2, '#3288bd',
            0.4, '#fee08b',
            0.6, '#f46d43',
            0.8, '#d73027',
            1, '#a50026',
          ],
          'heatmap-radius': ['interpolate', ['linear'], ['zoom'], 0, 20, 12, 60],
          'heatmap-opacity': opacity * 0.7,
        },
      });
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        minzoom: 7,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'prob_6lower_30yr'], 0],
            0, 4,
            0.3, 7,
            0.6, 12,
            0.9, 18,
          ],
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'prob_6lower_30yr'], 0],
            0, '#3288bd',
            0.3, '#fee08b',
            0.6, '#f46d43',
            0.9, '#a50026',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.4,
        },
      });
      break;

    case 'hiNet':
    case 'kNet':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 5,
          'circle-color': layerDef.color,
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    case 'jmaIntensity':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'intensity_numeric'], ['get', 'magnitude'], 5],
            0, 5,
            4, 9,
            6, 16,
            7, 24,
          ],
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'intensity_numeric'], ['get', 'magnitude'], 5],
            0, '#ffeb3b',
            3, '#ff9800',
            5, '#ff5722',
            6, '#d84315',
            7, '#b71c1c',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    // ── Wave 2: Health + Statistics + Commerce ─────────────────────
    case 'pharmacyMap':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 5,
          'circle-color': '#26a69a',
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    case 'convenienceStores':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 4,
          'circle-color': [
            'match', ['get', 'brand'],
            '7-Eleven', '#ff6b35',
            'FamilyMart', '#1a73e8',
            'Lawson', '#0077c8',
            'MiniStop', '#f1c232',
            'NewDays', '#43a047',
            '#43a047',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    case 'gasStations':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 5,
          'circle-color': [
            'match', ['get', 'brand'],
            'ENEOS', '#e60012',
            'Idemitsu', '#ee7800',
            'Cosmo', '#003da5',
            'Shell', '#fbce07',
            'JA-SS', '#43a047',
            '#e64a19',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    case 'tabelogRestaurants':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'rating'], 3.5],
            3.0, 4,
            4.0, 7,
            4.5, 10,
            5.0, 14,
          ],
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'rating'], 3.5],
            3.0, '#ffcc80',
            4.0, '#fb8c00',
            4.5, '#e65100',
            5.0, '#bf360c',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    case 'resasTourism':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'visitors_yr'], 1000000],
            500000, 4,
            5000000, 9,
            15000000, 16,
            100000000, 28,
          ],
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'foreign_pct'], 20],
            5, '#80deea',
            25, '#00acc1',
            50, '#006064',
            70, '#311b92',
          ],
          'circle-opacity': opacity * 0.75,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    case 'resasIndustry':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'employees'], 100000],
            10000, 5,
            100000, 10,
            500000, 18,
            4500000, 30,
          ],
          'circle-color': [
            'match', ['get', 'primary_sector'],
            'manufacturing', '#6d4c41',
            'services', '#1e88e5',
            'agriculture', '#7cb342',
            'fishery', '#0097a7',
            '#9e9e9e',
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    case 'mlitTransaction':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'transactions_q'], 200],
            50, 4,
            200, 8,
            300, 12,
          ],
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'avg_price_yen_per_m2'], 1500000],
            500000, '#a5d6a7',
            1500000, '#66bb6a',
            3500000, '#fb8c00',
            6500000, '#b71c1c',
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    case 'damWaterLevel':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'capacity_m3'], 50000000],
            10000000, 5,
            100000000, 10,
            300000000, 16,
            660000000, 24,
          ],
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'current_pct'], 70],
            0, '#b71c1c',
            40, '#fb8c00',
            70, '#42a5f5',
            90, '#0277bd',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    // ── Wave 3: Maritime + Ocean + Aviation ────────────────────────
    case 'jmaOceanWave':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'wave_height_m'], 1.5],
            0.5, 4,
            1.5, 8,
            2.5, 14,
            4.0, 22,
          ],
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'wave_height_m'], 1.5],
            0.5, '#80deea',
            1.5, '#0288d1',
            2.5, '#01579b',
            4.0, '#311b92',
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    case 'jmaOceanTemp':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 9,
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'sst_c'], 18],
            5, '#0d47a1',
            12, '#039be5',
            18, '#fdd835',
            24, '#fb8c00',
            28, '#b71c1c',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'anomaly_c'], 0],
            0, 1,
            2, 2,
            3, 3,
          ],
          'circle-stroke-color': '#000000',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'jmaTide':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'level_cm'], 100],
            70, 4,
            100, 7,
            130, 12,
          ],
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'anomaly_cm'], 0],
            -10, '#0d47a1',
            0, '#0288d1',
            5, '#26a69a',
            10, '#fb8c00',
            15, '#b71c1c',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    case 'nowphasWave':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'wave_height_m'], 1.5],
            0.5, 4,
            1.5, 8,
            2.5, 14,
          ],
          'circle-color': [
            'match', ['get', 'sensor_type'],
            'gps_buoy', '#1565c0',
            'ultrasonic', '#0288d1',
            '#01579b',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'lighthouseMap':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'range_km'], 25],
            15, 4,
            25, 7,
            40, 12,
          ],
          'circle-color': [
            'case',
            ['==', ['get', 'historic'], true], '#fdd835',
            '#ffeb3b',
          ],
          'circle-opacity': opacity * 0.9,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#212121',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'jarticTraffic':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'congestion_km'], 3],
            1, 5,
            4, 10,
            8, 18,
          ],
          'circle-color': [
            'match', ['get', 'level'],
            'moderate', '#fdd835',
            'heavy', '#fb8c00',
            'severe', '#b71c1c',
            '#9e9e9e',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    case 'droneNofly':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'radius_km'], 5],
            1, 5,
            5, 10,
            9, 16,
            25, 30,
          ],
          'circle-color': [
            'match', ['get', 'restriction'],
            'absolute', '#b71c1c',
            'permit', '#fb8c00',
            '#9e9e9e',
          ],
          'circle-opacity': opacity * 0.35,
          'circle-stroke-width': 2,
          'circle-stroke-color': [
            'match', ['get', 'restriction'],
            'absolute', '#b71c1c',
            'permit', '#fb8c00',
            '#9e9e9e',
          ],
          'circle-stroke-opacity': opacity * 0.85,
        },
      });
      break;

    case 'jcgPatrol':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'vessels_count'], 5],
            3, 5,
            10, 9,
            18, 14,
          ],
          'circle-color': [
            'match', ['get', 'base_type'],
            'rcgh', '#00695c',
            'office', '#26a69a',
            '#00838f',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    // Wave 4: Government + Defense
    case 'governmentBuildings':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'match', ['get', 'kind'],
            'cabinet', 12, 'parliament', 12, 'imperial', 12,
            'ministry', 8, 'judiciary', 9, 'central_bank', 9,
            'agency', 6, 'prefectural', 7,
            5,
          ],
          'circle-color': [
            'match', ['get', 'kind'],
            'cabinet', '#d32f2f', 'parliament', '#c2185b', 'imperial', '#ad1457',
            'ministry', '#6a1b9a', 'agency', '#7b1fa2', 'judiciary', '#4527a0',
            'prefectural', '#5e35b2', 'central_bank', '#1565c0',
            '#6a1b9a',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'cityHalls':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'population'], 100000],
            50000, 4,
            500000, 7,
            2000000, 11,
            4000000, 14,
          ],
          'circle-color': [
            'match', ['get', 'kind'],
            'designated_city', '#4a148c',
            'core_city', '#7b1fa2',
            'ward', '#9c27b0',
            'townhall', '#ba68c8',
            '#7b1fa2',
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'courtsPrisons':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'match', ['get', 'type'],
            'high_court', 9,
            'district_court', 6,
            'courthouse', 5,
            'prison', 8,
            'detention', 9,
            'medical_prison', 7,
            'juvenile_prison', 7,
            6,
          ],
          'circle-color': [
            'match', ['get', 'type'],
            'high_court', '#311b92',
            'district_court', '#4527a0',
            'courthouse', '#5e35b2',
            'prison', '#b71c1c',
            'detention', '#c62828',
            'medical_prison', '#ad1457',
            'juvenile_prison', '#d84315',
            '#4527a0',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'embassies':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 5,
          'circle-color': '#1565c0',
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.8,
        },
      });
      break;

    case 'jsdfBases':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'match', ['get', 'role'],
            'army_hq', 12, 'mod_hq', 14, 'air_hq', 12, 'fleet_hq', 12,
            'division', 9, 'fleet', 9, 'air_base', 9,
            'brigade', 8, 'air_station', 8, 'fighter', 9,
            'regiment', 6, 'aviation', 7, 'training', 6,
            'airborne', 8, 'tank', 8, 'csbrn', 7, 'engineer', 6,
            'logistics', 5, 'coastal', 7, 'training_range', 5,
            6,
          ],
          'circle-color': [
            'match', ['get', 'branch'],
            'GSDF', '#33691e',
            'MSDF', '#0d47a1',
            'ASDF', '#01579b',
            '#33691e',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'usfjBases':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'match', ['get', 'role'],
            'air_hq', 13, 'army_hq', 13, 'fleet_hq', 13, 'hq', 12,
            'fighter', 10, 'air_station', 9,
            'fleet', 10, 'port', 8, 'depot', 7,
            'training', 6, 'cmd', 8, 'support', 6,
            'logistics', 6, 'medical', 6, 'comms', 6,
            'housing', 5, 'army', 7, 'aux_field', 7,
            6,
          ],
          'circle-color': [
            'match', ['get', 'branch'],
            'USAF', '#0d47a1',
            'USN', '#1a237e',
            'USMC', '#bf360c',
            'USA', '#1b5e20',
            '#1a237e',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 2,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.8,
        },
      });
      break;

    case 'radarSites':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'match', ['get', 'type'],
            'bmd_xband', 12,
            'air_defense', 8,
            'weather', 6,
            6,
          ],
          'circle-color': [
            'match', ['get', 'type'],
            'bmd_xband', '#d50000',
            'air_defense', '#bf360c',
            'weather', '#0277bd',
            '#bf360c',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 2,
          'circle-stroke-color': '#ffeb3b',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'coastGuardStations':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'match', ['get', 'kind'],
            'office', 7,
            'station', 5,
            6,
          ],
          'circle-color': '#0277bd',
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    // Wave 5: Industry + Energy Deep
    case 'autoPlants':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'employees'], 1500],
            1000, 4,
            5000, 8,
            10000, 12,
          ],
          'circle-color': [
            'match', ['get', 'brand'],
            'Toyota', '#e53935', 'Lexus', '#b71c1c',
            'Nissan', '#1565c0', 'Honda', '#00838f',
            'Mazda', '#5d4037', 'Subaru', '#0277bd',
            'Mitsubishi', '#c62828', 'Suzuki', '#1976d2',
            'Daihatsu', '#d84315', 'Isuzu', '#388e3c',
            'Hino', '#558b2f', 'Kawasaki', '#33691e',
            'Yamaha', '#283593',
            '#d84315',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'steelMills':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'capacity_mt_yr'], 1],
            0, 5,
            5, 9,
            12, 14,
          ],
          'circle-color': [
            'match', ['get', 'company'],
            'Nippon Steel', '#5d4037',
            'JFE', '#3e2723',
            'Kobelco', '#8d6e63',
            'Daido', '#6d4c41',
            'Tokyo Steel', '#a1887f',
            'Nisshin', '#795548',
            '#5d4037',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'petrochemical':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'ethylene_capacity_kt_yr'], 200],
            0, 6,
            1000, 10,
            3500, 16,
          ],
          'circle-color': '#6a1b9a',
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'refineries':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'capacity_bpd'], 100000],
            50000, 5,
            150000, 9,
            300000, 13,
            400000, 16,
          ],
          'circle-color': [
            'match', ['get', 'company'],
            'ENEOS', '#ff6f00',
            'Idemitsu', '#e65100',
            'Cosmo', '#bf360c',
            'Showa Yokkaichi', '#d84315',
            'Toa Oil', '#f57c00',
            'Fuji Oil', '#fb8c00',
            'Taiyo Oil', '#ef6c00',
            'Seibu Oil', '#e65100',
            'Kashima Oil', '#bf360c',
            '#e65100',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'semiconductorFabs':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'wafer_size_mm'], 200],
            100, 5,
            200, 7,
            300, 11,
          ],
          'circle-color': [
            'match', ['get', 'company'],
            'Kioxia', '#1565c0',
            'Sony', '#00838f',
            'Renesas', '#283593',
            'Rohm', '#1976d2',
            'Mitsubishi Electric', '#c62828',
            'Fuji Electric', '#0277bd',
            'TSMC JASM', '#d32f2f',
            'Rapidus', '#7b1fa2',
            'Micron Japan', '#1565c0',
            'WD/Kioxia', '#00897b',
            '#1565c0',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'shipyards':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'max_dwt'], 50000],
            30000, 5,
            150000, 9,
            300000, 14,
          ],
          'circle-color': [
            'match', ['get', 'kind'],
            'commercial_navy', '#33691e',
            'navy', '#1b5e20',
            'submarine', '#d50000',
            'commercial', '#00695c',
            '#00695c',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'petroleumStockpile':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'capacity_kl'], 500000],
            500000, 6,
            2000000, 10,
            6000000, 16,
          ],
          'circle-color': [
            'match', ['get', 'kind'],
            'national', '#bf360c',
            'national_lpg', '#f57c00',
            'commercial', '#a1887f',
            '#bf360c',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 2,
          'circle-stroke-color': '#ffeb3b',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'windTurbines':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'capacity_mw'], 10],
            5, 4,
            30, 7,
            100, 11,
            150, 14,
          ],
          'circle-color': [
            'match', ['get', 'kind'],
            'offshore_floating', '#01579b',
            'offshore', '#0277bd',
            'onshore', '#43a047',
            '#43a047',
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'dataCenters':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'tier'], 3],
            2, 5,
            3, 7,
            4, 11,
          ],
          'circle-color': [
            'match', ['get', 'operator'],
            'Equinix', '#d50000',
            'NTT', '#1565c0',
            'KDDI', '#ef6c00',
            'IIJ', '#6a1b9a',
            'AWS', '#ff9900',
            'Google', '#4285f4',
            'Microsoft', '#00a4ef',
            'IDC Frontier', '#388e3c',
            'SAKURA', '#e91e63',
            '#00838f',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'internetExchanges':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'members'], 30],
            5, 5,
            50, 9,
            200, 14,
            400, 18,
          ],
          'circle-color': [
            'match', ['get', 'operator'],
            'JPNAP', '#00acc1',
            'JPIX', '#0288d1',
            'BBIX', '#7e57c2',
            'Equinix', '#d50000',
            'DIX-IE', '#43a047',
            '#00acc1',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 2,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'submarineCables':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['zoom'], 4, 5, 8, 9, 12, 13,
          ],
          'circle-color': [
            'match', ['get', 'operator'],
            'NTT', '#1565c0',
            'KDDI', '#ef6c00',
            'SoftBank', '#9e9e9e',
            'Google', '#4285f4',
            'Facebook/Meta', '#1877f2',
            '#01579b',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 2,
          'circle-stroke-color': '#ffeb3b',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    case 'torExitNodes':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'bandwidth_mbps'], 50],
            10, 4,
            100, 7,
            500, 11,
            1500, 15,
          ],
          'circle-color': '#7b1fa2',
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#e1bee7',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'coverage5g':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'cells'], 100],
            10, 5,
            100, 8,
            500, 12,
            2000, 17,
          ],
          'circle-color': [
            'match', ['get', 'operator'],
            'NTT Docomo', '#d50000',
            'KDDI', '#ef6c00',
            'SoftBank', '#9e9e9e',
            'Rakuten Mobile', '#bf360c',
            '#e91e63',
          ],
          'circle-opacity': opacity * 0.7,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    case 'satelliteGroundStations':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'match', ['get', 'kind'],
            'deep_space', 14,
            'launch', 13,
            'launch_tracking', 12,
            'mission_control', 12,
            'commercial_satcom', 10,
            'satcom', 9,
            'leo_gateway', 9,
            'vlbi', 8,
            'tt&c', 8,
            'tracking', 7,
            7,
          ],
          'circle-color': [
            'match', ['get', 'operator'],
            'JAXA', '#ff5722',
            'NICT', '#9c27b0',
            'KDDI', '#ef6c00',
            'SKY Perfect JSAT', '#0288d1',
            'NAOJ', '#1a237e',
            'SoftBank', '#9e9e9e',
            'Mitsubishi Electric', '#37474f',
            '#ffa726',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 2,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

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

    case 'amateurRadioRepeaters':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'match', ['get', 'mode'],
            'D-STAR', 8,
            'C4FM', 8,
            'DMR', 8,
            'CW beacon', 9,
            'SSB/CW', 9,
            'FM', 6,
            6,
          ],
          'circle-color': [
            'match', ['get', 'mode'],
            'D-STAR', '#1565c0',
            'C4FM', '#6a1b9a',
            'DMR', '#00838f',
            'CW beacon', '#ef6c00',
            'SSB/CW', '#d84315',
            'FM', '#8d6e63',
            '#8d6e63',
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.6,
        },
      });
      break;

    // ── Wave 7: Tourism + Culture ─────────────────────────────────
    case 'nationalParks':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'area_ha'], 30000],
            5000, 8, 30000, 14, 150000, 20,
          ],
          'circle-color': [
            'match', ['get', 'kind'],
            'national', '#2e7d32',
            'quasi_national', '#66bb6a',
            'protected_area', '#81c784',
            '#2e7d32',
          ],
          'circle-opacity': opacity * 0.65,
          'circle-stroke-width': 2,
          'circle-stroke-color': '#1b5e20',
          'circle-stroke-opacity': opacity * 0.9,
        },
      });
      break;

    case 'unescoHeritage':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 11,
          'circle-color': [
            'match', ['get', 'category'],
            'cultural', '#ffa000',
            'natural', '#2e7d32',
            'mixed', '#ff6f00',
            '#ffa000',
          ],
          'circle-opacity': opacity * 0.9,
          'circle-stroke-width': 3,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity,
        },
      });
      break;

    case 'castles':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 8,
          'circle-color': [
            'match', ['get', 'era'],
            'Edo', '#5d4037',
            'Sengoku', '#8d6e63',
            'Kamakura', '#795548',
            'Ryukyu', '#ef6c00',
            'Asuka', '#6d4c41',
            'Nara', '#6d4c41',
            'Yayoi', '#4e342e',
            '#8d6e63',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 2,
          'circle-stroke-color': '#3e2723',
          'circle-stroke-opacity': opacity * 0.8,
        },
      });
      break;

    case 'museums':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 7,
          'circle-color': layerDef.color,
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.8,
        },
      });
      break;

    case 'stadiums':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'capacity'], 20000],
            5000, 6, 30000, 10, 70000, 16,
          ],
          'circle-color': [
            'match', ['get', 'kind'],
            'baseball', '#d32f2f',
            'football', '#2e7d32',
            'multipurpose', '#1565c0',
            'rugby', '#f9a825',
            'sumo', '#880e4f',
            '#00897b',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 2,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.8,
        },
      });
      break;

    case 'racetracks':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'capacity'], 15000],
            5000, 6, 30000, 10, 200000, 18,
          ],
          'circle-color': [
            'match', ['get', 'sport'],
            'horse_jra', '#006400',
            'horse_nar', '#228b22',
            'keirin', '#1565c0',
            'kyotei', '#0277bd',
            'auto_race', '#c62828',
            '#c2185b',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 2,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.8,
        },
      });
      break;

    case 'shrineTemple':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 6,
          'circle-color': [
            'match', ['get', 'religion'],
            'shinto', '#d32f2f',
            'buddhist', '#ffa000',
            '#b71c1c',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.7,
        },
      });
      break;

    case 'onsenMap':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'est_visitors_yr'], 300000],
            100000, 5, 1000000, 10, 5000000, 16,
          ],
          'circle-color': layerDef.color,
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 2,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.75,
        },
      });
      break;

    case 'skiResorts':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'lifts'], 8],
            3, 5, 12, 9, 25, 14,
          ],
          'circle-color': layerDef.color,
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 2,
          'circle-stroke-color': '#e3f2fd',
          'circle-stroke-opacity': opacity * 0.9,
        },
      });
      break;

    case 'animePilgrimage':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 6,
          'circle-color': layerDef.color,
          'circle-opacity': opacity * 0.9,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.8,
        },
      });
      break;

    // ── Wave 8: Crime + Vice + Wildlife ─────────────────────────────
    case 'yakuzaHq':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'members_est'], 100],
            0, 5, 500, 9, 2000, 14, 4000, 18,
          ],
          'circle-color': [
            'match',
            ['get', 'designation'],
            'tokutei', '#b71c1c',
            'shitei', '#6a1b9a',
            'shitei_sub', '#8e24aa',
            'defunct', '#616161',
            '#7b1fa2',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 2,
          'circle-stroke-color': '#fff8e1',
          'circle-stroke-opacity': opacity * 0.9,
        },
      });
      break;

    case 'redLightZones':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'est_establishments'], 50],
            0, 4, 500, 8, 2000, 14, 4500, 20,
          ],
          'circle-color': [
            'match',
            ['get', 'type'],
            'red_light_mixed', '#d81b60',
            'soapland_zone', '#c2185b',
            'hostess_zone', '#ec407a',
            'nightclub_zone', '#ab47bc',
            'geisha_quarter', '#ad1457',
            'love_hotel_zone', '#e91e63',
            'historic_red_light', '#795548',
            '#d81b60',
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#fce4ec',
          'circle-stroke-opacity': opacity * 0.85,
        },
      });
      break;

    case 'pachinkoDensity':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'parlor_count'], 10],
            0, 5, 30, 10, 60, 16,
          ],
          'circle-color': layerDef.color,
          'circle-opacity': opacity * 0.75,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#ffffff',
          'circle-stroke-opacity': opacity * 0.85,
        },
      });
      break;

    case 'wantedPersons':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'open_cases'], 30],
            0, 5, 100, 9, 250, 14, 450, 20,
          ],
          'circle-color': layerDef.color,
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 2,
          'circle-stroke-color': '#ffebee',
          'circle-stroke-opacity': opacity * 0.9,
        },
      });
      break;

    case 'phoneScamHotspots':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'incidents_yr'], 100],
            0, 5, 200, 10, 400, 15, 600, 20,
          ],
          'circle-color': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'damage_yen'], 300000000],
            0, '#ffcc80',
            500000000, '#ff7043',
            1200000000, '#d84315',
          ],
          'circle-opacity': opacity * 0.8,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#fff3e0',
          'circle-stroke-opacity': opacity * 0.85,
        },
      });
      break;

    // ── Wave 9: Food + Agriculture ─────────────────────────────────
    case 'sakeBreweries':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'annual_production_koku'], 5000],
            0, 5, 50000, 9, 250000, 14, 750000, 20,
          ],
          'circle-color': layerDef.color,
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#e8eaf6',
          'circle-stroke-opacity': opacity * 0.9,
        },
      });
      break;

    case 'wineriesCraftbeer':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 6,
          'circle-color': [
            'match',
            ['get', 'category'],
            'winery', '#880e4f',
            'craft_beer', '#ef6c00',
            'whisky', '#6d4c41',
            '#ad1457',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#fce4ec',
          'circle-stroke-opacity': opacity * 0.9,
        },
      });
      break;

    case 'fishMarkets':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'throughput_tpy'], 10000],
            0, 5, 60000, 9, 200000, 14, 500000, 20,
          ],
          'circle-color': [
            'match',
            ['get', 'kind'],
            'central_wholesale', '#01579b',
            'port_market', '#0277bd',
            'local_wholesale', '#0288d1',
            'tourist_morning_market', '#29b6f6',
            'tourist_wholesale', '#4fc3f7',
            '#0288d1',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#e1f5fe',
          'circle-stroke-opacity': opacity * 0.9,
        },
      });
      break;

    case 'wagyuRanches':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'head_count'], 5000],
            0, 5, 10000, 9, 50000, 14, 120000, 20,
          ],
          'circle-color': [
            'match',
            ['get', 'tier'],
            'premium', '#3e2723',
            'regional', '#6d4c41',
            'rare_breed', '#8d6e63',
            'heritage', '#bf360c',
            '#6d4c41',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 2,
          'circle-stroke-color': '#efebe9',
          'circle-stroke-opacity': opacity * 0.9,
        },
      });
      break;

    case 'teaZones':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'production_t'], 500],
            0, 5, 1000, 9, 3000, 13, 6000, 18,
          ],
          'circle-color': [
            'match',
            ['get', 'variety'],
            'matcha', '#1b5e20',
            'matcha_gyokuro', '#2e7d32',
            'gyokuro', '#388e3c',
            'sencha', '#43a047',
            'sencha_fukamushi', '#4caf50',
            'kabusecha', '#66bb6a',
            'kamairicha', '#81c784',
            'hojicha', '#8d6e63',
            'tamaryokucha', '#7cb342',
            'traditional_gyokuro', '#1b5e20',
            'organic_sencha', '#689f38',
            '#43a047',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#e8f5e9',
          'circle-stroke-opacity': opacity * 0.9,
        },
      });
      break;

    case 'ricePaddies':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'production_t'], 10000],
            0, 5, 50000, 10, 100000, 14, 200000, 20,
          ],
          'circle-color': [
            'match',
            ['get', 'grade'],
            'special_a', '#558b2f',
            'a', '#7cb342',
            'terraced', '#ffa726',
            'terraced_100', '#ff9800',
            '#9ccc65',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#f1f8e9',
          'circle-stroke-opacity': opacity * 0.9,
        },
      });
      break;

    // ── Wave 10: Niche + Pop Culture ─────────────────────────────
    case 'vendingMachines':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'vm_count_est'], 50],
            0, 4, 50, 7, 150, 11, 300, 16, 400, 20,
          ],
          'circle-color': [
            'match',
            ['get', 'zone_type'],
            'transit_hub', '#e91e63',
            'commercial', '#ec407a',
            'entertainment', '#d81b60',
            'tourist', '#f06292',
            'market', '#ad1457',
            'remote', '#880e4f',
            '#e91e63',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#fce4ec',
          'circle-stroke-opacity': opacity * 0.9,
        },
      });
      map.addLayer({
        id: `${mainLayerId}-heat`,
        type: 'heatmap',
        source: sourceId,
        paint: {
          'heatmap-weight': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'vm_count_est'], 50],
            0, 0, 400, 1,
          ],
          'heatmap-intensity': 0.8,
          'heatmap-radius': 30,
          'heatmap-opacity': opacity * 0.5,
        },
      });
      break;

    case 'karaokeChains':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'rooms'], 40],
            0, 5, 30, 7, 50, 10, 70, 14,
          ],
          'circle-color': [
            'match',
            ['get', 'brand'],
            'Big Echo', '#c2185b',
            'Karaoke-kan', '#e91e63',
            'JOYSOUND', '#7b1fa2',
            'Club DAM', '#512da8',
            'Manekineko', '#d81b60',
            'Jankara', '#ad1457',
            'Shidax', '#455a64',
            'Uta Hiroba', '#6a1b9a',
            'Banban', '#ec407a',
            "Cote d'Azur", '#880e4f',
            'Karaoke Mac', '#9c27b0',
            '#d81b60',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#fce4ec',
          'circle-stroke-opacity': opacity * 0.9,
        },
      });
      break;

    case 'mangaNetCafes':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'seats'], 150],
            0, 5, 100, 8, 180, 11, 220, 14,
          ],
          'circle-color': [
            'match',
            ['get', 'brand'],
            'Jiyu-kukan', '#7b1fa2',
            'Media Cafe Popeye', '#8e24aa',
            'Bagus', '#6a1b9a',
            'DiCE', '#9c27b0',
            'Manboo!', '#ab47bc',
            'Aprecio', '#5e35b1',
            'Customa Cafe', '#4527a0',
            '#8e24aa',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#f3e5f5',
          'circle-stroke-opacity': opacity * 0.9,
        },
      });
      break;

    case 'sentoPublicBaths':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'case',
            ['==', ['get', 'landmark'], true], 10,
            6,
          ],
          'circle-color': [
            'match',
            ['get', 'style'],
            'miyazukuri', '#006064',
            'machiya', '#00838f',
            'kuroyu', '#212121',
            'modern', '#0097a7',
            'super_sento', '#00acc1',
            'standard', '#26c6da',
            '#0097a7',
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#e0f7fa',
          'circle-stroke-opacity': opacity * 0.9,
        },
      });
      break;

    case 'themedCafes':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 7,
          'circle-color': [
            'match',
            ['get', 'theme'],
            'maid', '#ec407a',
            'cat', '#ff7043',
            'owl', '#8d6e63',
            'hedgehog', '#a1887f',
            'rabbit', '#f8bbd0',
            'capybara', '#6d4c41',
            'dog', '#ffb74d',
            'reptile', '#66bb6a',
            'beetle', '#33691e',
            'exotic', '#ab47bc',
            'character', '#42a5f5',
            'gaming', '#5c6bc0',
            'anime', '#7e57c2',
            'robot', '#78909c',
            'butler', '#455a64',
            'concept', '#ef5350',
            '#f06292',
          ],
          'circle-opacity': opacity * 0.9,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#fce4ec',
          'circle-stroke-opacity': opacity,
        },
      });
      break;

    // ── Wave 11: External Mapping Platforms ────────────────────
    case 'marineTraffic':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'length_m'], 50],
            0, 3, 50, 5, 150, 8, 300, 12, 400, 15,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'vessel_type'], 'other'],
            'cargo', '#01579b',
            'tanker', '#ef6c00',
            'passenger', '#7c4dff',
            'fishing', '#558b2f',
            'harbour', '#37474f',
            'port', '#263238',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 0.5,
          'circle-stroke-color': '#fff',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;

    case 'vesselFinder':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': [
            'interpolate', ['linear'],
            ['coalesce', ['get', 'length_m'], 50],
            0, 3, 50, 5, 150, 8, 300, 12, 400, 15,
          ],
          'circle-color': [
            'match', ['coalesce', ['get', 'vessel_type'], 'other'],
            'passenger', '#7c4dff',
            'ferry_terminal', '#0288d1',
            'cargo', '#01579b',
            'tanker', '#ef6c00',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.85,
          'circle-stroke-width': 0.5,
          'circle-stroke-color': '#fff',
          'circle-stroke-opacity': opacity * 0.5,
        },
      });
      break;


    case 'googleMyMaps':
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 6,
          'circle-color': [
            'match', ['coalesce', ['get', 'category'], 'other'],
            'landmark', '#ea4335',
            'temple', '#d81b60',
            'shrine', '#b71c1c',
            'castle', '#6d4c41',
            'park', '#2e7d32',
            'nature', '#388e3c',
            'onsen', '#ef6c00',
            'market', '#fb8c00',
            'district', '#7b1fa2',
            'memorial', '#455a64',
            'theme_park', '#f06292',
            'aquarium', '#0288d1',
            'garden', '#43a047',
            'village', '#8d6e63',
            'island', '#00acc1',
            'canal', '#0097a7',
            layerDef.color,
          ],
          'circle-opacity': opacity * 0.9,
          'circle-stroke-width': 1.5,
          'circle-stroke-color': '#fff',
          'circle-stroke-opacity': opacity,
        },
      });
      break;

    default:
      map.addLayer({
        id: mainLayerId,
        type: 'circle',
        source: sourceId,
        paint: {
          'circle-radius': 6,
          'circle-color': layerDef.color,
          'circle-opacity': opacity * 0.8,
        },
      });
  }
}

// Enumerate every MapLibre layer ID on the map that belongs to an
// interactive feature collection — the hit targets for hover/click.
//
// Covers:
//   layer-<id>                              main layer
//   layer-<id>-(heat|extrude|line|fallback) variants from the renderer switch
//   layer-<id>-ring[1-4]                    unified-transport station rings
//   layer-flightAdsb(-mil)?-b<num>          aircraft altitude-bucket sprites
//   live-vehicles-(train|subway|bus)-layer  live-vehicle sprites (added by
//                                           the vehicles effect, outside
//                                           addLayerToMap)
//
// We walk map.getStyle().layers once and match by ID. That's cheaper than
// the old Object.keys(LAYER_DEFINITIONS) × suffix cross-product and — more
// importantly — catches the aircraft bucket layers whose suffixes weren't
// in the original hardcoded list, leaving every plane un-clickable.
function collectInteractiveLayerIds(map) {
  const out = [];
  const style = map.getStyle?.();
  const all = style?.layers || [];
  for (const l of all) {
    const id = l.id;
    if (!id) continue;
    if (id.startsWith('layer-')) out.push(id);
    else if (id.startsWith('live-vehicles-') && id.endsWith('-layer')) out.push(id);
  }
  return out;
}

function removeLayerFromMap(map, layerId) {
  const mainLayerId = `layer-${layerId}`;
  const sourceId = `source-${layerId}`;
  const variants = [
    mainLayerId,
    `${mainLayerId}-heat`,
    `${mainLayerId}-extrude`,
    `${mainLayerId}-line`,
    `${mainLayerId}-label`,
    `${mainLayerId}-fallback`,
    `${mainLayerId}-ring1`,
    `${mainLayerId}-ring2`,
    `${mainLayerId}-ring3`,
    `${mainLayerId}-ring4`,
    `${mainLayerId}-dropline`,
  ];

  for (const lid of variants) {
    if (map.getLayer(lid)) map.removeLayer(lid);
  }
  // Aircraft: altitude-bucket icon sub-layers.
  if (layerId === 'flightAdsb' && map.getStyle) {
    const allLayers = (map.getStyle().layers || []).map((l) => l.id);
    for (const id of allLayers) {
      if (
        id.startsWith(`${mainLayerId}-b`) ||
        id.startsWith(`${mainLayerId}-mil-b`)
      ) {
        map.removeLayer(id);
      }
    }
  }
  unregisterDroplineLayer(`${mainLayerId}-dropline`);
  if (map.getSource(sourceId)) map.removeSource(sourceId);
  clearLayerAddons(layerId);
}

export default function MapView({ layers, layerData, onFeatureClick, onMapReady }) {
  const mapContainerRef = useRef(null);
  const mapRef = useRef(null);
  const [mapReady, setMapReady] = useState(false);
  const [cursorCoords, setCursorCoords] = useState(null);
  const [zoom, setZoom] = useState(5);
  const [currentStyle, setCurrentStyle] = useState('carto_dark_matter');
  const [viewport, setViewport] = useState(null);
  const prevLayersRef = useRef({});
  const bakedSceneIdsRef = useRef(new Set());
  const satelliteImageryVisibleRef = useRef(false);
  // Single deck.gl MapboxOverlay shared by all client-rendered overlays
  // (currently just PLATEAU 3D buildings). Created lazily on first use,
  // re-used across toggle/style-switch cycles, never recreated.
  const deckOverlayRef = useRef(null);
  // Two independent sources of overlay layers — PLATEAU 3D buildings and
  // re-channeled basemap labels. Each effect publishes into its slot via
  // pushDeckLayers(); the reconciler concatenates and pushes once.
  const deckLayerSlotsRef = useRef({ plateau: [], labels: [] });
  const pushDeckLayers = useCallback((slot, layers) => {
    deckLayerSlotsRef.current[slot] = layers || [];
    if (!deckOverlayRef.current) return;
    const merged = [
      ...deckLayerSlotsRef.current.plateau,
      ...deckLayerSlotsRef.current.labels,
    ];
    try { deckOverlayRef.current.setProps({ layers: merged }); } catch { /* overlay torn down */ }
  }, []);

  // Live vehicles ride along with the standard transport layer — no separate
  // toggle. When the user enables "Trains", moving dots spawn on train routes
  // (and similarly for subways/buses). Slice C: the same hook also polls
  // /api/transit/active-trips using the viewport bbox, merging schedule-backed
  // positions on top.
  const liveTrainsGeo = useLiveVehicles('train', !!layers?.unifiedTrains?.visible, viewport);
  const liveSubwaysGeo = useLiveVehicles('subway', !!layers?.unifiedSubways?.visible, viewport);
  const liveBusesGeo = useLiveVehicles('bus', !!layers?.unifiedBuses?.visible, viewport);

  const coloredSatelliteTrackingFc = useMemo(() => {
    const fc = layerData?.satelliteTracking;
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
  }, [layerData?.satelliteTracking]);

  const satelliteTrackFc = useSatelliteTracks(
    layers?.satelliteTracking?.visible ? coloredSatelliteTrackingFc : null
  );

  // Initialize map
  useEffect(() => {
    if (mapRef.current || !mapContainerRef.current) return;

    const map = new maplibregl.Map({
      container: mapContainerRef.current,
      style: MAP_STYLES[currentStyle].style,
      center: [138, 36.5],
      zoom: 5,
      maxBounds: [[120, 20], [155, 50]],
      attributionControl: true,
    });

    map.addControl(new maplibregl.NavigationControl({ showCompass: true }), 'top-right');
    map.addControl(new maplibregl.ScaleControl({ maxWidth: 150 }), 'bottom-right');

    const publishViewport = () => {
      try {
        const b = map.getBounds();
        setViewport({
          minLng: b.getWest(),
          minLat: b.getSouth(),
          maxLng: b.getEast(),
          maxLat: b.getNorth(),
        });
      } catch { /* map not ready yet */ }
    };

    map.on('load', () => {
      registerLayerIcons(map);
      setMapReady(true);
      publishViewport();
    });

    // Updated on pan/zoom settle so the hooks that key on viewport
    // (useLiveVehicles Slice C poll) re-request with the current bbox.
    map.on('moveend', publishViewport);

    map.on('mousemove', (e) => {
      setCursorCoords({ lat: e.lngLat.lat.toFixed(5), lng: e.lngLat.lng.toFixed(5) });

      // Change cursor on hover over interactive features
      const interactiveLayers = collectInteractiveLayerIds(map);
      const features = interactiveLayers.length > 0
        ? map.queryRenderedFeatures(e.point, { layers: interactiveLayers })
        : [];
      map.getCanvas().style.cursor = features.length > 0 ? 'pointer' : 'default';
    });

    map.on('zoom', () => {
      setZoom(map.getZoom().toFixed(1));
    });

    map.on('pitch', () => {
      applyDroplineFade(map);
    });

    map.on('click', (e) => {
      const interactiveLayers = collectInteractiveLayerIds(map);
      if (interactiveLayers.length === 0) return;

      const features = map.queryRenderedFeatures(e.point, { layers: interactiveLayers });
      if (features.length > 0) {
        const feature = features[0];
        let layerType;
        if (feature.layer.id.startsWith('live-vehicles-')) {
          layerType = 'liveVehicle';
        } else {
          // Strip known suffixes: -heat/-extrude/-line/-fallback/-ringN,
          // aircraft bucket suffixes (-b<num>|-mil-b<num>), and the
          // generic -dropline ground-cross layer.
          layerType = feature.layer.id
            .replace('layer-', '')
            .replace(/-(heat|extrude|line|fallback|dropline|ring[1-4])$/, '')
            .replace(/(-mil)?-b-?\d+$/, '');
        }
        if (onFeatureClick) {
          const coords = feature.geometry?.coordinates;
          const lngLat = Array.isArray(coords) && Number.isFinite(coords[0]) && Number.isFinite(coords[1])
            ? [coords[0], coords[1]]
            : [e.lngLat.lng, e.lngLat.lat];
          onFeatureClick(feature, layerType, lngLat);
        }
      }
    });

    mapRef.current = map;
    if (onMapReady) onMapReady(map);

    // Allow other panels (e.g. CameraDiscoveryThread) to recenter the map by
    // dispatching a `japanosint:flyto` window event. Minimal coupling — no
    // shared context or ref lifting needed.
    const flyHandler = (ev) => {
      const { lat, lon, zoom } = ev.detail || {};
      if (Number.isFinite(lat) && Number.isFinite(lon)) {
        map.flyTo({ center: [lon, lat], zoom: Number.isFinite(zoom) ? zoom : 13, speed: 1.4 });
      }
    };
    window.addEventListener('japanosint:flyto', flyHandler);

    return () => {
      window.removeEventListener('japanosint:flyto', flyHandler);
      map.remove();
      mapRef.current = null;
    };
  }, []);

  // Sync layers
  useEffect(() => {
    if (!mapReady || !mapRef.current) return;
    const map = mapRef.current;

    for (const [layerId, layerState] of Object.entries(layers)) {
      const def = LAYER_DEFINITIONS[layerId];
      if (!def) continue;

      const data = layerId === 'satelliteTracking'
        ? coloredSatelliteTrackingFc
        : layerData[layerId];
      const prev = prevLayersRef.current[layerId];
      const wasVisible = prev?.visible;
      const wasData = prev?.dataLength;
      const currentDataLength = data?.features?.length ?? 0;

      if (layerState.visible && data) {
        if (!wasVisible || wasData !== currentDataLength) {
          addLayerToMap(map, layerId, data, def, layerState.opacity);
        }
      } else if (!layerState.visible && wasVisible) {
        removeLayerFromMap(map, layerId);
      }

      prevLayersRef.current[layerId] = {
        visible: layerState.visible,
        dataLength: currentDataLength,
      };
    }

    // unifiedStations + unifiedStationFootprints are hidden auto-followed
    // layers — visibility is managed by useMapLayers from mode toggles.
    // The filter on pin slots + footprints reflects the CURRENTLY-ENABLED
    // modes so only the relevant pins/fills render for a cross-mode
    // station. Re-apply on every toggle change.
    const enabledModes = new Set();
    if (layers?.unifiedTrains?.visible) enabledModes.add('train');
    if (layers?.unifiedSubways?.visible) enabledModes.add('subway');
    if (layers?.unifiedBuses?.visible) enabledModes.add('bus');
    applyUnifiedStationsModeFilter(map, enabledModes);
  }, [layers, layerData, mapReady, coloredSatelliteTrackingFc]);

  // Live-transit vehicle layers — dedicated sources/layers managed outside
  // the standard fetch-and-paint path because data is client-generated.
  useEffect(() => {
    const map = mapRef.current;
    if (!map || !mapReady) return;

    const specs = [
      {
        sourceId: 'live-vehicles-train',
        layerId: 'live-vehicles-train-layer',
        data: liveTrainsGeo,
        visible: !!layers?.unifiedTrains?.visible,
        defaultColor: '#2e7d32',
      },
      {
        sourceId: 'live-vehicles-subway',
        layerId: 'live-vehicles-subway-layer',
        data: liveSubwaysGeo,
        visible: !!layers?.unifiedSubways?.visible,
        defaultColor: '#ff7043',
      },
      {
        sourceId: 'live-vehicles-bus',
        layerId: 'live-vehicles-bus-layer',
        data: liveBusesGeo,
        visible: !!layers?.unifiedBuses?.visible,
        defaultColor: '#fb8c00',
      },
    ];

    for (const s of specs) {
      if (!s.visible) {
        if (map.getLayer(s.layerId)) map.removeLayer(s.layerId);
        if (map.getSource(s.sourceId)) map.removeSource(s.sourceId);
        continue;
      }
      if (!map.getSource(s.sourceId)) {
        map.addSource(s.sourceId, { type: 'geojson', data: s.data, tolerance: 0 });
        // Rounded-rectangle SDF icon, tinted with the line color (darkened),
        // rotated so the long axis aligns with the track, ground-aligned
        // so it lies flat against the basemap — same treatment as the
        // ground-cross pin marker.
        map.addLayer({
          id: s.layerId,
          type: 'symbol',
          source: s.sourceId,
          minzoom: 10,
          layout: {
            'icon-image': 'live-train-rect',
            'icon-size': 0.6,
            // Add 90° so the rectangle's long axis sits along the track
            // instead of perpendicular to it.
            'icon-rotate': ['+', ['coalesce', ['get', 'bearing'], 0], 90],
            'icon-allow-overlap': true,
            'icon-ignore-placement': true,
            'icon-pitch-alignment': 'map',
            'icon-rotation-alignment': 'map',
          },
          paint: {
            'icon-color': ['coalesce', ['get', 'line_color'], s.defaultColor],
            'icon-opacity': 0.9,
          },
        });
      } else {
        const src = map.getSource(s.sourceId);
        // Defensive type check: only call setData on a GeoJSON source. If
        // the same id were ever taken by a non-geojson source (style change,
        // bug), this avoids a confusing runtime throw.
        if (src && src.type === 'geojson') src.setData(s.data);
      }
    }
    // NOTE: no cleanup return here. This effect re-runs at 10 Hz when the
    // live GeoJSON refreshes; tearing down the source/layer each run would
    // produce a visible blink. The unmount-only teardown lives in a
    // separate effect below.
  }, [mapReady, liveTrainsGeo, liveSubwaysGeo, liveBusesGeo,
      layers?.unifiedTrains?.visible,
      layers?.unifiedSubways?.visible,
      layers?.unifiedBuses?.visible]);

  // Unmount-only cleanup of live-transit sources/layers. Deps are `[]` so
  // this fires once on the component unmount, not on every data update.
  useEffect(() => {
    return () => {
      const m = mapRef.current;
      if (!m) return;
      for (const id of ['live-vehicles-train', 'live-vehicles-subway', 'live-vehicles-bus']) {
        const layerId = `${id}-layer`;
        if (m.getLayer(layerId)) m.removeLayer(layerId);
        if (m.getSource(id)) m.removeSource(id);
      }
    };
  }, []);

  // Station building footprints for major metros. Only fires at high zoom
  // (station buildings are invisible below zoom 14) and only when at least
  // one transit layer is enabled — otherwise the polygons are noise.
  useEffect(() => {
    const map = mapRef.current;
    if (!map || !mapReady) return;

    const transitOn = !!(layers?.unifiedTrains?.visible
                      || layers?.unifiedSubways?.visible
                      || layers?.unifiedBuses?.visible);
    const zoom = map.getZoom();
    const shouldShow = transitOn && zoom >= 14 && viewport;

    const SRC = 'station-boundaries';
    const LYR = 'station-boundaries-fill';
    const LYR_OUT = 'station-boundaries-outline';

    if (!shouldShow) {
      if (map.getLayer(LYR_OUT)) map.removeLayer(LYR_OUT);
      if (map.getLayer(LYR)) map.removeLayer(LYR);
      if (map.getSource(SRC)) map.removeSource(SRC);
      return;
    }

    let cancelled = false;
    (async () => {
      try {
        const bbox = `${viewport.minLng},${viewport.minLat},${viewport.maxLng},${viewport.maxLat}`;
        const res = await fetch(`/api/transit/station-boundaries?bbox=${encodeURIComponent(bbox)}`);
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const fc = await res.json();
        if (cancelled) return;
        if (!map.getSource(SRC)) {
          map.addSource(SRC, { type: 'geojson', data: fc });
          map.addLayer({
            id: LYR,
            type: 'fill',
            source: SRC,
            minzoom: 14,
            paint: {
              'fill-color': '#888888',
              'fill-opacity': 0.2,
            },
          });
          map.addLayer({
            id: LYR_OUT,
            type: 'line',
            source: SRC,
            minzoom: 14,
            paint: {
              'line-color': '#aaaaaa',
              'line-width': 1,
              'line-opacity': 0.6,
            },
          });
        } else {
          const src = map.getSource(SRC);
          if (src && src.type === 'geojson') src.setData(fc);
        }
      } catch (err) {
        if (!cancelled) console.warn('[MapView] station-boundaries', err?.message);
      }
    })();
    return () => { cancelled = true; };
  }, [mapReady,
      layers?.unifiedTrains?.visible,
      layers?.unifiedSubways?.visible,
      layers?.unifiedBuses?.visible,
      viewport?.minLng, viewport?.minLat, viewport?.maxLng, viewport?.maxLat]);

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

    return undefined;
  }, [mapReady, satelliteTrackFc]);

  // Satellite imagery "bake on map" — overlays one or more clicked scenes as
  // persistent raster layers. Each scene gets its own source/layer keyed by a
  // slug of its sceneId, so multiple bakes coexist.
  useEffect(() => {
    const map = mapRef.current;
    if (!map) return;
    const bakedIds = bakedSceneIdsRef.current;

    function removeOne(sceneId) {
      if (!map.style) { bakedIds.delete(sceneId); return; }
      const lyr = bakeLayerIdFor(sceneId);
      const src = bakeSourceIdFor(sceneId);
      if (map.getLayer(lyr)) map.removeLayer(lyr);
      if (map.getSource(src)) map.removeSource(src);
      bakedIds.delete(sceneId);
    }

    function removeAll() {
      for (const id of Array.from(bakedIds)) removeOne(id);
    }

    function onBake(e) {
      const { show, sceneId, tileUrl, previewUrl, bboxGeom, opacity } = e.detail || {};
      if (!map || !sceneId) return;
      const lyr = bakeLayerIdFor(sceneId);
      const src = bakeSourceIdFor(sceneId);

      // Same scene already on the map → opacity-only update.
      if (show && bakedIds.has(sceneId) && map.getLayer(lyr)) {
        map.setPaintProperty(lyr, 'raster-opacity', opacity ?? 0.6);
        return;
      }

      if (!show) { removeOne(sceneId); return; }

      // Fresh add. Defensive cleanup in case of stale leftovers.
      if (map.getLayer(lyr)) map.removeLayer(lyr);
      if (map.getSource(src)) map.removeSource(src);

      if (tileUrl) {
        map.addSource(src, { type: 'raster', tiles: [tileUrl], tileSize: 256 });
      } else if (previewUrl) {
        map.addSource(src, {
          type: 'image',
          url: previewUrl,
          coordinates: imageCoordsFromGeom(bboxGeom),
        });
      } else {
        return;
      }
      // Insert beneath the first symbol layer so pins/icons stay on top.
      const firstSymbol = map.getStyle()?.layers?.find((l) => l.type === 'symbol')?.id;
      const visible = satelliteImageryVisibleRef.current;
      map.addLayer(
        {
          id: lyr,
          type: 'raster',
          source: src,
          layout: { visibility: visible ? 'visible' : 'none' },
          paint: { 'raster-opacity': opacity ?? 0.6 },
        },
        firstSymbol,
      );
      bakedIds.add(sceneId);
    }

    window.addEventListener('satellite-imagery-bake', onBake);
    return () => {
      window.removeEventListener('satellite-imagery-bake', onBake);
      removeAll();
    };
    // `layers` is read via the closure above only for initial visibility of a
    // newly-baked layer; visibility changes afterward are handled by the
    // separate effect below, so we deliberately don't rebind this effect.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Sync baked-layer visibility with the Satellite Imagery layer toggle.
  useEffect(() => {
    const visible = !!layers?.satelliteImagery?.visible;
    satelliteImageryVisibleRef.current = visible;
    const map = mapRef.current;
    if (!map || !map.style) return;
    const vis = visible ? 'visible' : 'none';
    for (const sceneId of bakedSceneIdsRef.current) {
      const lyr = bakeLayerIdFor(sceneId);
      if (map.getLayer(lyr)) {
        map.setLayoutProperty(lyr, 'visibility', vis);
      }
    }
  }, [layers?.satelliteImagery?.visible]);

  // PLATEAU 3D buildings: spin up a deck.gl MapboxOverlay on demand,
  // mount per-city Tile3DLayer instances pointing at PLATEAU's nationwide
  // 3D Tiles catalog, and hide the basemap's flat 2D building footprints
  // while it's active. Re-applied on style switch via the switchStyle
  // callback so basemap changes don't strand it.
  useEffect(() => {
    const map = mapRef.current;
    if (!map || !mapReady) return;
    const visible = !!layers?.plateauBuildings?.visible;
    const opacity = layers?.plateauBuildings?.opacity ?? 1;

    if (!visible) {
      pushDeckLayers('plateau', []);
      setBasemapBuildingsHidden(map, false);
      return;
    }

    // Lazy-create the overlay the first time we need it. addControl docks
    // it into the map's WebGL context so deck layers interleave correctly.
    if (!deckOverlayRef.current) {
      deckOverlayRef.current = new MapboxOverlay({ interleaved: true, layers: [] });
      map.addControl(deckOverlayRef.current);
    }

    let cancelled = false;
    setBasemapBuildingsHidden(map, true);

    loadPlateauTilesets()
      .then((tilesets) => {
        if (cancelled || !deckOverlayRef.current) return;
        const tint = PLATEAU_TINT_BY_STYLE[currentStyle] ?? [120, 120, 120];
        const deckLayers = tilesets.map((t) => new Tile3DLayer({
          id: `plateau-bldg-${t.id}`,
          data: t.tilesetUrl,
          loader: Tiles3DLoader,
          opacity,
          // Shift the whole tileset up/down along the local ellipsoid normal
          // by PLATEAU_Z_OFFSET_M so building foundations sit on the basemap
          // plane instead of floating above it. Tweak the constant above to
          // taste — the value is in metres, negative = down.
          onTilesetLoad: (tileset) => {
            const c = tileset?.cartographicCenter; // [lon°, lat°, h] (degrees)
            if (!c || !Number.isFinite(c[0]) || !Number.isFinite(c[1])) return;
            const lonRad = (c[0] * Math.PI) / 180;
            const latRad = (c[1] * Math.PI) / 180;
            // Local ellipsoid normal in ECEF (unit vector pointing "up").
            const nx = Math.cos(latRad) * Math.cos(lonRad);
            const ny = Math.cos(latRad) * Math.sin(lonRad);
            const nz = Math.sin(latRad);
            const dx = PLATEAU_Z_OFFSET_M * nx;
            const dy = PLATEAU_Z_OFFSET_M * ny;
            const dz = PLATEAU_Z_OFFSET_M * nz;
            // Pre-multiply the existing modelMatrix (loaders.gl initializes
            // it to a Matrix4 identity) so any baked-in transform is kept.
            const base = tileset.modelMatrix instanceof Matrix4
              ? tileset.modelMatrix
              : new Matrix4(tileset.modelMatrix || undefined);
            tileset.modelMatrix = new Matrix4().translate([dx, dy, dz]).multiplyRight(base);
          },
          // PLATEAU LOD1 meshes ship as untextured white; repaint them in
          // the theme tint so they don't read as a solid white blob on
          // dark basemaps. Textured LOD2 materials are left alone.
          onTileLoad: (tile) => {
            const gltf = tile?.content?.gltf;
            if (gltf) tintGltfMaterials(gltf, tint);
          },
          pickable: false,
        }));
        pushDeckLayers('plateau', deckLayers);
      })
      .catch((err) => {
        console.warn('[MapView] PLATEAU tilesets unavailable:', err?.message);
      });

    return () => { cancelled = true; };
  }, [
    mapReady,
    layers?.plateauBuildings?.visible,
    layers?.plateauBuildings?.opacity,
    currentStyle,
  ]);

  // Re-channel basemap labels through deck.gl so they sit *above* PLATEAU
  // 3D buildings instead of disappearing behind them. Always-on, always
  // billboarded. Re-harvested on every moveend so labels track the viewport.
  useEffect(() => {
    const map = mapRef.current;
    if (!map || !mapReady) return;

    // Lazy-create the shared overlay if PLATEAU hasn't already.
    if (!deckOverlayRef.current) {
      deckOverlayRef.current = new MapboxOverlay({ interleaved: true, layers: [] });
      map.addControl(deckOverlayRef.current);
    }

    setBasemapSymbolsHidden(map, true);

    const refresh = () => {
      // After a setStyle the new style ships its own symbol layers — hide them
      // again on every refresh so style swaps don't strand native labels.
      setBasemapSymbolsHidden(map, true);
      const data = harvestBasemapLabels(map);
      const layer = new TextLayer({
        id: 'basemap-labels-rechanneled',
        data,
        getPosition: (d) => d.position,
        getText: (d) => d.text,
        getSize: 13,
        getColor: [235, 235, 240, 230],
        getAngle: 0,
        billboard: true,
        sizeUnits: 'pixels',
        background: true,
        getBackgroundColor: [10, 12, 18, 170],
        backgroundPadding: [4, 2],
        fontFamily: 'Inter, "Helvetica Neue", system-ui, sans-serif',
        fontWeight: 500,
        characterSet: 'auto',
        outlineWidth: 2,
        outlineColor: [0, 0, 0, 200],
        fontSettings: { sdf: true },
        pickable: false,
      });
      pushDeckLayers('labels', [layer]);
    };

    refresh();
    map.on('moveend', refresh);
    map.on('styledata', refresh);

    return () => {
      map.off('moveend', refresh);
      map.off('styledata', refresh);
      pushDeckLayers('labels', []);
      setBasemapSymbolsHidden(map, false);
    };
  }, [mapReady, currentStyle, pushDeckLayers]);

  // Style switcher
  const switchStyle = useCallback((styleKey) => {
    if (!mapRef.current || styleKey === currentStyle) return;
    mapRef.current.setStyle(MAP_STYLES[styleKey].style);
    setCurrentStyle(styleKey);
    prevLayersRef.current = {};

    // Re-add layers after style change (icons must be re-registered
    // because map.setStyle() drops all previously added images)
    mapRef.current.once('style.load', () => {
      registerLayerIcons(mapRef.current);
      for (const [layerId, layerState] of Object.entries(layers)) {
        if (layerState.visible && layerData[layerId]) {
          addLayerToMap(
            mapRef.current,
            layerId,
            layerData[layerId],
            LAYER_DEFINITIONS[layerId],
            layerState.opacity
          );
        }
      }
      // PLATEAU's overlay survives setStyle, but setLayoutProperty does
      // not — re-hide the new basemap's flat building layer if PLATEAU
      // is currently on. The PLATEAU effect will also re-fire because
      // currentStyle is in its dep array; this just plugs the gap
      // between style.load and the next React render.
      if (layers?.plateauBuildings?.visible) {
        setBasemapBuildingsHidden(mapRef.current, true);
      }
    });
  }, [currentStyle, layers, layerData]);

  return (
    <div className="map-container">
      <div ref={mapContainerRef} className="map-wrapper" />

      {/* Style switcher */}
      <div className="absolute top-3 right-14 z-20 flex gap-1">
        {Object.entries(MAP_STYLES).map(([key, style]) => (
          <button
            key={key}
            onClick={() => switchStyle(key)}
            className={`px-2 py-1 text-[10px] rounded border transition-colors ${
              currentStyle === key
                ? 'border-neon-cyan/50 bg-neon-cyan/10 text-neon-cyan'
                : 'border-osint-border bg-osint-surface/80 text-gray-500 hover:text-gray-300'
            }`}
          >
            {style.name}
          </button>
        ))}
      </div>

      {/* Cursor coordinates */}
      {cursorCoords && (
        <div className="absolute bottom-1 left-1 z-20 text-[10px] font-mono text-gray-500 bg-osint-bg/80 px-2 py-0.5 rounded">
          {cursorCoords.lat}, {cursorCoords.lng} | z{zoom}
        </div>
      )}
    </div>
  );
}
