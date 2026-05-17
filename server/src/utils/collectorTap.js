/**
 * Collector Follow Tap
 *
 * Captures every outbound HTTP request fired by a collector and broadcasts
 * a structured event over the existing /ws WebSocket so the client's
 * FollowPanel can render it like a network inspector.
 *
 * How it works
 *  - `installFetchTap()` monkey-patches `globalThis.fetch` once. Any code
 *    using fetch() inside a `withCollectorRun()` scope is auto-tapped.
 *  - `withCollectorRun(key, fn)` opens an AsyncLocalStorage scope that
 *    carries `{run_id, collector_key, source_id}` to the patched fetch.
 *  - Outside any run scope (raw Express middleware, health checks, the
 *    scheduler's own probe `fetchSource`), emission is skipped — we only
 *    surface real collector traffic.
 *  - A bounded ring buffer (cap RING_CAPACITY) seeds new clients via
 *    `getRecentEvents()` — exposed by routes/follow.js.
 *
 * Event shapes (broadcast over WS):
 *  {type:'collector_request_hit', phase:'start'|'end', run_id, request_seq,
 *   collector_key, source_id, source_name, method, url, host, path,
 *   status, latency_ms, response_bytes, data_type, error, timestamp}
 *  {type:'collector_run_start', run_id, collector_key, source_id,
 *   source_name, trigger, timestamp}
 *  {type:'collector_run_end', run_id, status, duration_ms, hit_count,
 *   error?, timestamp}
 *  {type:'collector_hit_annotate', run_id, request_seq, record_count,
 *   data_type}
 */

import { AsyncLocalStorage } from 'node:async_hooks';
import crypto from 'node:crypto';
import sources from './sourceRegistry.js';
import { recordCollectorUsage } from './usage.js';

const RING_CAPACITY = 5000;

export const runStore = new AsyncLocalStorage();

let wsRef = null;
let ringBuffer = [];

/** Cache source registry lookups by collector key */
const sourceByKey = new Map();
function lookupSource(key) {
  if (sourceByKey.has(key)) return sourceByKey.get(key);
  const s = sources.find((x) => x.id === key) || null;
  sourceByKey.set(key, s);
  return s;
}

export function setBroadcaster(wsServer) {
  wsRef = wsServer;
}

export function getBroadcaster() {
  return wsRef;
}

function broadcast(payload) {
  // Always push to ring buffer first so /api/follow/recent stays useful
  // even when no clients are connected.
  pushRing(payload);
  if (!wsRef) return;
  let msg;
  try { msg = JSON.stringify(payload); } catch { return; }
  for (const client of wsRef.clients) {
    if (client.readyState === 1) {
      try { client.send(msg); } catch { /* ignore single-client error */ }
    }
  }
}

/** Public broadcast used by non-collector subsystems (e.g. ftsRegistry's fts_ready). */
export function broadcastEvent(payload) {
  broadcast(payload);
}

function pushRing(ev) {
  ringBuffer.push(ev);
  if (ringBuffer.length > RING_CAPACITY) {
    ringBuffer = ringBuffer.slice(ringBuffer.length - RING_CAPACITY);
  }
}

export function getRecentEvents(limit = 500) {
  if (!Number.isFinite(limit) || limit <= 0) limit = 500;
  if (limit > RING_CAPACITY) limit = RING_CAPACITY;
  return ringBuffer.slice(-limit);
}

// ─── URL & content-type helpers ──────────────────────────────────────────

function splitUrl(rawUrl) {
  try {
    const u = new URL(rawUrl);
    return { host: u.host, path: u.pathname + (u.search || '') };
  } catch {
    return { host: 'unknown', path: rawUrl?.slice(0, 200) || '' };
  }
}

function deriveDataType(contentType) {
  if (!contentType) return null;
  const ct = String(contentType).toLowerCase();
  if (ct.includes('geo+json')) return 'geojson';
  if (ct.includes('json')) return 'json';
  if (ct.includes('xml') || ct.includes('rss') || ct.includes('atom')) return 'xml';
  if (ct.includes('html')) return 'html';
  if (ct.includes('csv')) return 'csv';
  if (ct.startsWith('text/')) return 'text';
  if (ct.startsWith('image/') || ct.startsWith('video/') || ct.startsWith('audio/')) return 'binary';
  if (ct.startsWith('application/octet-stream')) return 'binary';
  return 'text';
}

function safeContentLength(headers) {
  if (!headers) return null;
  const v = headers.get?.('content-length') ?? headers['content-length'];
  if (v == null) return null;
  const n = Number(v);
  return Number.isFinite(n) ? n : null;
}

// ─── Run scope ───────────────────────────────────────────────────────────

/**
 * Run `fn` inside an ALS scope so the global fetch tap can attribute hits
 * to this collector run. Emits `collector_run_start` / `collector_run_end`
 * brackets either way.
 *
 * @param {string} key   collector key (matches sourceRegistry id when possible)
 * @param {() => Promise<any>} fn
 * @param {object} [opts]
 * @param {string} [opts.trigger='on-demand']  'cron'|'on-demand'|'boot'
 */
/**
 * Flush per-tenant usage for a finished run. No-op unless the run carried a
 * tenant id AND a credential was actually consumed (set by noteCredentialUse
 * via collectorEnv.envFor). Idempotent — guarded by ctx._usage_flushed.
 */
function flushUsage(ctx) {
  if (!ctx || ctx._usage_flushed) return;
  ctx._usage_flushed = true;
  if (!ctx.tenant_id) return;
  recordCollectorUsage({
    tenantId: ctx.tenant_id,
    sourceId: ctx.source_id,
    usedTenant: !!ctx.used_tenant_cred,
    usedPlatform: !!ctx.used_platform_cred,
  });
}

export async function withCollectorRun(key, fn, { trigger = 'on-demand', tenantId = null } = {}) {
  const run_id = crypto.randomUUID();
  const source = lookupSource(key);
  const ctx = {
    run_id,
    collector_key: key,
    source_id: source?.id ?? key,
    source_name: source?.name ?? key,
    seq: 0,
    hit_count: 0,
    last_hit: null,
    started_ms: Date.now(),
    // Tenant context: set by request-scoped callers (on-demand routes) so
    // collectorEnv.envFor resolves BYOK secrets and usage is attributed.
    // null for scheduler / cron / boot runs → platform fallback, no metering.
    tenant_id: tenantId || null,
    used_tenant_cred: false,
    used_platform_cred: false,
  };

  broadcast({
    type: 'collector_run_start',
    run_id,
    collector_key: key,
    source_id: ctx.source_id,
    source_name: ctx.source_name,
    trigger,
    timestamp: new Date().toISOString(),
  });

  try {
    const result = await runStore.run(ctx, fn);
    flushUsage(ctx);
    broadcast({
      type: 'collector_run_end',
      run_id,
      status: 'ok',
      duration_ms: Date.now() - ctx.started_ms,
      hit_count: ctx.hit_count,
      timestamp: new Date().toISOString(),
    });
    return result;
  } catch (err) {
    flushUsage(ctx);
    broadcast({
      type: 'collector_run_end',
      run_id,
      status: 'error',
      duration_ms: Date.now() - ctx.started_ms,
      hit_count: ctx.hit_count,
      error: err?.message || String(err),
      timestamp: new Date().toISOString(),
    });
    throw err;
  }
}

/**
 * Tenant scope for non-collector pipelines (the OSINT search tab). Runs `fn`
 * inside an ALS scope carrying the tenant id so any `envFor()` deep in the
 * pipeline / services resolves the tenant's BYOK key, and any consumed
 * credential is metered into tenant_quotas under `label`.
 *
 * Differs from withCollectorRun: `tap_silent` suppresses the per-request
 * Follow-log broadcasts (a search run is not a collector and should not
 * masquerade as one), and it returns whatever `fn` returns synchronously so
 * fire-and-forget pipelines work. Usage is flushed when the returned value
 * settles: an EventEmitter on 'done'/'error', a promise on settle, otherwise
 * immediately.
 *
 * @param {string|null} tenantId
 * @param {string} label  source id used for usage attribution (e.g. 'osint-search')
 * @param {() => any} fn
 */
export function withTenantScope(tenantId, label, fn) {
  const ctx = {
    run_id: crypto.randomUUID(),
    collector_key: label,
    source_id: label,
    source_name: label,
    seq: 0,
    hit_count: 0,
    last_hit: null,
    started_ms: Date.now(),
    tenant_id: tenantId || null,
    tap_silent: true,
    used_tenant_cred: false,
    used_platform_cred: false,
  };
  const done = () => flushUsage(ctx);
  const result = runStore.run(ctx, fn);
  if (result && typeof result.once === 'function') {
    result.once('done', done);
    result.once('error', done);
  } else if (result && typeof result.then === 'function') {
    Promise.resolve(result).then(done, done);
  } else {
    done();
  }
  return result;
}

/**
 * Record that a credential resolved during the in-flight run, tagged by its
 * origin ('tenant' = BYOK, 'platform' = shared key). Called by
 * collectorEnv.envFor on every successful resolve. Safe no-op outside a run.
 */
export function noteCredentialUse(source) {
  const ctx = runStore.getStore();
  if (!ctx) return;
  if (source === 'tenant') ctx.used_tenant_cred = true;
  else if (source === 'platform') ctx.used_platform_cred = true;
}

/**
 * Return the tenant id for the in-flight collector run, or null when there
 * is no run scope (scheduler / cron / platform-level invocations) or the
 * run carries no tenant. Used by the tenant-aware credential resolver so
 * BYOK secrets are honored for request-scoped collector runs.
 *
 * The run context `tenant_id` is set by withCollectorRun's caller (e.g. an
 * on-demand request handler) via the ALS ctx; absence => platform fallback.
 */
export function currentTenant() {
  const ctx = runStore.getStore();
  return ctx?.tenant_id ?? null;
}

/**
 * Annotate the most recent emitted hit in the current run with a
 * post-parse record count (e.g. `data.features.length`). Safe no-op when
 * called outside a run.
 */
export function annotateLastHit({ record_count, data_type } = {}) {
  const ctx = runStore.getStore();
  if (!ctx || !ctx.last_hit) return;
  const ann = {
    type: 'collector_hit_annotate',
    run_id: ctx.run_id,
    request_seq: ctx.last_hit.request_seq,
  };
  if (Number.isFinite(record_count)) ann.record_count = record_count;
  if (data_type) ann.data_type = data_type;
  broadcast(ann);
}

// ─── Global fetch patch ──────────────────────────────────────────────────

export function installFetchTap() {
  if (globalThis.__collectorFetchPatched) return;
  globalThis.__collectorFetchPatched = true;

  const realFetch = globalThis.fetch.bind(globalThis);

  globalThis.fetch = async function tappedFetch(input, init) {
    const ctx = runStore.getStore();
    if (!ctx || ctx.tap_silent) {
      // Outside a collector run, or inside a tap-silent tenant scope (search
      // pipeline) — pass through without Follow-log emission. The ALS context
      // still resolves for currentTenant()/envFor.
      return realFetch(input, init);
    }

    const url = typeof input === 'string'
      ? input
      : (input?.url || String(input));
    const method = (init?.method || (typeof input === 'object' && input?.method) || 'GET').toUpperCase();
    const { host, path } = splitUrl(url);
    const request_seq = ++ctx.seq;
    const ts = new Date().toISOString();
    const startMs = Date.now();

    const baseHit = {
      type: 'collector_request_hit',
      run_id: ctx.run_id,
      request_seq,
      collector_key: ctx.collector_key,
      source_id: ctx.source_id,
      source_name: ctx.source_name,
      method,
      url,
      host,
      path,
      timestamp: ts,
    };

    // Phase: start (in-flight row)
    broadcast({ ...baseHit, phase: 'start', status: null, latency_ms: null });
    ctx.last_hit = { request_seq };

    try {
      const res = await realFetch(input, init);
      const latency_ms = Date.now() - startMs;
      const ct = res.headers?.get?.('content-type') || null;
      broadcast({
        ...baseHit,
        phase: 'end',
        status: res.status,
        latency_ms,
        response_bytes: safeContentLength(res.headers),
        data_type: deriveDataType(ct),
        error: null,
      });
      ctx.hit_count += 1;
      return res;
    } catch (err) {
      const latency_ms = Date.now() - startMs;
      broadcast({
        ...baseHit,
        phase: 'end',
        status: null,
        latency_ms,
        response_bytes: null,
        data_type: null,
        error: err?.message || String(err),
      });
      ctx.hit_count += 1;
      throw err;
    }
  };
}
