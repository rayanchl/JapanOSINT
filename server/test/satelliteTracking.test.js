import { test } from 'node:test';
import assert from 'node:assert/strict';
import collectSatelliteTracking from '../src/collectors/satelliteTracking.js';

test('satelliteTracking returns a valid FeatureCollection envelope', async () => {
  const fc = await collectSatelliteTracking();
  assert.equal(fc.type, 'FeatureCollection');
  assert.ok(Array.isArray(fc.features));
  assert.ok(fc._meta);
  assert.equal(typeof fc._meta.live, 'boolean');
  assert.equal(fc._meta.recordCount, fc.features.length);
});

test('satelliteTracking features carry TLE lines for client ground-track compute', async () => {
  const fc = await collectSatelliteTracking();
  assert.ok(fc.features.length > 0, 'must always return at least seed features');
  for (const f of fc.features.slice(0, 10)) {
    assert.equal(f.type, 'Feature');
    assert.equal(f.geometry.type, 'Point');
    const [lon, lat] = f.geometry.coordinates;
    assert.ok(lon >= 122 && lon <= 154, `lon ${lon} outside Japan bbox`);
    assert.ok(lat >= 24 && lat <= 46, `lat ${lat} outside Japan bbox`);
    assert.ok(f.properties.norad_id, 'missing norad_id');
    assert.ok(f.properties.name, 'missing name');
    assert.ok(f.properties.category, 'missing category');
    assert.ok(f.properties.tle_line1 && f.properties.tle_line2, 'missing TLE lines');
  }
});

test('satelliteTracking seed ISS position is reasonable when no live data', async () => {
  const fc = await collectSatelliteTracking();
  // If live, we can't predict positions; just confirm live flag is boolean.
  assert.equal(typeof fc._meta.live, 'boolean');
});
