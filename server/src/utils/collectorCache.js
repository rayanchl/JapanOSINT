/**
 * Per-collector TTL cache, SQLite-backed.
 *
 * Two tables (created in database.js):
 *   collector_ttls   — one row per collector key, TTL in ms.
 *   collector_cache  — one row per cached FeatureCollection, JSON-encoded.
 *
 * TTLs are seeded at boot from sourceRegistry.updateInterval (see
 * seedTtlsFromRegistry) but can be edited at runtime via setTtlMs or direct
 * sqlite UPDATE. Existing rows survive restarts.
 *
 * Cache entries record their TTL at write time, so a live TTL edit doesn't
 * retro-expire already-cached data.
 */

import db from './database.js';
import sources from './sourceRegistry.js';

const DEFAULT_TTL_MS = 15 * 60 * 1000;  // 15 min when no registry entry
const MIN_TTL_MS     = 60 * 1000;        // 1 min floor
const MAX_TTL_MS     = 24 * 60 * 60 * 1000; // 24 h ceiling

const stmtGetTtl    = db.prepare('SELECT ttl_ms FROM collector_ttls WHERE key = ?');
const stmtUpsertTtl = db.prepare(
  `INSERT INTO collector_ttls (key, ttl_ms, source, updated_at)
   VALUES (?, ?, ?, ?)
   ON CONFLICT(key) DO UPDATE SET
     ttl_ms = excluded.ttl_ms,
     source = excluded.source,
     updated_at = excluded.updated_at`,
);
const stmtInsertTtlIfMissing = db.prepare(
  `INSERT OR IGNORE INTO collector_ttls (key, ttl_ms, source, updated_at)
   VALUES (?, ?, ?, ?)`,
);

const stmtGetCached = db.prepare(
  'SELECT fc_json, fetched_at, ttl_ms FROM collector_cache WHERE key = ?',
);
const stmtSetCached = db.prepare(
  `INSERT INTO collector_cache (key, fc_json, fetched_at, ttl_ms)
   VALUES (?, ?, ?, ?)
   ON CONFLICT(key) DO UPDATE SET
     fc_json = excluded.fc_json,
     fetched_at = excluded.fetched_at,
     ttl_ms = excluded.ttl_ms`,
);
const stmtPruneExpired = db.prepare(
  'DELETE FROM collector_cache WHERE fetched_at + ttl_ms < ?',
);

function clampTtl(ms) {
  if (!Number.isFinite(ms) || ms <= 0) return DEFAULT_TTL_MS;
  return Math.min(MAX_TTL_MS, Math.max(MIN_TTL_MS, Math.floor(ms)));
}

export function getTtlMs(key) {
  const row = stmtGetTtl.get(key);
  if (row && Number.isFinite(row.ttl_ms)) return row.ttl_ms;
  return DEFAULT_TTL_MS;
}

export function setTtlMs(key, ttlMs, source = 'user') {
  stmtUpsertTtl.run(key, clampTtl(ttlMs), source, Date.now());
}

export function getCached(key) {
  const row = stmtGetCached.get(key);
  if (!row) return null;
  const now = Date.now();
  const ageMs = now - row.fetched_at;
  if (ageMs > row.ttl_ms) return null;
  try {
    const fc = JSON.parse(row.fc_json);
    return { fc, fetchedAt: row.fetched_at, ageMs };
  } catch {
    return null;
  }
}

export function setCached(key, fc, ttlMs) {
  let json;
  try { json = JSON.stringify(fc); } catch { return; }
  stmtSetCached.run(key, json, Date.now(), clampTtl(ttlMs));
}

export function pruneExpired() {
  const info = stmtPruneExpired.run(Date.now());
  return info.changes || 0;
}

/**
 * Seed collector_ttls from sourceRegistry. Idempotent — existing rows are
 * left untouched so runtime edits survive restarts. Should be called once
 * at server boot (see scheduler.js).
 */
export function seedTtlsFromRegistry() {
  const now = Date.now();
  let inserted = 0;
  for (const src of sources) {
    const intervalSec = Number(src.updateInterval);
    const ttlMs = clampTtl(Number.isFinite(intervalSec) && intervalSec > 0
      ? intervalSec * 1000
      : DEFAULT_TTL_MS);
    const info = stmtInsertTtlIfMissing.run(src.id, ttlMs, 'registry', now);
    if (info.changes > 0) inserted += 1;
  }
  return { inserted, total: sources.length };
}

export { DEFAULT_TTL_MS, MIN_TTL_MS, MAX_TTL_MS };
