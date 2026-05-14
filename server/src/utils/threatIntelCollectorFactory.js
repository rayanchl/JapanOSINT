/**
 * Factory for the GeoJSON-emitting threat-intel collector pattern.
 *
 * The 25-ish *Jp threat-intel scrapers all follow the same shape:
 *
 *   1. Read an API key from env (or fall back to "no key" mode).
 *   2. Run one or more upstream fetches (often Tokyo-anchored, no per-row geo).
 *   3. Map rows to GeoJSON features and return a FeatureCollection wrapped in
 *      a `_meta` envelope.
 *   4. On any error, return an empty FeatureCollection with `_meta.error`.
 *
 * This factory bakes points 1, 3 and 4 (key check, envelope assembly, error
 * envelope) so each collector only writes the run-and-map function.
 *
 * Usage:
 *
 *   export default createThreatIntelCollector({
 *     sourceId: 'abuseipdb',
 *     description: 'AbuseIPDB blacklist — JP-IP entries (confidence ≥90)',
 *     envKey: 'ABUSEIPDB_API_KEY',
 *     envHint: 'Set ABUSEIPDB_API_KEY (free at https://...)',
 *     run: async (key) => {
 *       const rows = await fetchUpstream(key);
 *       const features = rows.map(toFeature);
 *       return { features, extraMeta: { total_rows: rows.length } };
 *     },
 *   });
 *
 * The returned function is a plain `() => Promise<FeatureCollection>`, which
 * is the contract every collector in `server/src/collectors/index.js` already
 * exposes — no caller-side change is required.
 */

/**
 * Build a wrapped envelope. `meta` extends the base `_meta` with whatever the
 * caller passes; `source` overrides the `_meta.source` slot when set (e.g. the
 * `_no_key` / `_error` / `_seed` variants used today).
 */
function envelope({ sourceId, description, features = [], meta = {}, source = null }) {
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: source || sourceId,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description,
      ...meta,
    },
  };
}

/**
 * Construct a collector function.
 *
 * @param {object} opts
 * @param {string}        opts.sourceId
 * @param {string}        opts.description
 * @param {string|null}   [opts.envKey]      Env var that holds the API key. If
 *                                           null/undefined, the collector is
 *                                           treated as keyless and `run` is
 *                                           always invoked with `null`.
 * @param {string[]}      [opts.envFallbackKeys=[]]  Extra env vars consulted
 *                                           in order if `envKey` is unset.
 * @param {string|null}   [opts.envHint]     Shown in `_meta.env_hint` when no
 *                                           key is found.
 * @param {(key: string|null) => Promise<{features: any[], extraMeta?: object, source?: string}>}
 *                        opts.run          The actual fetch + map logic. May
 *                                           throw — the factory catches and
 *                                           returns an error envelope.
 * @returns {() => Promise<object>}
 */
export function createThreatIntelCollector({
  sourceId,
  description,
  envKey = null,
  envFallbackKeys = [],
  envHint = null,
  run,
}) {
  if (!sourceId) throw new Error('createThreatIntelCollector: sourceId is required');
  if (typeof run !== 'function') throw new Error('createThreatIntelCollector: run must be a function');

  return async function collect() {
    let key = null;
    if (envKey) {
      const candidates = [envKey, ...envFallbackKeys];
      for (const name of candidates) {
        const v = process.env[name];
        if (v) { key = v; break; }
      }
      if (!key) {
        return envelope({
          sourceId,
          description,
          source: `${sourceId}_no_key`,
          meta: envHint ? { env_hint: envHint } : {},
        });
      }
    }

    try {
      const result = await run(key);
      const features = Array.isArray(result?.features) ? result.features : [];
      const extraMeta = result?.extraMeta || {};
      return envelope({
        sourceId,
        description,
        features,
        meta: extraMeta,
        source: result?.source || sourceId,
      });
    } catch (err) {
      return envelope({
        sourceId,
        description,
        source: `${sourceId}_error`,
        meta: { error: err?.message || 'fetch_failed' },
      });
    }
  };
}
