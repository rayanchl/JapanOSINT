import { test } from 'node:test';
import assert from 'node:assert/strict';
import { parseGtfsTime } from '../src/utils/gtfsIngest.js';

test('parseGtfsTime handles HH:MM:SS', () => {
  assert.equal(parseGtfsTime('00:00:00'), 0);
  assert.equal(parseGtfsTime('08:30:00'), 30_600);
});

test('parseGtfsTime handles >24h overflow GTFS permits', () => {
  assert.equal(parseGtfsTime('25:15:00'), 90_900);
  assert.equal(parseGtfsTime('120:00:00'), 432_000);
});

test('parseGtfsTime returns null for invalid input', () => {
  assert.equal(parseGtfsTime('invalid'), null);
  assert.equal(parseGtfsTime(''), null);
  assert.equal(parseGtfsTime(null), null);
  assert.equal(parseGtfsTime(undefined), null);
  assert.equal(parseGtfsTime(12345), null);
});

test('parseGtfsTime handles single-digit hours and trims whitespace', () => {
  assert.equal(parseGtfsTime('8:30:00'), 30_600);
  assert.equal(parseGtfsTime('  8:30:00  '), 30_600);
});
