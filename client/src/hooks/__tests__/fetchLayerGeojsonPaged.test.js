import { describe, it, expect } from 'vitest';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { fetchLayerGeojsonPaged } from '../useMapLayers.js';

const here = path.dirname(fileURLToPath(import.meta.url));
const CAPTURE = JSON.parse(readFileSync(
  path.resolve(here, '../../../../native/tests/contract/_api_layers_v2_geojson.json'), 'utf8',
));

function pageFor(url, total, limit) {
  const u = new URL(url, 'http://x');
  const offset = Number(u.searchParams.get('offset'));
  const n = Math.max(0, Math.min(limit, total - offset));
  const features = Array.from({ length: n }, (_, i) => ({
    type: 'Feature', geometry: { type: 'Point', coordinates: [0, 0] }, properties: { i: offset + i },
  }));
  const truncated = offset + n < total;
  return {
    ...CAPTURE,
    features,
    _meta: {
      ...CAPTURE._meta, records_available: total, records_used: n, limit, offset, truncated,
      ...(truncated ? { next_offset: offset + n } : {}),
    },
  };
}

const okRes = (body) => ({ ok: true, status: 200, json: async () => body });

describe('fetchLayerGeojsonPaged', () => {
  it('follows _meta.truncated / next_offset until the server says complete, reporting the bound in band meanwhile', async () => {
    const calls = [];
    const partials = [];
    const fetchImpl = async (url) => { calls.push(url); return okRes(pageFor(url, 12, 5)); };
    const fc = await fetchLayerGeojsonPaged('police-crime-points', { limit: 5, fetchImpl, onPage: (p) => partials.push({ n: p.features.length, t: p._meta.truncated }) });
    expect(calls).toHaveLength(3);
    expect(calls[0]).toMatch(/\/api\/layers\/police-crime-points\/geojson\?limit=5&offset=0$/);
    expect(calls[2]).toMatch(/offset=10$/);
    expect(fc.features).toHaveLength(12);
    expect(fc.features.map((f) => f.properties.i)).toEqual([...Array(12).keys()]);
    expect(fc._meta.truncated).toBe(false);
    expect(fc._meta.records_available).toBe(12);
    expect(fc._meta.records_used).toBe(12);
    // while pages are still arriving the partial collection says it is truncated
    expect(partials).toEqual([{ n: 5, t: true }, { n: 10, t: true }, { n: 12, t: false }]);
  });

  it('the real empty capture is one page, zero features, not truncated', async () => {
    const fc = await fetchLayerGeojsonPaged('police-crime-points', { limit: 5, fetchImpl: async () => okRes(CAPTURE) });
    expect(fc.features).toEqual([]);
    expect(fc._meta.truncated).toBe(false);
    expect(fc._meta.records_available).toBe(0);
    expect(fc._meta.served_from).toBe('intel_items');
  });

  it('a 404 throws an error carrying the status so the caller can distinguish it from a network error', async () => {
    await expect(fetchLayerGeojsonPaged('nope', { fetchImpl: async () => ({ ok: false, status: 404 }) }))
      .rejects.toMatchObject({ status: 404, message: 'HTTP 404' });
  });

  it('stays marked truncated (never claims completeness) when next_offset stops advancing', async () => {
    const fetchImpl = async () => okRes({
      type: 'FeatureCollection',
      features: [{ type: 'Feature', geometry: null, properties: {} }],
      _meta: { truncated: true, next_offset: 0, records_available: 99 },
    });
    const fc = await fetchLayerGeojsonPaged('stuck', { limit: 1, fetchImpl });
    expect(fc.features).toHaveLength(1);
    expect(fc._meta.truncated).toBe(true);
    expect(fc._meta.truncation_reason).toMatch(/did not advance/);
  });
});
