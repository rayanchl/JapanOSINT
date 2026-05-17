/**
 * Entity-graph schema migration. Additive only, run unconditionally at boot,
 * idempotent via `CREATE TABLE IF NOT EXISTS`. Same mechanism and discipline
 * as runTenancyMigration() — this repo has no migration framework; boot-time
 * idempotent SQL is the established pattern.
 *
 * Called from server/src/index.js immediately after runTenancyMigration(),
 * before any route/worker touches these tables and before the FTS registry's
 * boot rebuild (entityStore defines an `entities_fts` mirror over `entities`).
 *
 * Shared-pool, exactly like intel_items: `tenant_id` is provenance only and
 * is NEVER a read filter. The corpus/entity graph is the product.
 */

import db from './database.js';

export function runEntityMigration() {
  db.exec(`
    -- One row per resolved real-world subject. type vocabulary = the exact
    -- lowercase enum from OSINTsaas osint_analysis.gbnf so search-discovered
    -- and NER-extracted entities share one type space.
    CREATE TABLE IF NOT EXISTS entities (
      entity_id     TEXT PRIMARY KEY,
      type          TEXT NOT NULL,
      canonical     TEXT NOT NULL,
      norm_key      TEXT NOT NULL,
      name_ja       TEXT,
      name_romaji   TEXT,
      aliases_json  TEXT NOT NULL DEFAULT '[]',
      properties    TEXT NOT NULL DEFAULT '{}',
      mention_count INTEGER NOT NULL DEFAULT 0,
      first_seen_at TEXT NOT NULL DEFAULT (datetime('now')),
      last_seen_at  TEXT NOT NULL DEFAULT (datetime('now')),
      tenant_id     TEXT
    );
    -- Deterministic tier-1 dedup: one row per (type, normalised key).
    CREATE UNIQUE INDEX IF NOT EXISTS idx_entities_normkey
      ON entities(type, norm_key);
    CREATE INDEX IF NOT EXISTS idx_entities_type_seen
      ON entities(type, last_seen_at DESC);

    -- An entity occurs in an intel_items row (collector OR osint-search).
    -- item_uid is a plain column (no FK): retention prunes intel_items
    -- independently and GCs orphaned mentions explicitly.
    CREATE TABLE IF NOT EXISTS entity_mentions (
      entity_id   TEXT NOT NULL REFERENCES entities(entity_id) ON DELETE CASCADE,
      item_uid    TEXT NOT NULL,
      source_id   TEXT NOT NULL,
      surface     TEXT,
      field       TEXT,
      confidence  REAL NOT NULL DEFAULT 0.5,
      extractor   TEXT NOT NULL,
      created_at  TEXT NOT NULL DEFAULT (datetime('now')),
      PRIMARY KEY (entity_id, item_uid, field)
    );
    CREATE INDEX IF NOT EXISTS idx_em_item   ON entity_mentions(item_uid);
    CREATE INDEX IF NOT EXISTS idx_em_entity ON entity_mentions(entity_id, created_at DESC);
    CREATE INDEX IF NOT EXISTS idx_em_source ON entity_mentions(source_id);

    -- Entity <-> entity edges (co-mention, pivot-discovered, resolves-to, …).
    CREATE TABLE IF NOT EXISTS entity_relationships (
      src_entity_id TEXT NOT NULL REFERENCES entities(entity_id) ON DELETE CASCADE,
      dst_entity_id TEXT NOT NULL REFERENCES entities(entity_id) ON DELETE CASCADE,
      rel_type      TEXT NOT NULL,
      weight        REAL NOT NULL DEFAULT 1.0,
      evidence_uid  TEXT,
      first_seen_at TEXT NOT NULL DEFAULT (datetime('now')),
      last_seen_at  TEXT NOT NULL DEFAULT (datetime('now')),
      PRIMARY KEY (src_entity_id, dst_entity_id, rel_type)
    );
    CREATE INDEX IF NOT EXISTS idx_er_src ON entity_relationships(src_entity_id, weight DESC);
    CREATE INDEX IF NOT EXISTS idx_er_dst ON entity_relationships(dst_entity_id, weight DESC);

    -- Tier-2 LLM resolution adjudication — direct clone of llm_station_merges.
    CREATE TABLE IF NOT EXISTS entity_merges (
      entity_a    TEXT NOT NULL,
      entity_b    TEXT NOT NULL,
      same        INTEGER NOT NULL CHECK(same IN (0,1)),
      confidence  REAL NOT NULL,
      reason      TEXT,
      decided_at  TEXT NOT NULL DEFAULT (datetime('now')),
      PRIMARY KEY (entity_a, entity_b)
    );

    -- Per-item NER watermark so the worker/backfill never re-scan. Separate
    -- table (not an intel_items column) to avoid an ALTER on the hot master
    -- and keep extractor versioning independent. failed_count>=5 => skip
    -- (mirrors intel_items.keywords_failed discipline).
    CREATE TABLE IF NOT EXISTS entity_extraction_state (
      item_uid          TEXT PRIMARY KEY,
      extracted_at      TEXT NOT NULL DEFAULT (datetime('now')),
      extractor_version INTEGER NOT NULL DEFAULT 1,
      failed_count      INTEGER NOT NULL DEFAULT 0
    );
  `);
  console.log('[entity] schema migration applied');
}
