// server/src/utils/searchIngest.js
//
// Bridges a completed OSINT-search run into the unified store: results land in
// intel_items under the synthetic `osint-search` source (so they get FTS, the
// Sources panel, alerts) and every entity flows into the entity graph (so
// collector data and search data share ONE graph). Call on the pipeline's
// 'done' event.

import { upsertItems } from './intelStore.js';
import { upsertEntity, addMention, addRelationship } from './entityStore.js';

const SOURCE_ID = 'osint-search';

/**
 * @param {object} snap  RequestProgress.toJSON() of a finished run
 * @returns {Promise<{items:number, entities:number}>}
 */
export async function ingestSearchRun(snap) {
  if (!snap?.request_id) return { items: 0, entities: 0 };
  const now = new Date().toISOString();
  const runUid = `${SOURCE_ID}|run:${snap.request_id}`;
  const svcResults = Array.isArray(snap.results?.services) ? snap.results.services : [];

  // 1. Run summary row + one row per service result.
  const items = [{
    uid: runUid,
    title: `OSINT search: ${snap.query}`,
    summary: snap.results?.synthesis || snap.preliminary_findings || '',
    body: snap.results?.synthesis || '',
    record_type: 'osint_search_run',
    published_at: now,
    properties: {
      query: snap.query,
      phase: snap.phase,
      rounds: snap.current_round,
      stats: snap.stats,
      entities: snap.all_entities,
    },
    tags: ['osint-search'],
  }];
  svcResults.forEach((r, i) => {
    items.push({
      uid: `${SOURCE_ID}|${snap.request_id}|svc:${r.name}:${i}`,
      title: `${r.name} — ${r.entity}`,
      body: typeof r.data === 'string' ? r.data : JSON.stringify(r.data ?? null),
      summary: r.success ? `${r.name} returned data for ${r.entity}` : `${r.name} ${r.error || 'no data'}`,
      record_type: 'osint_service_result',
      published_at: now,
      properties: { service: r.name, entity: r.entity, success: r.success, confidence: r.confidence, error: r.error || null },
      tags: ['osint-search', r.name],
    });
  });

  const { count } = await upsertItems(items, SOURCE_ID, now);

  // 2. Entity graph. Query entities + discovered entities → mentions on the
  //    run row; seed→discovered edges = pivot_discovered (the relationship
  //    store that collectors don't produce).
  let entityCount = 0;
  const idByValue = new Map();
  const linkEntity = (value, type, field, conf) => {
    if (!value) return null;
    const id = upsertEntity({ type: type || 'unknown', value: String(value) });
    if (!id) return null;
    addMention({
      entityId: id, itemUid: runUid, sourceId: SOURCE_ID,
      surface: String(value), field, confidence: conf, extractor: 'osint-search',
    });
    idByValue.set(`${type}|${String(value).toLowerCase()}`, id);
    entityCount += 1;
    return id;
  };

  const seeds = [];
  for (const e of snap.entities || []) {
    const id = linkEntity(e.value, e.type, 'query', 0.9);
    if (id) seeds.push(id);
  }
  for (const e of snap.discovered_entities || []) {
    const id = linkEntity(e.value, e.type, `service:${e.discovered_by || 'phase2'}`, 0.6);
    if (id) {
      for (const s of seeds) {
        addRelationship({ srcId: s, dstId: id, relType: 'pivot_discovered', weight: 1.0, evidenceUid: runUid });
      }
    }
  }

  return { items: count, entities: entityCount };
}
