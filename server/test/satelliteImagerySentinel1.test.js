import { test } from 'node:test';
import assert from 'node:assert/strict';

// Each test uses a fresh fake fetch. We don't need module re-imports since
// the collector reads globalThis.fetch at call time inside fetchJson().
const origFetch = globalThis.fetch;

function makeJson(body, status = 200) {
  return Promise.resolve({
    ok: status >= 200 && status < 300,
    status,
    json: () => Promise.resolve(body),
  });
}

test('trySentinel1 — CDSE OData maps Products to GRD features', async () => {
  globalThis.fetch = (url) => {
    if (String(url).includes('catalogue.dataspace.copernicus.eu/odata')) {
      return makeJson({
        value: [{
          Id: 'abc-123',
          Name: 'S1A_IW_GRDH_1SDV_20260419T093000_20260419T093025_000000_000000_AAAA.SAFE',
          ContentDate: { Start: '2026-04-19T09:30:00Z', End: '2026-04-19T09:30:25Z' },
          GeoFootprint: {
            type: 'Polygon',
            coordinates: [[[139, 35], [140, 35], [140, 36], [139, 36], [139, 35]]],
          },
        }],
      });
    }
    return makeJson({}, 404);
  };
  try {
    const { trySentinel1 } = await import('../src/collectors/satelliteImagery.js');
    const features = await trySentinel1();
    assert.ok(features, 'expected features, got null');
    assert.equal(features.length, 1);
    const f = features[0];
    assert.equal(f.type, 'Feature');
    assert.equal(f.properties.platform, 'sentinel-1a');
    assert.equal(f.properties.product_type, 'GRD');
    assert.equal(f.properties.polarization, 'VV');
    assert.equal(f.properties.source, 'cdse_odata');
    assert.ok(f.properties.scene_id.includes('S1A_IW_GRDH'));
  } finally {
    globalThis.fetch = origFetch;
  }
});

test('trySentinel1 — falls back to Planetary Computer when CDSE empty', async () => {
  globalThis.fetch = (url) => {
    const u = String(url);
    if (u.includes('catalogue.dataspace.copernicus.eu')) {
      return makeJson({ value: [] });
    }
    if (u.includes('planetarycomputer.microsoft.com')) {
      return makeJson({
        features: [{
          id: 'S1A_IW_GRDH_PC_XYZ',
          geometry: {
            type: 'Polygon',
            coordinates: [[[139, 35], [140, 35], [140, 36], [139, 36], [139, 35]]],
          },
          properties: {
            platform: 'sentinel-1a',
            datetime: '2026-04-19T09:30:00Z',
          },
        }],
      });
    }
    return makeJson({}, 404);
  };
  try {
    const { trySentinel1 } = await import('../src/collectors/satelliteImagery.js');
    const features = await trySentinel1();
    assert.equal(features.length, 1);
    assert.equal(features[0].properties.source, 'planetary_computer_s1');
    assert.ok(features[0].properties.tile_url?.includes('planetarycomputer.microsoft.com'));
    assert.ok(features[0].properties.tile_url?.includes('assets=vv'));
  } finally {
    globalThis.fetch = origFetch;
  }
});
