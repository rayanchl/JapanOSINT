import { describe, it, expect } from 'vitest';
import {
  renderByModality, rendererForModality, resolveRasterSources, geometryFamilies,
} from '../modalityRenderers.js';

function fakeMap() {
  const layers = [];
  const sources = {};
  return {
    layers,
    sources,
    addLayer(cfg) { layers.push(cfg); },
    addSource(id, cfg) { sources[id] = cfg; },
    getLayer(id) { return layers.find((l) => l.id === id); },
    getSource(id) { return sources[id]; },
  };
}

const pt = (props = {}) => ({ type: 'Feature', geometry: { type: 'Point', coordinates: [139.7, 35.7] }, properties: props });
const ln = () => ({ type: 'Feature', geometry: { type: 'LineString', coordinates: [[139, 35], [140, 36]] }, properties: {} });
const pg = (props = {}) => ({
  type: 'Feature',
  geometry: { type: 'Polygon', coordinates: [[[139, 35], [140, 35], [140, 36], [139, 36], [139, 35]]] },
  properties: props,
});
const fc = (...features) => ({ type: 'FeatureCollection', features });

function render(modality, geojson, applyOverride) {
  const map = fakeMap();
  const res = renderByModality(map, {
    layerId: 'x', entry: { modality, color: '#123456' }, geojson, opacity: 1,
    sourceId: 'source-x', mainLayerId: 'layer-x', applyOverride,
  });
  return { map, res, types: map.layers.map((l) => l.type) };
}

describe('rendererForModality', () => {
  it('maps each declared modality to its renderer and null to geometry', () => {
    expect(rendererForModality('point')).toBe('point');
    expect(rendererForModality('heatmap')).toBe('heatmap');
    expect(rendererForModality('line')).toBe('line');
    expect(rendererForModality('polygon')).toBe('polygon');
    expect(rendererForModality('raster')).toBe('raster');
    expect(rendererForModality(null)).toBe('geometry');
    expect(rendererForModality(undefined)).toBe('geometry');
    expect(rendererForModality('bogus')).toBe('geometry');
  });
});

describe('renderByModality', () => {
  it('point -> circle', () => {
    const { types } = render('point', fc(pt(), pt()));
    expect(types).toEqual(['circle']);
  });
  it('heatmap -> heatmap', () => {
    const { types, map } = render('heatmap', fc(pt()));
    expect(types).toEqual(['heatmap']);
    expect(map.layers[0].id).toBe('layer-x-heat');
  });
  it('line -> line', () => {
    expect(render('line', fc(ln())).types).toEqual(['line']);
  });
  it('polygon -> fill + outline', () => {
    expect(render('polygon', fc(pg())).types).toEqual(['fill', 'line']);
  });
  it('null modality -> by each feature geometry type, nothing guessed', () => {
    const { types, res } = render(null, fc(pt(), ln(), pg()));
    expect(res.renderer).toBe('geometry');
    expect(types.sort()).toEqual(['circle', 'fill', 'line', 'line'].sort());
  });
  it('null modality with only points draws only circles', () => {
    expect(render(null, fc(pt())).types).toEqual(['circle']);
  });

  it('features the primary renderer cannot draw are still drawn by geometry (nothing dropped)', () => {
    const { types } = render('point', fc(pt(), ln()));
    expect(types.sort()).toEqual(['circle', 'line']);
    const { types: t2 } = render('polygon', fc(pg(), pt()));
    expect(t2.sort()).toEqual(['circle', 'fill', 'line']);
  });

  it('per-layer override is consulted for point/undeclared and wins when it draws', () => {
    let called = 0;
    const override = (map) => { called += 1; map.addLayer({ id: 'layer-x', type: 'circle' }); return true; };
    const { types, res } = render('point', fc(pt()), override);
    expect(called).toBe(1);
    expect(types).toEqual(['circle']);
    expect(res.renderer).toBe('point+override');
  });
  it('override that declines falls through to the modality renderer', () => {
    const { types } = render('point', fc(pt()), () => false);
    expect(types).toEqual(['circle']);
  });
  it('override is NOT consulted for heatmap/line/polygon/raster — the server modality decides', () => {
    let called = 0;
    const override = () => { called += 1; return true; };
    render('heatmap', fc(pt()), override);
    render('line', fc(ln()), override);
    render('polygon', fc(pg()), override);
    render('raster', fc(pg()), override);
    expect(called).toBe(0);
  });
});

describe('raster renderer', () => {
  it('draws a raster tile layer from a feature tile_url template', () => {
    const { map, res } = render('raster', fc(pg({ tile_url: 'https://tiles.example/{z}/{x}/{y}.png' })));
    expect(map.sources['source-x-raster-0']).toEqual({ type: 'raster', tiles: ['https://tiles.example/{z}/{x}/{y}.png'], tileSize: 256 });
    expect(map.layers.find((l) => l.type === 'raster')).toBeTruthy();
    expect(res.notices).toEqual([]);
    // the scene footprint is still a record and is drawn too
    expect(map.layers.some((l) => l.type === 'fill')).toBe(true);
  });
  it('draws a georeferenced image from preview_url + polygon footprint', () => {
    const { map } = render('raster', fc(pg({ preview_url: 'https://img.example/scene.jpg' })));
    const src = map.sources['source-x-image-0'];
    expect(src.type).toBe('image');
    expect(src.coordinates).toEqual([[139, 36], [140, 36], [140, 35], [139, 35]]);
  });
  it('with no raster URL renders no imagery and says so in band — never fabricates', () => {
    const { map, res } = render('raster', fc(pt({ name: 'scene' }), pt()));
    expect(map.layers.some((l) => l.type === 'raster')).toBe(false);
    expect(Object.keys(map.sources)).toEqual([]);
    expect(res.notices).toHaveLength(1);
    expect(res.notices[0].code).toBe('no_raster_url');
    expect(res.notices[0].message).toMatch(/none of the 2 records/);
  });
  it('resolveRasterSources ignores a preview image with no footprint', () => {
    const r = resolveRasterSources(fc(pt({ preview_url: 'https://img.example/a.png' })));
    expect(r.images).toEqual([]);
    expect(r.scanned).toBe(1);
  });
});

describe('geometryFamilies', () => {
  it('reports the families present, including inside GeometryCollections', () => {
    const g = fc({ type: 'Feature', geometry: { type: 'GeometryCollection', geometries: [{ type: 'MultiLineString', coordinates: [] }] }, properties: {} });
    expect(geometryFamilies(g)).toEqual({ point: false, line: true, polygon: false });
    expect(geometryFamilies(fc())).toEqual({ point: false, line: false, polygon: false });
  });
});
