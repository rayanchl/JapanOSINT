import { test } from 'node:test';
import assert from 'node:assert/strict';
import collectSatelliteImagery from '../src/collectors/satelliteImagery.js';

test('satelliteImagery returns a valid FeatureCollection envelope', async () => {
  const fc = await collectSatelliteImagery();
  assert.equal(fc.type, 'FeatureCollection');
  assert.ok(Array.isArray(fc.features));
  assert.ok(fc._meta);
  assert.equal(typeof fc._meta.live, 'boolean');
  assert.equal(typeof fc._meta.recordCount, 'number');
  assert.equal(fc._meta.recordCount, fc.features.length);
});

test('satelliteImagery features have required OSINT props', async () => {
  const fc = await collectSatelliteImagery();
  assert.ok(fc.features.length > 0, 'must always return at least seed features');
  for (const f of fc.features) {
    assert.equal(f.type, 'Feature');
    assert.equal(f.geometry.type, 'Point');
    assert.ok(Array.isArray(f.geometry.coordinates));
    assert.equal(f.geometry.coordinates.length, 2);
    assert.ok(f.properties.platform, `missing platform: ${JSON.stringify(f.properties)}`);
    assert.ok(f.properties.source, `missing source: ${JSON.stringify(f.properties)}`);
    assert.ok(f.properties.id);
    // preview_url OR tile_url must be present (nullable but the key must exist).
    assert.ok('preview_url' in f.properties || 'tile_url' in f.properties);
  }
});

test('satelliteImagery seeds have archive_era tag', async () => {
  const fc = await collectSatelliteImagery();
  for (const f of fc.features) {
    assert.ok(
      f.properties.archive_era === 'real-time' || f.properties.archive_era === 'historical',
      `unexpected archive_era: ${f.properties.archive_era}`
    );
  }
});
