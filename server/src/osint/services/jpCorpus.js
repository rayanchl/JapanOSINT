// server/src/osint/services/jpCorpus.js
//
// The always-on Japan adapter. Every entity the pipeline touches is also run
// through this so the OSINT search inherently reaches JapanOSINT's 308
// collectors + the unified entity graph — the whole reason the brain lives in
// Node rather than a sealed C sidecar.

import { intelMirror } from '../../utils/intelStore.js';
import { searchEntities, getEntity, getGraph } from '../../utils/entityStore.js';

/**
 * @param {string} entity  the value to look up (e.g. "Tanaka Taro", "8.8.8.8")
 * @returns {Promise<{success,data,confidence,error?}>}
 */
export async function jpCorpusLookup(entity) {
  try {
    // 1. Full-text hits across the entire collected corpus.
    const { rows } = await intelMirror.search({
      q: entity,
      limit: 15,
      selectColumns: 'intel_items.uid, intel_items.source_id, intel_items.title, ' +
        'intel_items.summary, intel_items.link, intel_items.published_at, intel_items.record_type',
    });
    const items = rows.map((r) => ({
      uid: r.uid, source_id: r.source_id, title: r.title,
      summary: r.summary, link: r.link, published_at: r.published_at,
      record_type: r.record_type, excerpt: r._excerpt,
    }));

    // 2. Already-known entity + its relationship neighbourhood, if resolved.
    let entityGraph = null;
    const matched = await searchEntities({ q: entity, limit: 1 });
    if (matched.length) {
      const full = getEntity(matched[0].entity_id);
      entityGraph = {
        entity: full
          ? { id: full.entity_id, type: full.type, value: full.canonical, mention_count: full.mention_count }
          : null,
        graph: getGraph(matched[0].entity_id, { depth: 1 }),
      };
    }

    const found = items.length > 0 || !!entityGraph;
    return {
      success: true,
      confidence: found ? 80 : 30,
      data: JSON.stringify({
        query: entity,
        corpus_hits: items.length,
        items,
        entity_graph: entityGraph,
      }),
    };
  } catch (err) {
    return { success: false, error: err?.message || String(err), confidence: 0 };
  }
}
