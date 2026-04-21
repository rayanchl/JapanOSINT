import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  getCached, setCached, getTtlMs, setTtlMs, pruneExpired,
  MIN_TTL_MS, MAX_TTL_MS, DEFAULT_TTL_MS,
} from '../src/utils/collectorCache.js';
import db from '../src/utils/database.js';

// These tests hit the real SQLite DB — that's the contract we care about.
// Use unique test keys so we don't collide with seeded rows.
const KEY_A = '__test_cache_a';
const KEY_B = '__test_cache_b';

// Direct-insert helper so we can simulate "already expired" rows without
// waiting MIN_TTL_MS seconds in real time.
const stmtInsertRaw = db.prepare(
  `INSERT INTO collector_cache (key, fc_json, fetched_at, ttl_ms)
   VALUES (?, ?, ?, ?)
   ON CONFLICT(key) DO UPDATE SET
     fc_json = excluded.fc_json,
     fetched_at = excluded.fetched_at,
     ttl_ms = excluded.ttl_ms`,
);
function insertExpired(key, ageMsBeyondTtl = 1000) {
  const ttl = MIN_TTL_MS;  // the actual minimum we'd ever see in the wild
  const fetchedAt = Date.now() - ttl - ageMsBeyondTtl;
  stmtInsertRaw.run(key, JSON.stringify({ type: 'FeatureCollection', features: [] }), fetchedAt, ttl);
}

test('setCached + getCached round-trip', () => {
  const fc = {
    type: 'FeatureCollection',
    features: [{ type: 'Feature', geometry: null, properties: { id: 1 } }],
    _meta: { source: 'test', fetchedAt: 'now', recordCount: 1, live: true },
  };
  setCached(KEY_A, fc, 60_000);
  const hit = getCached(KEY_A);
  assert.ok(hit, 'expected cache hit');
  assert.equal(hit.fc.features.length, 1);
  assert.equal(hit.fc._meta.source, 'test');
  assert.ok(Number.isFinite(hit.ageMs));
  assert.ok(hit.ageMs >= 0 && hit.ageMs < 5_000);
});

test('getCached returns null for an expired row', () => {
  insertExpired(KEY_B, 5_000);
  assert.equal(getCached(KEY_B), null);
});

test('setTtlMs persists and is retrievable', () => {
  setTtlMs('__test_ttl_probe', 120_000, 'user');
  assert.equal(getTtlMs('__test_ttl_probe'), 120_000);
});

test('getTtlMs falls back to DEFAULT_TTL_MS for unknown key', () => {
  assert.equal(getTtlMs('__does_not_exist_xyz'), DEFAULT_TTL_MS);
});

test('setTtlMs clamps outside the allowed range', () => {
  // Below floor — should clamp up to MIN_TTL_MS.
  setTtlMs('__test_ttl_tiny', 100, 'user');
  assert.equal(getTtlMs('__test_ttl_tiny'), MIN_TTL_MS);
  // Above ceiling — should clamp down to MAX_TTL_MS.
  setTtlMs('__test_ttl_huge', MAX_TTL_MS * 10, 'user');
  assert.equal(getTtlMs('__test_ttl_huge'), MAX_TTL_MS);
});

test('pruneExpired removes expired rows without touching fresh ones', () => {
  setCached('__prune_fresh', { type: 'FeatureCollection', features: [] }, 60_000);
  insertExpired('__prune_stale', 5_000);
  const removed = pruneExpired();
  assert.ok(removed >= 1, `expected at least 1 pruned row, got ${removed}`);
  // Fresh entry still retrievable.
  assert.ok(getCached('__prune_fresh'), 'fresh entry should survive prune');
  // Stale entry gone.
  assert.equal(getCached('__prune_stale'), null);
});
