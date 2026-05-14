import { Router } from 'express';
import { listSources, listItems, getItem } from '../utils/intelStore.js';
import sources from '../utils/sourceRegistry.js';
import { INTEL_SOURCE_SET } from '../utils/intelCatalog.js';
import { collectors } from '../collectors/index.js';
import { withCollectorRun } from '../utils/collectorTap.js';
import { mirrorCollectorOutput } from '../utils/collectorMirror.js';
import { getTtlMs } from '../utils/collectorCache.js';
import { readinessMiddleware } from '../utils/ftsRegistry.js';

const router = Router();

// /items only hits FTS when ?q= or ?qAlt= is present; the predicate keeps
// non-search listings unaffected by warm-up. qAlt is the auto-translated
// counterpart the iOS app sends alongside q for the bilingual search UX.
const intelFtsGate = readinessMiddleware('intel_items_fts', {
  gateWhen: (req) => Boolean(
    (req.query?.q && String(req.query.q).trim())
    || (req.query?.qAlt && String(req.query.qAlt).trim())
  ),
});

const sourceById = new Map(sources.map((s) => [s.id, s]));

/**
 * GET /api/intel/sources
 * Lists every source in the registry (post-overhaul: every collector mirrors
 * into intel_items, so every source belongs in the intel tab — geocoded rows
 * also render on the map). Joins registry metadata with per-source aggregates
 * (item_count / geocoded / ungeocoded / awaiting_geo / last_fetched).
 *
 * ?intelOnly=1 keeps the legacy whitelist behaviour for clients that still
 * want only the historically non-spatial sources.
 *
 * Sort: most-recently-fetched first; never-collected sources at the bottom
 * alphabetically. Lets the user see "what's fresh" at the top.
 */
router.get('/sources', (req, res) => {
  try {
    const aggregates = listSources();
    const aggMap = new Map(aggregates.map((a) => [a.source_id, a]));
    const intelOnly = req.query.intelOnly === '1' || req.query.intelOnly === 'true';
    const registryIds = sources.map((s) => s.id);
    const registrySet = new Set(registryIds);
    // Surface unregistered source_ids that exist in intel_items too — these
    // come from the Phase B backfill (cameras → 'camera-discovery', historical
    // transport rows under synthetic ids) and any other bucket the mirror has
    // created that isn't in sourceRegistry. Without this they'd be hidden.
    const orphanIds = aggregates
      .map((a) => a.source_id)
      .filter((id) => !registrySet.has(id));
    const ids = intelOnly
      ? registryIds.filter((id) => INTEL_SOURCE_SET.has(id))
      : [...registryIds, ...orphanIds];

    const data = ids.map((id) => {
      const meta = sourceById.get(id) || {};
      const agg = aggMap.get(id) || {
        item_count: 0, geocoded: 0, ungeocoded: 0, awaiting_geo: 0,
        last_fetched: null, last_published: null,
      };
      return {
        id,
        name: meta.name || id,
        name_ja: meta.nameJa || null,
        category: meta.category || null,
        description: meta.description || null,
        url: meta.url || null,
        item_count:    agg.item_count,
        geocoded:      agg.geocoded,
        ungeocoded:    agg.ungeocoded,
        awaiting_geo:  agg.awaiting_geo,
        last_fetched:  agg.last_fetched,
        last_published: agg.last_published,
        ttl_ms: getTtlMs(id),
        is_intel: INTEL_SOURCE_SET.has(id),
      };
    });

    data.sort((a, b) => {
      const aFresh = a.last_fetched || a.last_published;
      const bFresh = b.last_fetched || b.last_published;
      if (aFresh && bFresh) return bFresh.localeCompare(aFresh);
      if (aFresh) return -1;
      if (bFresh) return 1;
      return a.name.localeCompare(b.name);
    });

    res.json({ data, meta: { fetched_at: new Date().toISOString(), total: data.length } });
  } catch (err) {
    console.error('[intel] /sources failed:', err.message);
    res.status(500).json({ error: 'failed_to_list_intel_sources' });
  }
});

/**
 * POST /api/intel/sources/:id/run
 * User-initiated trigger: runs the named intel collector now and upserts
 * its output into intel_items. Single-flighted per source via in-memory
 * Set; concurrent calls for the same id reject with 409.
 *
 * Wraps the collector in withCollectorRun(...) so the run shows up in the
 * Follow log / collector-tap WS stream just like a scheduled run.
 */
const inFlight = new Set();

router.post('/sources/:id/run', async (req, res) => {
  const id = req.params.id;
  const collectorFn = collectors[id];
  if (!collectorFn) {
    return res.status(404).json({ error: 'no_collector_registered', source_id: id });
  }
  if (inFlight.has(id)) {
    return res.status(409).json({ error: 'run_in_flight', source_id: id });
  }

  inFlight.add(id);
  try {
    const startedAt = Date.now();
    const out = await withCollectorRun(id, () => collectorFn(), { trigger: 'manual' });
    // Every source flows through the polymorphic mirror — FC features and
    // intel envelopes both land in intel_items. Whitelist gate retired.
    const fetchedAt = out?.meta?.fetchedAt || out?._meta?.fetchedAt || new Date().toISOString();
    const counts = await mirrorCollectorOutput(out, id, fetchedAt);
    const ingested = (counts?.features?.count || 0) + (counts?.intel?.count || 0);
    const geocoded = (counts?.features?.geocoded || 0) + (counts?.intel?.geocoded || 0);
    const ungeocoded = (counts?.features?.ungeocoded || 0) + (counts?.intel?.ungeocoded || 0);
    return res.json({
      ran: true,
      source_id: id,
      ingested,
      geocoded,
      ungeocoded,
      kind: out?.kind || (Array.isArray(out?.features) ? 'feature_collection' : null),
      duration_ms: Date.now() - startedAt,
      meta: out?.meta || out?._meta || null,
    });
  } catch (err) {
    console.error(`[intel] /sources/${id}/run failed:`, err?.stack || err?.message || err);
    return res.status(500).json({ ran: false, source_id: id, error: 'collector_run_failed' });
  } finally {
    inFlight.delete(id);
  }
});

/**
 * GET /api/intel/items?source=&q=&qAlt=&lang=&since=&until=&tag=&limit=&cursor=
 * Paginated unified feed. FTS when q is set.
 *
 * Bilingual search: when both `q` and `qAlt` are provided, run two FTS
 * queries in parallel, merge by uid, and tag rows that matched only `qAlt`
 * with `via_translation: true`. Cursor pagination is dropped in this mode
 * (next_cursor returns null); the iOS bilingual search UX fetches a single
 * large page (limit=100) and doesn't paginate.
 */
router.get('/items', intelFtsGate, async (req, res) => {
  try {
    const sourcesParam = req.query.sources
      ? String(req.query.sources).split(',').map((s) => s.trim()).filter(Boolean)
      : null;

    const q    = req.query.q    ? String(req.query.q).trim()    : null;
    const qAlt = req.query.qAlt ? String(req.query.qAlt).trim() : null;
    const limitParam = req.query.limit ? Number(req.query.limit) : 50;

    const commonFilters = {
      source: req.query.source ? String(req.query.source) : null,
      sources: sourcesParam,
      tag:    req.query.tag    ? String(req.query.tag)    : null,
      since:  req.query.since  ? String(req.query.since)  : null,
      until:  req.query.until  ? String(req.query.until)  : null,
      lang:   req.query.lang   ? String(req.query.lang)   : null,
      recordType:   req.query.record_type   ? String(req.query.record_type)   : null,
      subSourceId:  req.query.sub_source_id ? String(req.query.sub_source_id) : null,
      hasGeom:      req.query.has_geom      ? String(req.query.has_geom)      : null,
      limit: limitParam,
    };

    let items;
    let nextCursor = null;
    let total = null;

    if (q && qAlt && q !== qAlt) {
      // Bilingual path: fetch both, merge by uid, sort by event time desc.
      const [primary, secondary] = await Promise.all([
        listItems({ ...commonFilters, q,    cursor: null }),
        listItems({ ...commonFilters, q: qAlt, cursor: null }),
      ]);
      const seen = new Set();
      const merged = [];
      for (const it of primary.items) {
        if (seen.has(it.uid)) continue;
        seen.add(it.uid);
        merged.push({ ...it, via_translation: false });
      }
      for (const it of secondary.items) {
        if (seen.has(it.uid)) continue;
        seen.add(it.uid);
        merged.push({ ...it, via_translation: true, matched_alt: qAlt });
      }
      merged.sort((a, b) => {
        const ta = a.published_at || a.fetched_at || '';
        const tb = b.published_at || b.fetched_at || '';
        return tb.localeCompare(ta);
      });
      items = merged.slice(0, limitParam);
    } else {
      const result = await listItems({
        ...commonFilters,
        q,
        cursor: req.query.cursor ? String(req.query.cursor) : null,
      });
      items = result.items;
      nextCursor = result.nextCursor;
      total = result.total;
    }

    res.json({
      data: items,
      page: {
        next_cursor: nextCursor,
        limit: limitParam,
        total,
      },
      meta: {
        fetched_at: new Date().toISOString(),
        filters: {
          source: req.query.source || null,
          q: q || null,
          q_alt: qAlt || null,
          lang: req.query.lang || null,
          record_type:   req.query.record_type   || null,
          sub_source_id: req.query.sub_source_id || null,
          has_geom:      req.query.has_geom      || null,
        },
      },
    });
  } catch (err) {
    console.error('[intel] /items failed:', err.message);
    res.status(500).json({ error: 'failed_to_list_intel_items' });
  }
});

/**
 * GET /api/intel/items/:uid
 * Single item with full body and properties.
 */
router.get('/items/:uid', (req, res) => {
  try {
    const item = getItem(req.params.uid);
    if (!item) return res.status(404).json({ error: 'not_found' });
    res.json({ data: item });
  } catch (err) {
    console.error('[intel] /items/:uid failed:', err.message);
    res.status(500).json({ error: 'failed_to_load_intel_item' });
  }
});

/** Alias for symmetry. */
router.get('/search', (req, res, next) => {
  req.url = `/items?${new URLSearchParams(req.query).toString()}`;
  router.handle(req, res, next);
});

export default router;
