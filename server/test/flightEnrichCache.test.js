import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  getEnrich,
  setEnrich,
  __resetForTests,
  TTL_MS,
} from '../src/utils/flightEnrichCache.js';

test('miss returns null', () => {
  __resetForTests();
  assert.equal(getEnrich('abc123'), null);
});

test('set then get returns the stored value', () => {
  __resetForTests();
  setEnrich('abc123', { origin_icao: 'RJAA', destination_icao: 'KLAX' });
  const v = getEnrich('abc123');
  assert.deepEqual(v, { origin_icao: 'RJAA', destination_icao: 'KLAX' });
});

test('entry older than TTL is treated as miss', () => {
  __resetForTests();
  setEnrich('abc123', { origin_icao: 'RJAA' }, Date.now() - TTL_MS - 1);
  assert.equal(getEnrich('abc123'), null);
});

test('TTL_MS is 10 minutes', () => {
  assert.equal(TTL_MS, 10 * 60 * 1000);
});
