import { test } from 'node:test';
import assert from 'node:assert/strict';
import { interpolateAlongShape } from '../src/utils/gtfsActiveTrips.js';

test('interpolateAlongShape returns the first point when distM <= 0', () => {
  const shape = [
    { lat: 35.0, lon: 139.0, dist_m: 0 },
    { lat: 35.0, lon: 140.0, dist_m: 100_000 },
  ];
  const p = interpolateAlongShape(shape, 0);
  assert.equal(p.lat, 35.0);
  assert.equal(p.lon, 139.0);
});

test('interpolateAlongShape interpolates linearly', () => {
  const shape = [
    { lat: 35.0, lon: 139.0, dist_m: 0 },
    { lat: 35.0, lon: 140.0, dist_m: 100_000 },
  ];
  const p = interpolateAlongShape(shape, 50_000);
  assert.ok(Math.abs(p.lon - 139.5) < 0.01);
  assert.ok(Math.abs(p.lat - 35.0) < 0.001);
});

test('interpolateAlongShape crosses multiple segments', () => {
  const shape = [
    { lat: 0, lon: 0, dist_m: 0 },
    { lat: 0, lon: 1, dist_m: 100 },
    { lat: 0, lon: 2, dist_m: 200 },
  ];
  const p = interpolateAlongShape(shape, 150);
  assert.ok(Math.abs(p.lon - 1.5) < 0.01);
});

test('interpolateAlongShape clamps past the end', () => {
  const shape = [
    { lat: 0, lon: 0, dist_m: 0 },
    { lat: 0, lon: 1, dist_m: 100 },
  ];
  const p = interpolateAlongShape(shape, 999);
  assert.equal(p.lon, 1);
});

test('interpolateAlongShape returns null for empty shape', () => {
  assert.equal(interpolateAlongShape([], 50), null);
  assert.equal(interpolateAlongShape(null, 50), null);
});
