// One renderer per server-declared modality.
//
// The server's `modality` (GET /api/layers) decides HOW a layer is drawn:
//   point   -> circle (MapView swaps circles for the layer's icon sprite)
//   heatmap -> MapLibre heatmap
//   line    -> line
//   polygon -> fill + outline
//   raster  -> raster/image source from a URL the features themselves carry
//   null    -> undeclared: each feature by its own GeoJSON geometry type
//
// Whatever the modality, features whose geometry the primary renderer cannot
// draw (a LineString in a `point` layer, a Point in a `polygon` layer) are
// drawn by geometry type as well, so no record the server sent is silently
// dropped at the client seam (house rule 2).
//
// The per-layer paint tables in MapView (`addLayerToMapInner`'s switch) are
// OVERRIDES on top of this: they are consulted only for `point` / undeclared
// layers, which is what every one of them styles, and they never decide the
// modality.

export const GEO_SUFFIXES = ['-geo-point', '-geo-line', '-geo-fill', '-geo-outline', '-raster', '-raster-footprint'];

const POINT_TYPES = new Set(['Point', 'MultiPoint']);
const LINE_TYPES = new Set(['LineString', 'MultiLineString']);
const POLY_TYPES = new Set(['Polygon', 'MultiPolygon']);

/** Which geometry families a collection actually contains. */
export function geometryFamilies(geojson) {
  const out = { point: false, line: false, polygon: false };
  const walk = (g) => {
    if (!g || !g.type) return;
    if (g.type === 'GeometryCollection') { (g.geometries || []).forEach(walk); return; }
    if (POINT_TYPES.has(g.type)) out.point = true;
    else if (LINE_TYPES.has(g.type)) out.line = true;
    else if (POLY_TYPES.has(g.type)) out.polygon = true;
  };
  for (const f of geojson?.features || []) walk(f?.geometry);
  return out;
}

/**
 * Map a server modality to the renderer that draws it. `null` (undeclared)
 * maps to 'geometry' — never to a guessed modality.
 */
export function rendererForModality(modality) {
  switch (modality) {
    case 'point': return 'point';
    case 'heatmap': return 'heatmap';
    case 'line': return 'line';
    case 'polygon': return 'polygon';
    case 'raster': return 'raster';
    default: return 'geometry';
  }
}

const TILE_KEYS = ['tile_url', 'tiles', 'tile_template', 'xyz_url', 'tile', 'url', 'href'];
const IMAGE_KEYS = ['image_url', 'image', 'preview_url', 'thumbnail_url', 'browse_url', 'url', 'href'];

function isTileTemplate(v) {
  return typeof v === 'string' && /\{z\}/.test(v) && /\{x\}/.test(v) && /\{y\}/.test(v);
}
function isImageUrl(v) {
  return typeof v === 'string' && /^https?:\/\//.test(v) && /\.(png|jpe?g|webp|gif|tiff?)(\?|$)/i.test(v);
}

function bboxFromFeature(f) {
  const b = f?.bbox || f?.properties?.bbox;
  if (Array.isArray(b) && b.length === 4 && b.every(Number.isFinite)) return b;
  const g = f?.geometry;
  if (g && POLY_TYPES.has(g.type)) {
    let minX = Infinity; let minY = Infinity; let maxX = -Infinity; let maxY = -Infinity;
    const rings = g.type === 'Polygon' ? g.coordinates : g.coordinates.flat();
    for (const ring of rings || []) for (const [x, y] of ring || []) {
      if (!Number.isFinite(x) || !Number.isFinite(y)) continue;
      if (x < minX) minX = x; if (x > maxX) maxX = x; if (y < minY) minY = y; if (y > maxY) maxY = y;
    }
    if (Number.isFinite(minX) && minX < maxX && minY < maxY) return [minX, minY, maxX, maxY];
  }
  return null;
}

/**
 * Find raster sources the collection carries. Returns
 *   { tiles: [{url}], images: [{url, coordinates}], scanned }
 * Nothing is invented: a feature with no URL contributes nothing.
 */
export function resolveRasterSources(geojson) {
  const tiles = [];
  const images = [];
  const seen = new Set();
  let scanned = 0;
  for (const f of geojson?.features || []) {
    scanned += 1;
    const p = f?.properties || {};
    let hit = false;
    for (const k of TILE_KEYS) {
      const v = Array.isArray(p[k]) ? p[k][0] : p[k];
      if (isTileTemplate(v) && !seen.has(v)) { seen.add(v); tiles.push({ url: v }); hit = true; break; }
    }
    if (hit) continue;
    for (const k of IMAGE_KEYS) {
      const v = p[k];
      if (!isImageUrl(v) || seen.has(v)) continue;
      const bbox = bboxFromFeature(f);
      if (!bbox) continue;
      const [w, s, e, n] = bbox;
      seen.add(v);
      images.push({ url: v, coordinates: [[w, n], [e, n], [e, s], [w, s]] });
      break;
    }
  }
  return { tiles, images, scanned };
}

function addPointLayer(map, id, sourceId, color, opacity, filter, extra = {}) {
  map.addLayer({
    id, type: 'circle', source: sourceId,
    ...(filter ? { filter } : {}),
    paint: {
      'circle-radius': 6,
      'circle-color': color,
      'circle-opacity': opacity * 0.8,
      'circle-stroke-width': 1,
      'circle-stroke-color': '#ffffff',
      'circle-stroke-opacity': opacity * 0.3,
      ...extra,
    },
  });
}

function addLineLayer(map, id, sourceId, color, opacity, filter) {
  map.addLayer({
    id, type: 'line', source: sourceId,
    ...(filter ? { filter } : {}),
    layout: { 'line-join': 'round', 'line-cap': 'round' },
    paint: { 'line-color': color, 'line-width': 2, 'line-opacity': opacity * 0.85 },
  });
}

function addFillLayers(map, id, outlineId, sourceId, color, opacity, filter) {
  map.addLayer({
    id, type: 'fill', source: sourceId,
    ...(filter ? { filter } : {}),
    paint: { 'fill-color': color, 'fill-opacity': opacity * 0.3 },
  });
  map.addLayer({
    id: outlineId, type: 'line', source: sourceId,
    ...(filter ? { filter } : {}),
    paint: { 'line-color': color, 'line-width': 1, 'line-opacity': opacity * 0.8 },
  });
}

function addHeatmapLayer(map, id, sourceId, color, opacity) {
  map.addLayer({
    id, type: 'heatmap', source: sourceId,
    filter: ['==', ['geometry-type'], 'Point'],
    paint: {
      'heatmap-weight': 1,
      'heatmap-intensity': 1,
      'heatmap-color': [
        'interpolate', ['linear'], ['heatmap-density'],
        0, 'rgba(0,0,0,0)',
        0.3, color,
        1, '#ffffff',
      ],
      'heatmap-radius': 24,
      'heatmap-opacity': opacity * 0.75,
    },
  });
}

const FILTER_POINT = ['in', ['geometry-type'], ['literal', ['Point', 'MultiPoint']]];
const FILTER_LINE = ['in', ['geometry-type'], ['literal', ['LineString', 'MultiLineString']]];
const FILTER_POLY = ['in', ['geometry-type'], ['literal', ['Polygon', 'MultiPolygon']]];

/**
 * Draw the geometry families in `families` that `covered` does not already
 * draw. Used as the primary renderer for undeclared modality and as the
 * leftover renderer for every other modality.
 */
function addGeometryLayers(map, base, sourceId, color, opacity, families, covered) {
  if (families.polygon && !covered.polygon) {
    addFillLayers(map, `${base}-geo-fill`, `${base}-geo-outline`, sourceId, color, opacity, FILTER_POLY);
  }
  if (families.line && !covered.line) {
    addLineLayer(map, `${base}-geo-line`, sourceId, color, opacity, FILTER_LINE);
  }
  if (families.point && !covered.point) {
    addPointLayer(map, `${base}-geo-point`, sourceId, color, opacity, FILTER_POINT);
  }
}

/**
 * Render `geojson` (already added to the map as `sourceId`) by modality.
 *
 * @param {object} ctx { layerId, entry, geojson, opacity, sourceId, mainLayerId,
 *                       applyOverride(map) -> bool  (per-layer paint override;
 *                       returns true if it drew the layer) }
 * @returns {{ renderer: string, notices: Array<{code, message}> }}
 */
export function renderByModality(map, ctx) {
  const { layerId, entry, geojson, opacity, sourceId, mainLayerId, applyOverride } = ctx;
  const modality = entry?.modality ?? null;
  const renderer = rendererForModality(modality);
  const color = entry?.color || '#9e9e9e';
  const families = geometryFamilies(geojson);
  const notices = [];
  const covered = { point: false, line: false, polygon: false };

  switch (renderer) {
    case 'heatmap':
      addHeatmapLayer(map, `${mainLayerId}-heat`, sourceId, color, opacity);
      covered.point = true;
      break;

    case 'line':
      addLineLayer(map, mainLayerId, sourceId, color, opacity, FILTER_LINE);
      covered.line = true;
      break;

    case 'polygon':
      addFillLayers(map, mainLayerId, `${mainLayerId}-geo-outline`, sourceId, color, opacity, FILTER_POLY);
      covered.polygon = true;
      break;

    case 'raster': {
      const { tiles, images, scanned } = resolveRasterSources(geojson);
      tiles.forEach((t, i) => {
        const sid = `${sourceId}-raster-${i}`;
        map.addSource(sid, { type: 'raster', tiles: [t.url], tileSize: 256 });
        map.addLayer({ id: `${mainLayerId}-raster-${i}`, type: 'raster', source: sid, paint: { 'raster-opacity': opacity } });
      });
      images.forEach((im, i) => {
        const sid = `${sourceId}-image-${i}`;
        map.addSource(sid, { type: 'image', url: im.url, coordinates: im.coordinates });
        map.addLayer({ id: `${mainLayerId}-image-${i}`, type: 'raster', source: sid, paint: { 'raster-opacity': opacity } });
      });
      if (tiles.length === 0 && images.length === 0) {
        notices.push({
          code: 'no_raster_url',
          message: `Raster layer: none of the ${scanned.toLocaleString()} records carries a tile template or georeferenced image URL, so no imagery is drawn. Record footprints are shown instead.`,
        });
      }
      // Scene footprints / points are still records: draw them.
      break;
    }

    case 'point':
    case 'geometry':
    default: {
      // The client's per-layer paint table is an override for point-shaped
      // and undeclared layers only.
      const handled = typeof applyOverride === 'function' && applyOverride(map) === true;
      if (handled) return { renderer: `${renderer}+override`, notices, layerId };
      if (renderer === 'point') {
        addPointLayer(map, mainLayerId, sourceId, color, opacity, FILTER_POINT);
        covered.point = true;
      }
      break;
    }
  }

  addGeometryLayers(map, mainLayerId, sourceId, color, opacity, families, covered);
  return { renderer, notices, layerId };
}
