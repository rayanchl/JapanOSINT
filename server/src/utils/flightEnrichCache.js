/**
 * Per-aircraft enrich cache.
 * Stores OpenSky /flights/aircraft lookups (origin_icao, destination_icao,
 * first_seen_ts, last_seen_ts) keyed by icao24. TTL is 10 minutes —
 * aircraft flight plans don't meaningfully change within a single session.
 */

export const TTL_MS = 10 * 60 * 1000;

const cache = new Map();

export function getEnrich(icao24) {
  const entry = cache.get(icao24);
  if (!entry) return null;
  if (Date.now() > entry.expiresAt) {
    cache.delete(icao24);
    return null;
  }
  return entry.data;
}

// `storedAt` is optional — tests pass a past timestamp to simulate an
// already-expired entry; production callers omit it and get `Date.now()`.
export function setEnrich(icao24, data, storedAt = Date.now()) {
  cache.set(icao24, { data, expiresAt: storedAt + TTL_MS });
}

// Exposed for `node --test` because module-level Map can't be reset
// between test runs without re-importing the module.
export function __resetForTests() {
  cache.clear();
}
