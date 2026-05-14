/**
 * Per-tenant rate limiter using a token-bucket per (tenant_id, routeClass).
 *
 * Three route classes — picked by URL prefix in `classifyRequest`. Free /
 * Pro / Team / Enterprise tiers scale the base limits by 0.25 / 1 / 4 /
 * unlimited. Token-bucket lets short bursts through without permanently
 * granting the average rate; that's a better fit for "search + skim"
 * traffic than a flat per-window counter.
 *
 * In-process state: a single-node deployment is the target for v1. When
 * the server scales out, swap `_buckets` for a Redis-backed implementation
 * with the same shape — every other piece of the middleware stays the same.
 *
 * Behind MULTI_TENANT_ENABLED.
 */

import { MULTI_TENANT_ENABLED } from './auth.js';

// Base capacity (tokens) + refill rate (tokens per second) per class.
// Tokens-per-minute is more intuitive but we work in seconds so partial
// refills are exact.
const BASE = {
  read:   { capacity: 60, refillPerSec: 60 / 60 },    //  60 rpm
  search: { capacity: 30, refillPerSec: 30 / 60 },    //  30 rpm
  mutate: { capacity: 10, refillPerSec: 10 / 60 },    //  10 rpm
};

const PLAN_MULTIPLIER = {
  free:        0.25,
  pro:         1,
  team:        4,
  enterprise:  Infinity,
};

/** key: `${tenantId}|${cls}` → { tokens, lastRefillMs, capacity, refillPerSec } */
const _buckets = new Map();

export function rateLimit(req, res, next) {
  if (!MULTI_TENANT_ENABLED) return next();
  if (!req.tenant) return next();

  const cls = classifyRequest(req);
  const mult = PLAN_MULTIPLIER[req.tenant.plan] ?? 1;
  if (mult === Infinity) return next(); // enterprise: no cap

  const base = BASE[cls];
  const capacity = base.capacity * mult;
  const refillPerSec = base.refillPerSec * mult;
  const key = `${req.tenant.id}|${cls}`;
  const now = Date.now();

  let b = _buckets.get(key);
  if (!b) {
    b = { tokens: capacity, lastRefillMs: now, capacity, refillPerSec };
    _buckets.set(key, b);
  } else if (b.capacity !== capacity) {
    // Plan changed; resize the bucket and clamp tokens.
    b.capacity = capacity;
    b.refillPerSec = refillPerSec;
    if (b.tokens > capacity) b.tokens = capacity;
  }

  // Refill since last touch.
  const elapsedSec = (now - b.lastRefillMs) / 1000;
  if (elapsedSec > 0) {
    b.tokens = Math.min(capacity, b.tokens + elapsedSec * refillPerSec);
    b.lastRefillMs = now;
  }

  if (b.tokens >= 1) {
    b.tokens -= 1;
    setRateLimitHeaders(res, b);
    return next();
  }

  // Out of tokens — compute how long until one is available.
  const retryAfterSec = Math.max(1, Math.ceil((1 - b.tokens) / refillPerSec));
  res.set('Retry-After', String(retryAfterSec));
  setRateLimitHeaders(res, b);
  return res.status(429).json({
    error: 'Rate limit exceeded',
    class: cls,
    retry_after_seconds: retryAfterSec,
  });
}

/**
 * Classify by URL prefix. New mutation routes need to land under one of the
 * mutation-shaped prefixes (alerts, integrations, members, …) or method
 * fallback catches them.
 */
function classifyRequest(req) {
  const method = req.method.toUpperCase();
  if (method === 'POST' || method === 'PUT' || method === 'PATCH' || method === 'DELETE') {
    return 'mutate';
  }
  // FTS-heavy endpoints: cost more per request, scale slower.
  if (req.path.includes('/intel/items') || req.path.includes('/search')) {
    return 'search';
  }
  return 'read';
}

function setRateLimitHeaders(res, b) {
  res.set('X-RateLimit-Limit', String(Math.round(b.capacity)));
  res.set('X-RateLimit-Remaining', String(Math.max(0, Math.floor(b.tokens))));
}
