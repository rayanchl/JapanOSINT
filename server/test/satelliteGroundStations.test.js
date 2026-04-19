import { test } from 'node:test';
import assert from 'node:assert/strict';
import collectSatelliteGroundStations from '../src/collectors/satelliteGroundStations.js';

test('satelliteGroundStations returns >50 features with valid envelope', async () => {
  const fc = await collectSatelliteGroundStations();
  assert.equal(fc.type, 'FeatureCollection');
  assert.ok(Array.isArray(fc.features));
  assert.ok(fc._meta);
  assert.equal(typeof fc._meta.live, 'boolean');
  assert.equal(fc._meta.recordCount, fc.features.length);
  assert.equal(fc._meta.source, 'satellite-ground-stations');
  assert.ok(fc.features.length > 50, `expected >50 features, got ${fc.features.length}`);
});

test('satelliteGroundStations features include all 6 category types', async () => {
  const fc = await collectSatelliteGroundStations();
  const categories = new Set(fc.features.map(f => f.properties.category));
  for (const c of ['satcom', 'vlbi', 'slr', 'optical_tracking', 'gnss_reference']) {
    assert.ok(categories.has(c), `missing category: ${c}`);
  }
});
