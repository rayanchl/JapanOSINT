/**
 * Unified entity store. Shared-pool (tenant_id is provenance only, never a
 * read filter — same doctrine as intel_items). Tier-1 dedup is a deterministic
 * per-type normalised key enforced by UNIQUE(type, norm_key); tier-2 ambiguity
 * is adjudicated by the LLM resolver (entityResolver.js) via entity_merges,
 * cloning the proven llm_station_merges pattern.
 *
 * Both collector NER (entityExtractor.js) and the OSINT search pipeline
 * (searchIngest.js) write here, so one entity graph spans every source.
 */

import { randomBytes } from 'node:crypto';
import db from './database.js';
import { defineFtsMirror } from './ftsMirror.js';
import { registerMirror } from './ftsRegistry.js';
import { runEntityMigration } from './entityMigration.js';

// better-sqlite3 compiles prepared statements at module-eval time, and ES
// imports evaluate before index.js's runEntityMigration() body call. Ensure
// the entity tables exist now so the statements below compile. Idempotent.
runEntityMigration();

// entities.aliases_json is a JSON array; flatten so the FTS helper can
// kuromoji-segment it like any other text column.
function joinAliasesForFts(raw) {
  try {
    const arr = JSON.parse(raw || '[]');
    return Array.isArray(arr) ? arr.join(' ') : '';
  } catch {
    return '';
  }
}

export const entityMirror = registerMirror(defineFtsMirror({
  name:              'entities_fts',
  baseTable:         'entities',
  baseUidColumn:     'entity_id',
  ftsTable:          'entities_fts',
  textColumns:       ['canonical', 'name_ja', 'name_romaji'],
  keywordsColumn:    'aliases_json',
  keywordsTransform: (raw) => joinAliasesForFts(raw),
  tokenizer:         'unicode61 remove_diacritics 1',
  segment:           true,
  segmenterVersion:  1,
}));

// ── Exact-idempotency key (NOT fuzzy dedup) ───────────────────────────────
// Entity sameness is decided by the LLM resolver ONLY (tier-2) — categorisation
// on the search tab has always been LLM-driven. The deterministic per-type
// normalisation (case/suffix/honorific/kanji rules) was removed: it made
// merge decisions the LLM should own.
//
// `normKey` is now just a minimal idempotency guard so re-running NER on the
// same item doesn't insert a fresh row for a byte-identical surface (which
// would explode the entity table and the resolver's pairwise space). Only
// Unicode-normalised, whitespace-collapsed, EXACTLY-equal strings of the same
// type collapse here; case, suffixes, kanji↔romaji, abbreviations, etc. stay
// as separate rows for the LLM resolver to union (or not).

function nfkcCollapse(s) {
  return String(s).normalize('NFKC').replace(/\s+/g, ' ').trim();
}

/** Exact idempotency key (type is a separate column in UNIQUE(type,norm_key)).
 *  No fuzzy collapsing — the LLM resolver is the only thing that judges
 *  whether two distinct surfaces are the same real-world entity. */
export function normKey(_type, value) {
  return nfkcCollapse(value);
}

function newEntityId() {
  return `ent_${randomBytes(8).toString('hex')}`;
}

// Keep the FTS mirror in sync on every entity write (the intel path does the
// equivalent via intelMirror.writeOne). Idempotent DELETE+INSERT.
function writeEntityFts({ entity_id, canonical, name_ja, name_romaji, aliases_json }) {
  const seg = entityMirror.segmentRowSync({
    uid: entity_id,
    canonical: canonical ?? '',
    name_ja: name_ja ?? '',
    name_romaji: name_romaji ?? '',
    keywords: aliases_json ?? '[]',
  });
  if (seg) entityMirror.writeOne(seg);
}

// Heuristic: a CJK-bearing string is the JA form; otherwise romaji/latin.
const CJK = /[぀-ヿ㐀-䶿一-鿿豈-﫿]/;

// ── Prepared statements ───────────────────────────────────────────────────

const stmtFindByKey = db.prepare(
  `SELECT * FROM entities WHERE type = ? AND norm_key = ?`,
);
const stmtInsertEntity = db.prepare(`
  INSERT INTO entities
    (entity_id, type, canonical, norm_key, name_ja, name_romaji,
     aliases_json, properties, mention_count, first_seen_at, last_seen_at, tenant_id)
  VALUES
    (@entity_id, @type, @canonical, @norm_key, @name_ja, @name_romaji,
     '[]', @properties, 0, datetime('now'), datetime('now'), @tenant_id)
`);
const stmtTouchEntity = db.prepare(`
  UPDATE entities
     SET last_seen_at = datetime('now'),
         aliases_json = @aliases_json,
         name_ja      = COALESCE(name_ja, @name_ja),
         name_romaji  = COALESCE(name_romaji, @name_romaji)
   WHERE entity_id = @entity_id
`);
const stmtGetEntity = db.prepare(`SELECT * FROM entities WHERE entity_id = ?`);
const stmtBumpMentionCount = db.prepare(
  `UPDATE entities SET mention_count = mention_count + 1, last_seen_at = datetime('now') WHERE entity_id = ?`,
);
const stmtInsertMention = db.prepare(`
  INSERT INTO entity_mentions
    (entity_id, item_uid, source_id, surface, field, confidence, extractor, created_at)
  VALUES (@entity_id, @item_uid, @source_id, @surface, @field, @confidence, @extractor, datetime('now'))
  ON CONFLICT(entity_id, item_uid, field) DO UPDATE SET
    confidence = MAX(confidence, excluded.confidence),
    surface    = excluded.surface
`);
const stmtMentionExists = db.prepare(
  `SELECT 1 FROM entity_mentions WHERE entity_id = ? AND item_uid = ? AND field = ?`,
);
const stmtUpsertRel = db.prepare(`
  INSERT INTO entity_relationships
    (src_entity_id, dst_entity_id, rel_type, weight, evidence_uid, first_seen_at, last_seen_at)
  VALUES (@src, @dst, @rel_type, @weight, @evidence_uid, datetime('now'), datetime('now'))
  ON CONFLICT(src_entity_id, dst_entity_id, rel_type) DO UPDATE SET
    weight       = entity_relationships.weight + excluded.weight,
    last_seen_at = datetime('now'),
    evidence_uid = COALESCE(entity_relationships.evidence_uid, excluded.evidence_uid)
`);

/**
 * Upsert one entity by deterministic tier-1 key. Returns the entity_id.
 * `value` is the surface form; `canonical` defaults to it. Idempotent.
 */
export function upsertEntity({ type, value, canonical, properties, tenantId } = {}) {
  const t = String(type || 'unknown').toLowerCase();
  const surface = nfkcCollapse(value);
  if (!surface) return null;
  const key = normKey(t, surface);
  const canon = nfkcCollapse(canonical || surface);
  const isJa = CJK.test(canon);

  const existing = stmtFindByKey.get(t, key);
  if (existing) {
    let aliases = [];
    try { aliases = JSON.parse(existing.aliases_json || '[]'); } catch { aliases = []; }
    if (surface !== existing.canonical && !aliases.includes(surface)) aliases.push(surface);
    const aliasesJson = JSON.stringify(aliases.slice(0, 50));
    stmtTouchEntity.run({
      entity_id: existing.entity_id,
      aliases_json: aliasesJson,
      name_ja: isJa ? canon : null,
      name_romaji: isJa ? null : canon,
    });
    writeEntityFts({
      entity_id: existing.entity_id,
      canonical: existing.canonical,
      name_ja: existing.name_ja ?? (isJa ? canon : null),
      name_romaji: existing.name_romaji ?? (isJa ? null : canon),
      aliases_json: aliasesJson,
    });
    return existing.entity_id;
  }

  const entity_id = newEntityId();
  stmtInsertEntity.run({
    entity_id,
    type: t,
    canonical: canon,
    norm_key: key,
    name_ja: isJa ? canon : null,
    name_romaji: isJa ? null : canon,
    properties: JSON.stringify(properties || {}),
    tenant_id: tenantId ?? null,
  });
  writeEntityFts({
    entity_id,
    canonical: canon,
    name_ja: isJa ? canon : null,
    name_romaji: isJa ? null : canon,
    aliases_json: '[]',
  });
  return entity_id;
}

/** Record that an entity occurs in an intel_items row. Bumps mention_count
 *  only on a genuinely new (entity,item,field) edge. */
export function addMention({ entityId, itemUid, sourceId, surface, field, confidence, extractor }) {
  if (!entityId || !itemUid) return;
  const fld = field || 'body';
  const isNew = !stmtMentionExists.get(entityId, itemUid, fld);
  stmtInsertMention.run({
    entity_id: entityId,
    item_uid: String(itemUid),
    source_id: sourceId || 'unknown',
    surface: surface != null ? String(surface).slice(0, 500) : null,
    field: fld,
    confidence: Number.isFinite(confidence) ? confidence : 0.5,
    extractor: extractor || 'ner-llm',
  });
  if (isNew) stmtBumpMentionCount.run(entityId);
}

/** Upsert an entity<->entity edge. Co-occurrence accumulates weight. */
export function addRelationship({ srcId, dstId, relType, weight, evidenceUid }) {
  if (!srcId || !dstId || srcId === dstId) return;
  stmtUpsertRel.run({
    src: srcId,
    dst: dstId,
    rel_type: relType || 'co_mention',
    weight: Number.isFinite(weight) ? weight : 1.0,
    evidence_uid: evidenceUid ?? null,
  });
}

export function getEntity(entityId) {
  return stmtGetEntity.get(entityId) || null;
}

const stmtMentions = db.prepare(`
  SELECT m.item_uid, m.source_id, m.surface, m.field, m.confidence, m.extractor,
         m.created_at, i.title, i.summary, i.link, i.published_at, i.record_type
    FROM entity_mentions m
    LEFT JOIN intel_items i ON i.uid = m.item_uid
   WHERE m.entity_id = ?
   ORDER BY m.created_at DESC
   LIMIT ? OFFSET ?
`);
export function listMentions(entityId, { limit = 50, offset = 0 } = {}) {
  return stmtMentions.all(entityId, limit, offset);
}

const stmtNeighbours = db.prepare(`
  SELECT src_entity_id AS a, dst_entity_id AS b, rel_type, weight
    FROM entity_relationships
   WHERE src_entity_id = @id OR dst_entity_id = @id
   ORDER BY weight DESC
   LIMIT @fanout
`);

/** BFS ego-network up to `depth` hops, capped fan-out per node. Returns
 *  {nodes, edges} ready for the graph UI. */
export function getGraph(entityId, { depth = 1, fanout = 25 } = {}) {
  const root = getEntity(entityId);
  if (!root) return { nodes: [], edges: [] };
  const nodes = new Map();
  const edges = [];
  const seenEdge = new Set();
  const add = (e) => { if (e && !nodes.has(e.entity_id)) nodes.set(e.entity_id, e); };
  add(root);

  let frontier = [entityId];
  const visited = new Set([entityId]);
  const d = Math.max(1, Math.min(3, depth));
  for (let hop = 0; hop < d; hop += 1) {
    const next = [];
    for (const id of frontier) {
      for (const r of stmtNeighbours.all({ id, fanout })) {
        const other = r.a === id ? r.b : r.a;
        const ek = `${r.a}|${r.b}|${r.rel_type}`;
        if (!seenEdge.has(ek)) {
          seenEdge.add(ek);
          edges.push({ source: r.a, target: r.b, relationship: r.rel_type, weight: r.weight });
        }
        add(getEntity(other));
        if (!visited.has(other)) { visited.add(other); next.push(other); }
      }
    }
    frontier = next;
    if (frontier.length === 0) break;
  }
  return {
    nodes: [...nodes.values()].map((e) => ({
      id: e.entity_id, type: e.type, value: e.canonical,
      label: e.canonical, mention_count: e.mention_count,
    })),
    edges,
  };
}

/** FTS entity search (kuromoji-segmented; kanji-substring works for free). */
export async function searchEntities({ q, type = null, limit = 30 } = {}) {
  const where = [];
  const params = {};
  if (type) { where.push('entities.type = @type'); params.type = String(type).toLowerCase(); }
  const { rows } = await entityMirror.search({
    q,
    where,
    params,
    orderBy: 'entities.mention_count DESC',
    limit,
    selectColumns: 'entities.*',
  });
  return rows.map((e) => ({
    entity_id: e.entity_id, type: e.type, value: e.canonical,
    mention_count: e.mention_count, last_seen_at: e.last_seen_at, excerpt: e._excerpt,
  }));
}

// ── Tier-2 resolution helpers (clone of llm_station_merges) ───────────────

const stmtMergeExists = db.prepare(
  `SELECT 1 FROM entity_merges WHERE entity_a = ? AND entity_b = ?`,
);
const stmtRecordMerge = db.prepare(`
  INSERT OR REPLACE INTO entity_merges
    (entity_a, entity_b, same, confidence, reason, decided_at)
  VALUES (?, ?, ?, ?, ?, datetime('now'))
`);
// Candidate pairs the deterministic key didn't merge: same type, sharing a
// leading token, neither already adjudicated. Cheap pre-filter; the LLM
// decides. Order entity ids so (a,b) is stable.
const stmtCandidatePairs = db.prepare(`
  SELECT e1.entity_id AS id_a, e1.type AS type, e1.canonical AS canon_a,
         e2.entity_id AS id_b, e2.canonical AS canon_b
    FROM entities e1
    JOIN entities e2
      ON e1.type = e2.type
     AND e1.entity_id < e2.entity_id
     AND substr(e1.norm_key,1,4) = substr(e2.norm_key,1,4)
   WHERE e1.type IN ('person','company','address')
   LIMIT ?
`);

export function mergePairExists(a, b) {
  return !!stmtMergeExists.get(a, b);
}
export function recordMerge(a, b, same, confidence, reason) {
  stmtRecordMerge.run(a, b, same ? 1 : 0, confidence,
    typeof reason === 'string' ? reason.slice(0, 200) : null);
}
export function candidatePairs(limit = 200) {
  return stmtCandidatePairs.all(limit);
}

const stmtRepointMentions = db.prepare(
  `UPDATE OR IGNORE entity_mentions SET entity_id = ? WHERE entity_id = ?`,
);
const stmtRepointRelSrc = db.prepare(
  `UPDATE OR IGNORE entity_relationships SET src_entity_id = ? WHERE src_entity_id = ?`,
);
const stmtRepointRelDst = db.prepare(
  `UPDATE OR IGNORE entity_relationships SET dst_entity_id = ? WHERE dst_entity_id = ?`,
);
const stmtDeleteEntity = db.prepare(`DELETE FROM entities WHERE entity_id = ?`);
const stmtRecount = db.prepare(`
  UPDATE entities SET mention_count =
    (SELECT COUNT(*) FROM entity_mentions WHERE entity_id = ?) WHERE entity_id = ?
`);

/** Fold loser into survivor (lexically-smaller id wins). Reversible audit
 *  trail: loser's canonical is appended to survivor.aliases_json. */
export function unionEntities(idA, idB) {
  const survivor = idA < idB ? idA : idB;
  const loser = idA < idB ? idB : idA;
  const sEnt = getEntity(survivor);
  const lEnt = getEntity(loser);
  if (!sEnt || !lEnt) return false;
  const tx = db.transaction(() => {
    stmtRepointMentions.run(survivor, loser);
    stmtRepointRelSrc.run(survivor, loser);
    stmtRepointRelDst.run(survivor, loser);
    let aliases = [];
    try { aliases = JSON.parse(sEnt.aliases_json || '[]'); } catch { aliases = []; }
    if (!aliases.includes(lEnt.canonical)) aliases.push(lEnt.canonical);
    const aliasesJson = JSON.stringify(aliases.slice(0, 50));
    db.prepare(`UPDATE entities SET aliases_json = ? WHERE entity_id = ?`)
      .run(aliasesJson, survivor);
    stmtDeleteEntity.run(loser);
    stmtRecount.run(survivor, survivor);
    entityMirror.deleteOne(loser);
    writeEntityFts({
      entity_id: survivor, canonical: sEnt.canonical,
      name_ja: sEnt.name_ja, name_romaji: sEnt.name_romaji, aliases_json: aliasesJson,
    });
  });
  tx();
  return true;
}
