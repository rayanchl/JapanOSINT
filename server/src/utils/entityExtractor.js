/**
 * Corpus NER + entity resolution workers. Driven by runLlmEnricher() (the
 * existing 5-min guarded cron) — same gate/budget as the other enrichers.
 *
 * enrichEntities(): drains intel_items that have no extraction watermark,
 *   newest-first, so new collector/search items are covered at the next tick
 *   and a full historical backfill walks backwards through the corpus. No
 *   hot-path hook in upsert: an item with no entity_extraction_state row IS
 *   the queue, so heavy LLM/DB work stays entirely in this drain.
 *
 * resolveEntities(): tier-2 fuzzy dedup. Direct clone of enrichStationDedup —
 *   pairwise LLM adjudication into entity_merges, then a union pass folds
 *   same=1 & confidence>=0.7 (the proven station threshold).
 */

import db from './database.js';
import { chat as defaultChat, completeJSON } from './llmClient.js';
import { loadGrammar } from './grammars.js';
import { buildEntityExtractionPrompt } from './llmPrompts.js';
import {
  upsertEntity, addMention, addRelationship,
  candidatePairs, mergePairExists, recordMerge, unionEntities, getEntity,
} from './entityStore.js';

const TIMEOUT_MS = Number(process.env.LLM_TIMEOUT_MS || 30_000);
const DEFAULT_BATCH = Number(process.env.ENTITY_BACKFILL_BATCH || 50);
const CONF = { high: 0.9, medium: 0.6, low: 0.3 };

const stmtPending = db.prepare(`
  SELECT i.uid, i.source_id, i.title, i.body, i.summary, i.language
    FROM intel_items i
    LEFT JOIN entity_extraction_state s ON s.item_uid = i.uid
   WHERE (s.item_uid IS NULL OR (s.extracted_at = '' AND s.failed_count < 5))
     AND (i.title IS NOT NULL OR i.body IS NOT NULL OR i.summary IS NOT NULL)
   ORDER BY i.fetched_at DESC
   LIMIT ?
`);
const stmtMarkDone = db.prepare(`
  INSERT OR REPLACE INTO entity_extraction_state
    (item_uid, extracted_at, extractor_version, failed_count)
  VALUES (?, datetime('now'), 1, 0)
`);
const stmtMarkFail = db.prepare(`
  INSERT INTO entity_extraction_state (item_uid, extracted_at, extractor_version, failed_count)
  VALUES (?, '', 1, 1)
  ON CONFLICT(item_uid) DO UPDATE SET failed_count = failed_count + 1, extracted_at = ''
`);

export async function enrichEntities(opts = {}) {
  const batchSize = opts.batchSize ?? DEFAULT_BATCH;
  const grammar = loadGrammar('entity_extraction');
  const rows = stmtPending.all(batchSize);

  let processed = 0;
  let failed = 0;
  let entitiesAdded = 0;
  for (const row of rows) {
    const { prompt } = buildEntityExtractionPrompt({
      title: row.title, body: row.body, summary: row.summary,
      language: row.language, source_id: row.source_id,
    });
    const out = await completeJSON({
      prompt, grammar, maxTokens: 1024, temperature: 0.1, timeoutMs: TIMEOUT_MS,
    });
    const list = out && Array.isArray(out.entities) ? out.entities : null;
    if (!list) {
      stmtMarkFail.run(row.uid);
      failed += 1;
      continue;
    }

    const tx = db.transaction(() => {
      const ids = [];
      for (const ent of list) {
        if (!ent || !ent.value || !ent.type) continue;
        const id = upsertEntity({ type: ent.type, value: String(ent.value) });
        if (!id) continue;
        addMention({
          entityId: id, itemUid: row.uid, sourceId: row.source_id,
          surface: String(ent.value), field: 'body',
          confidence: CONF[String(ent.confidence)] ?? 0.5, extractor: 'ner-llm',
        });
        ids.push(id);
        entitiesAdded += 1;
      }
      // Co-mention edges among distinct entities in the same item.
      const uniq = [...new Set(ids)];
      for (let a = 0; a < uniq.length; a += 1) {
        for (let b = a + 1; b < uniq.length; b += 1) {
          addRelationship({ srcId: uniq[a], dstId: uniq[b], relType: 'co_mention', weight: 1.0, evidenceUid: row.uid });
        }
      }
      stmtMarkDone.run(row.uid);
    });
    tx();
    processed += 1;
  }
  return { attempted: rows.length, processed, failed, entitiesAdded };
}

// ── Tier-2 resolution (clone of enrichStationDedup) ───────────────────────

const RESOLVE_BATCH = Number(process.env.ENTITY_RESOLVE_BATCH || 40);
const RESOLVE_THRESHOLD = 0.7; // same threshold as llm_station_merges union

const DEDUP_SCHEMA = {
  type: 'object',
  additionalProperties: false,
  required: ['same', 'confidence'],
  properties: {
    same: { type: 'boolean' },
    confidence: { type: 'number', minimum: 0, maximum: 1 },
    reason: { type: 'string', maxLength: 200 },
  },
};

function buildEntityDedupPrompt(pair) {
  return {
    messages: [
      {
        role: 'system',
        content:
          'You decide whether two extracted OSINT entities of the same type ' +
          'refer to the SAME real-world subject. Account for Japanese ' +
          'kanji/kana vs romaji spellings, company suffixes, and ' +
          'transliteration drift. Output JSON only.',
      },
      {
        role: 'user',
        content: `Type: ${pair.type}\nA: ${pair.canon_a}\nB: ${pair.canon_b}\n\nSame subject?`,
      },
    ],
    jsonSchema: DEDUP_SCHEMA,
  };
}

export async function resolveEntities(opts = {}) {
  const llmChat = opts.llmChat || defaultChat;
  const batchSize = opts.batchSize ?? RESOLVE_BATCH;

  let attempted = 0;
  let decided = 0;
  for (const p of candidatePairs(batchSize * 4)) {
    if (attempted >= batchSize) break;
    if (mergePairExists(p.id_a, p.id_b)) continue;
    attempted += 1;
    const { messages, jsonSchema } = buildEntityDedupPrompt(p);
    const out = await llmChat({ messages, jsonSchema, timeoutMs: TIMEOUT_MS });
    if (!out || typeof out.same !== 'boolean' || typeof out.confidence !== 'number') continue;
    recordMerge(p.id_a, p.id_b, out.same, out.confidence, out.reason);
    decided += 1;
  }

  // Union pass: fold confident same-pairs. Skip if either side was already
  // folded away by an earlier pair in this same tick.
  let merged = 0;
  const confirmed = db.prepare(
    `SELECT entity_a, entity_b FROM entity_merges WHERE same = 1 AND confidence >= ?`,
  ).all(RESOLVE_THRESHOLD);
  for (const m of confirmed) {
    if (!getEntity(m.entity_a) || !getEntity(m.entity_b)) continue;
    if (unionEntities(m.entity_a, m.entity_b)) merged += 1;
  }
  return { attempted, decided, merged };
}
