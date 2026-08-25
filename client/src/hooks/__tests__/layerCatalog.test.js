import { describe, it, expect } from 'vitest';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import {
  buildCatalog, groupByCategory, classifyLayerData, truncationNotice, CATEGORY_ORDER,
} from '../layerCatalog.js';
import { LAYER_DEFINITIONS } from '../layerDefinitions.js';

// Real capture of GET /api/layers (v2) from the running server.
const here = path.dirname(fileURLToPath(import.meta.url));
const FIXTURE = JSON.parse(readFileSync(
  path.resolve(here, '../../../../native/tests/contract/_api_layers_v2.json'), 'utf8',
));

describe('buildCatalog', () => {
  const catalog = buildCatalog(FIXTURE, LAYER_DEFINITIONS);

  it('carries every server layer, and every client-only layer, exactly once', () => {
    const serverIds = new Set(FIXTURE.map((l) => l.id));
    const seen = new Set();
    for (const e of Object.values(catalog)) {
      if (e.serverId) {
        expect(serverIds.has(e.serverId)).toBe(true);
        expect(seen.has(e.serverId)).toBe(false);
        seen.add(e.serverId);
      }
    }
    expect(seen.size).toBe(serverIds.size);
    const clientOnly = Object.values(catalog).filter((e) => e.clientOnly);
    expect(clientOnly.length).toBeGreaterThan(0);
    for (const e of clientOnly) expect(e.serverId).toBeNull();
  });

  it('keeps the server modality verbatim and NEVER guesses a null one', () => {
    for (const srv of FIXTURE) {
      const entry = Object.values(catalog).find((e) => e.serverId === srv.id);
      expect(entry).toBeTruthy();
      expect(entry.modality).toBe(srv.modality ?? null);
      expect(entry.data_type).toBe(srv.data_type ?? null);
      expect(entry.kind).toBe(srv.kind);
    }
    // The fixture has 89 undeclared layers; all must still be null here.
    const nulls = FIXTURE.filter((l) => l.modality == null).length;
    expect(Object.values(catalog).filter((e) => e.serverId && e.modality === null).length).toBe(nulls);
  });

  it('client-only layers have no modality/data_type (undeclared), and keep their /api/data endpoint', () => {
    const e = catalog.hospitalMap;
    expect(e.clientOnly).toBe(true);
    expect(e.modality).toBeNull();
    expect(e.data_type).toBeNull();
    expect(e.endpoint).toBe('/api/data/hospital-map');
  });

  it('maps the formerly-404 client ids onto real server layers', () => {
    // earthquake -> earthquakes, weather, population, transport, river,
    // radiation, air-quality, gdelt exist server-side; landprice and crime do not.
    expect(catalog.earthquakes.serverId).toBe('earthquakes');
    expect(catalog.weather.serverId).toBe('weather');
    expect(catalog.population.serverId).toBe('population');
    expect(catalog.population.modality).toBe('polygon');
    expect(catalog.transport.serverId).toBe('transport');
    expect(catalog.river.serverId).toBe('river');
    expect(catalog.radiation.serverId).toBe('radiation');
    expect(catalog.airQuality.serverId).toBe('air-quality');
    expect(catalog.gdeltEvents.serverId).toBe('gdelt');
    expect(catalog.landPrice).toBeUndefined();
    expect(catalog.crime).toBeUndefined();
    expect(catalog['police-crime-points'].modality).toBe('point');
    expect(catalog['police-crime-heatmap'].modality).toBe('heatmap');
  });

  it('uses the server category, lowercased, for matched layers', () => {
    expect(catalog.cameras.category).toBe('cameras'); // client said Cyber
    expect(catalog['satellite-imagery'] || catalog.satelliteImagery).toBeTruthy();
    expect(catalog.satelliteImagery.modality).toBe('raster');
  });

  it('with no server list, everything is client-only and nothing is invented', () => {
    const c = buildCatalog(null, LAYER_DEFINITIONS);
    for (const e of Object.values(c)) {
      expect(e.clientOnly).toBe(true);
      expect(e.modality).toBeNull();
    }
  });
});

describe('groupByCategory', () => {
  it('never drops a category the client has not heard of', () => {
    const c = buildCatalog([
      { id: 'a', name: 'A', category: 'crime', modality: 'point' },
      { id: 'b', name: 'B', category: 'xenobiology', modality: null },
      { id: 'c', name: 'C', category: 'Xenobiology', modality: null },
      { id: 'd', name: 'D', category: null, modality: null },
    ], {});
    const groups = groupByCategory(c);
    const cats = groups.map(([cat]) => cat);
    expect(cats).toContain('xenobiology');
    expect(cats).toContain('uncategorised');
    expect(groups.find(([cat]) => cat === 'xenobiology')[1]).toEqual(['b', 'c']);
    // known categories first, in CATEGORY_ORDER, unknown appended after
    expect(cats.indexOf('crime')).toBeLessThan(cats.indexOf('xenobiology'));
    expect(CATEGORY_ORDER).toContain('crime');
  });

  it('every server category in the real capture is grouped', () => {
    const c = buildCatalog(FIXTURE, LAYER_DEFINITIONS);
    const grouped = new Set(groupByCategory(c).flatMap(([, ids]) => ids));
    for (const [id, e] of Object.entries(c)) {
      if (!e.hidden) expect(grouped.has(id)).toBe(true);
    }
    const serverCats = new Set(FIXTURE.map((l) => String(l.category).toLowerCase()));
    const groupCats = new Set(groupByCategory(c).map(([cat]) => cat));
    for (const cat of serverCats) expect(groupCats.has(cat)).toBe(true);
  });

  it('hides fused upstream layers', () => {
    const c = buildCatalog(FIXTURE, LAYER_DEFINITIONS);
    const grouped = new Set(groupByCategory(c).flatMap(([, ids]) => ids));
    expect(grouped.has('mlitN02Stations')).toBe(false);
  });
});

describe('classifyLayerData: 404 vs error vs empty', () => {
  it('distinguishes the three', () => {
    expect(classifyLayerData({ type: 'FeatureCollection', features: [], _error: 'HTTP 404', _status: 404, _notFound: true }).state).toBe('not_found');
    expect(classifyLayerData({ type: 'FeatureCollection', features: [], _error: 'HTTP 500', _status: 500 }).state).toBe('error');
    expect(classifyLayerData({ type: 'FeatureCollection', features: [], _error: 'network' }).state).toBe('error');
    expect(classifyLayerData({ type: 'FeatureCollection', features: [] }).state).toBe('empty');
    expect(classifyLayerData(undefined).state).toBe('unloaded');
  });

  it('the real empty capture classifies as empty, not error, not truncated', () => {
    const fc = JSON.parse(readFileSync(
      path.resolve(here, '../../../../native/tests/contract/_api_layers_v2_geojson.json'), 'utf8',
    ));
    const c = classifyLayerData(fc);
    expect(c.state).toBe('empty');
    expect(c.available).toBe(0);
    expect(truncationNotice(fc)).toBeNull();
  });
});

describe('truncationNotice', () => {
  it('states N of M when the server marked the collection truncated', () => {
    const fc = {
      type: 'FeatureCollection',
      features: [{ type: 'Feature', geometry: null, properties: {} }, { type: 'Feature', geometry: null, properties: {} }],
      _meta: { truncated: true, records_available: 1234, records_used: 2 },
    };
    expect(truncationNotice(fc)).toBe('Showing 2 of 1,234 records');
  });
  it('is silent when complete', () => {
    const fc = { type: 'FeatureCollection', features: [{}], _meta: { truncated: false, records_available: 1 } };
    expect(truncationNotice(fc)).toBeNull();
  });
});
