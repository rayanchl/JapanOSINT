/**
 * Deterministic JSON canonicalisation + SHA-256, shared by the evidence
 * capture store and the tamper-evident audit chain.
 *
 * The whole defensibility story rests on "anyone can recompute this hash
 * and get the same value", so the serialisation must be byte-stable
 * regardless of object key insertion order. We sort object keys
 * recursively and reject the things JSON.stringify would silently mangle
 * (undefined, functions, NaN/Infinity → null) by normalising them
 * explicitly so the output is predictable.
 */

import { createHash } from 'crypto';

/**
 * Canonical JSON string: object keys sorted lexicographically at every
 * depth, arrays preserved in order. Non-finite numbers and undefined
 * become null (matching JSON semantics but made explicit). The result is
 * stable across Node versions and key orderings.
 */
export function canonicalJson(value) {
  return JSON.stringify(normalise(value));
}

function normalise(v) {
  if (v === null || v === undefined) return null;
  if (typeof v === 'number') return Number.isFinite(v) ? v : null;
  if (typeof v === 'bigint') return v.toString();
  if (typeof v !== 'object') return v; // string | boolean
  if (Array.isArray(v)) return v.map(normalise);
  const out = {};
  for (const key of Object.keys(v).sort()) {
    out[key] = normalise(v[key]);
  }
  return out;
}

/** Lowercase hex SHA-256 of a string (UTF-8). */
export function sha256Hex(str) {
  return createHash('sha256').update(String(str), 'utf8').digest('hex');
}

/** Convenience: canonicalise then hash. */
export function hashObject(value) {
  return sha256Hex(canonicalJson(value));
}
