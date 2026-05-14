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
export async function withCollectorRun(key, fn, { trigger = 'on-demand' } = {}) {
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
    if (!ctx) {
      // Outside a collector run — pass through silently.
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
