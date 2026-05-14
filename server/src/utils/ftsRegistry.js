/**
 * Boot orchestrator + readiness gate for FTS mirrors defined in
 * utils/ftsMirror.js. Each store module imports defineFtsMirror, then
 * registers the resulting mirror here so the boot sequence can drive
 * sequential rebuilds and search routes can gate themselves with the
 * 503 middleware.
 *
 * Sequential (not parallel) rebuild order keeps SQLite WAL contention
 * predictable and avoids kuromoji segmentation thrash from N parallel
 * batches racing the dictionary cache.
 */

const mirrors = new Map();

export function registerMirror(mirror) {
  if (!mirror?.name) throw new Error('[ftsRegistry] mirror missing name');
  if (mirrors.has(mirror.name)) {
    // Re-register is a no-op so module reloads (HMR-style) don't blow up.
    return mirrors.get(mirror.name);
  }
  mirrors.set(mirror.name, mirror);
  return mirror;
}

export function getMirror(name) {
  return mirrors.get(name) || null;
}

export function listMirrors() {
  return Array.from(mirrors.values());
}

export function isReady(name) {
  const m = mirrors.get(name);
  return Boolean(m && m.isReady());
}

/**
 * Drive each registered mirror's rebuild in series. Resolves when all are
 * complete. Errors per mirror are caught and logged but don't stop the
 * orchestrator — callers should still mark routes ready for unrelated
 * tables. The mirror that errored stays not-ready and its routes will
 * keep returning 503 until the next boot.
 */
export async function rebuildAllAtBoot() {
  for (const mirror of mirrors.values()) {
    const t0 = Date.now();
    try {
      const res = await mirror.rebuildFromBase();
      const dur = Date.now() - t0;
      if (res?.skipped) {
        // No work needed (fingerprint matched, counts ok). Mark ready.
        mirror.markReady({ rebuilt: 0, duration_ms: dur });
        continue;
      }
      console.log(`[ftsRegistry] ${mirror.name} rebuilt: ${res.rebuilt}/${res.baseCount} rows in ${dur}ms`);
      mirror.markReady({ rebuilt: res.rebuilt, duration_ms: dur });
    } catch (err) {
      console.warn(`[ftsRegistry] ${mirror.name} rebuild failed:`, err?.message);
      mirror.markFailed(err);
    }
  }
}

/**
 * Express middleware factory. Returns 503 + Retry-After until every named
 * mirror is ready. Use on routes whose handler issues a MATCH against any
 * of those mirrors.
 *
 *   router.get('/api/intel/items', readinessMiddleware('intel_items_fts'), handler);
 *
 * Optional `gateWhen(req)` predicate skips the guard when false — useful for
 * endpoints where FTS is only consulted for specific query params (e.g.
 * /api/intel/items only hits FTS when ?q= is present).
 *
 *   readinessMiddleware('intel_items_fts', {
 *     gateWhen: (req) => Boolean(req.query?.q?.trim()),
 *   })
 */
export function readinessMiddleware(...args) {
  let names = args;
  let gateWhen = null;
  const last = args[args.length - 1];
  if (last && typeof last === 'object' && !Array.isArray(last)) {
    names = args.slice(0, -1);
    gateWhen = last.gateWhen ?? null;
  }
  return function ftsReadinessGuard(req, res, next) {
    if (gateWhen && !gateWhen(req)) return next();
    const notReady = [];
    for (const name of names) {
      const m = mirrors.get(name);
      if (!m) {
        // Unknown mirror name — treat as not ready (config bug, fail loud)
        notReady.push({ name, error: 'unregistered' });
        continue;
      }
      if (!m.isReady()) {
        notReady.push({ name, ...m.getProgress() });
      }
    }
    if (notReady.length === 0) return next();
    res.set('Retry-After', '5');
    res.status(503).json({
      error: 'fts_warming',
      message: 'Full-text index is rebuilding. Try again in a few seconds.',
      tables: notReady,
    });
  };
}
