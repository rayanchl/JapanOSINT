/**
 * Reusable collector factory.
 *
 * Two exports:
 *
 *   - `createCollector()`            — the generalized factory. Encapsulates
 *                                      tenant-aware credential resolution,
 *                                      shared HTTP (via _liveHelpers), error
 *                                      handling, and a unified `_meta`
 *                                      envelope builder.
 *   - `createThreatIntelCollector()` — thin backward-compatible wrapper around
 *                                      `createCollector()` preserving the exact
 *                                      output shape the ~20 *Jp threat-intel
 *                                      scrapers already emit. No caller change.
 *
 * ── Migration pattern (for moving an ad-hoc collector onto this factory) ──
 *
 * A collector that currently does its own `process.env` read + AbortController
 * fetch + envelope assembly can be reduced to:
 *
 *   import { createCollector } from '../utils/threatIntelCollectorFactory.js';
 *
 *   export default createCollector({
 *     sourceId: 'abuseipdb',
 *     description: 'AbuseIPDB blacklist — JP-IP entries (confidence ≥90)',
 *     envKey: 'ABUSEIPDB_API_KEY',          // resolved via BYOK, NOT process.env
 *     envHint: 'Set ABUSEIPDB_API_KEY (free at https://...)',
 *     run: async (key, ctx) => {
 *       // ctx.fetchJson / ctx.fetchText are the shared _liveHelpers fns
 *       // (AbortController + timeout + 429/503 backoff + per-host rate limit).
 *       const data = await ctx.fetchJson('https://api.abuseipdb.com/...', {
 *         headers: { Key: key },
 *       });
 *       const features = (data?.data || []).map(toFeature);
 *       return { features, extraMeta: { total_rows: features.length } };
 *     },
 *   });
 *
 * Key wins handled by the factory so each collector no longer repeats them:
 *   1. Credential lookup honors BYOK (tenant_secrets → process.env → null)
 *      via the current collector run's tenant. No more `process.env[name]`.
 *   2. Shared HTTP helpers passed in `ctx` (no inline AbortController block).
 *   3. One unified `_meta` envelope, reconciling the GeoJSON `_meta` shape
 *      with intelHelpers.intelEnvelope's `meta` keys.
 *
 * Do NOT mass-rewrite all 300+ collectors onto this — it's opt-in. The
 * threat-intel wrapper keeps the 20 existing factory collectors working
 * unchanged while now also honoring BYOK (the single behavioral change).
 */

import { fetchJson, fetchText } from '../collectors/_liveHelpers.js';
import { envFor } from './collectorEnv.js';

/**
 * Build a unified envelope.
 *
 * Reconciles the two shapes used across the codebase:
 *   - GeoJSON collectors: { type:'FeatureCollection', features, _meta:{...} }
 *   - intel collectors:   { kind:'intel', items, meta:{...} }  (intelHelpers)
 *
 * The threat-intel collectors all emit the GeoJSON shape, so that stays the
 * default. `_meta` carries the union of fields both shapes rely on
 * (source/source_id, fetchedAt, recordCount, description, live) plus any
 * caller `meta`.
 *
 * @param {object} o
 * @param {string} o.sourceId
 * @param {string} o.description
 * @param {any[]}  [o.features=[]]
 * @param {object} [o.meta={}]
 * @param {string|null} [o.source=null]   overrides `_meta.source`
 *                                        (e.g. `${id}_no_key` / `${id}_error`)
 * @param {boolean|null} [o.live=null]    explicit health flag; defaults to
 *                                        features.length > 0
 */
function envelope({
  sourceId, description, features = [], meta = {}, source = null, live = null,
}) {
  const resolvedSource = source || sourceId;
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: resolvedSource,
      // Mirror the intel envelope's key so consumers reading either shape
      // find the id under a predictable name.
      source_id: resolvedSource,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: live != null ? !!live : features.length > 0,
      description,
      ...meta,
    },
  };
}

/**
 * Generalized collector factory.
 *
 * @param {object} opts
 * @param {string}      opts.sourceId
 * @param {string}      opts.description
 * @param {string|null} [opts.envKey]               Credential var. Resolved
 *                                                  via BYOK (tenant secret →
 *                                                  process.env → null), NOT a
 *                                                  raw process.env read. If
 *                                                  null, the collector is
 *                                                  keyless and `run(null,…)`.
 * @param {string[]}    [opts.envFallbackKeys=[]]   Extra credential vars tried
 *                                                  in order if `envKey` unset.
 * @param {string|null} [opts.envHint]              Surfaced in
 *                                                  `_meta.env_hint` when gated.
 * @param {(key: string|null, ctx: {fetchJson:Function, fetchText:Function, envFor:Function}) =>
 *          Promise<{features:any[], extraMeta?:object, source?:string, live?:boolean}>}
 *                      opts.run                    Fetch + map. May throw —
 *                                                  caught into an error
 *                                                  envelope.
 * @returns {() => Promise<object>}
 */
export function createCollector({
  sourceId,
  description,
  envKey = null,
  envFallbackKeys = [],
  envHint = null,
  run,
}) {
  if (!sourceId) throw new Error('createCollector: sourceId is required');
  if (typeof run !== 'function') throw new Error('createCollector: run must be a function');

  return async function collect() {
    let key = null;
    if (envKey) {
      const candidates = [envKey, ...envFallbackKeys];
      for (const name of candidates) {
        // BYOK-aware: reads the current collector run's tenant secret,
        // then process.env, then null. Replaces the old process.env[name].
        const v = envFor(name);
        if (v) { key = v; break; }
      }
      if (!key) {
        return envelope({
          sourceId,
          description,
          source: `${sourceId}_no_key`,
          live: false,
          meta: envHint ? { env_hint: envHint } : {},
        });
      }
    }

    try {
      const result = await run(key, { fetchJson, fetchText, envFor });
      const features = Array.isArray(result?.features) ? result.features : [];
      const extraMeta = result?.extraMeta || {};
      return envelope({
        sourceId,
        description,
        features,
        meta: extraMeta,
        source: result?.source || sourceId,
        live: result?.live != null ? result.live : null,
      });
    } catch (err) {
      return envelope({
        sourceId,
        description,
        source: `${sourceId}_error`,
        live: false,
        meta: { error: err?.message || 'fetch_failed' },
      });
    }
  };
}

/**
 * Backward-compatible threat-intel collector. Identical signature and output
 * to the original; `run` is still called as `run(key)` (the ctx arg is
 * passed too, but legacy collectors ignore it). The only behavioral change
 * is that credential lookup now honors BYOK.
 *
 * @param {object} opts  see createCollector
 * @returns {() => Promise<object>}
 */
export function createThreatIntelCollector(opts) {
  return createCollector(opts);
}
