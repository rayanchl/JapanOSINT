/**
 * Reusable FTS5 mirror helper.
 *
 * Each store module that wants full-text search defines a mirror once:
 *
 *   const mirror = defineFtsMirror({
 *     name: 'social_posts_fts',
 *     baseTable: 'social_posts',
 *     baseUidColumn: 'post_uid',          // identifier or SQL expression
 *     ftsTable: 'social_posts_fts',
 *     textColumns: ['text', 'title', 'author', 'llm_place_name'],
 *     keywordsColumn: null,               // 'keywords' for intel_items
 *     tokenizer: 'unicode61 remove_diacritics 1',
 *     segment: true,                      // run kuromoji on each text column
 *     stripHtml: false,
 *     segmenterVersion: 1,                // bump to force fleet-wide rebuild
 *   });
 *
 * Then in the caller's upsert path:
 *
 *   const segs = await mirror.segmentRows(items);
 *   const tx = db.transaction((items, segs) => {
 *     for (let i = 0; i < items.length; i++) {
 *       upsertBaseStmt.run(items[i]);
 *       mirror.writeOne(segs[i]);   // sync, inside the txn
 *     }
 *   });
 *   tx(items, segs);
 *
 * Concurrency model: both writeOne and the rebuild loop do DELETE+INSERT
 * per uid, each wrapped in a caller-owned db.transaction(). better-sqlite3
 * serializes write transactions, so writers and rebuild interleave safely
 * without producing duplicate FTS rows. Search routes return 503 until each
 * mirror's readyPromise resolves; writers continue throughout the warm-up.
 */

import crypto from 'node:crypto';
import db from './database.js';
import { segmentForFts, segmentForFtsSync } from './jpTokenizer.js';
import { broadcastEvent } from './collectorTap.js';

function computeFingerprint({ columns, tokenizer, segmenterVersion, hasKeywords }) {
  const payload = JSON.stringify({ columns, tokenizer, segmenterVersion, hasKeywords });
  return crypto.createHash('sha256').update(payload).digest('hex');
}

function stripHtmlBasic(s) {
  if (!s || typeof s !== 'string') return s;
  return s.replace(/<[^>]*>/g, ' ').replace(/\s+/g, ' ').trim();
}

const metaSelectStmt = db.prepare('SELECT fingerprint, version FROM _fts_meta WHERE name = ?');
const metaUpsertStmt = db.prepare(
  `INSERT INTO _fts_meta (name, fingerprint, columns, tokenizer, version, rebuilt_at)
   VALUES (@name, @fingerprint, @columns, @tokenizer, @version, @rebuilt_at)
   ON CONFLICT(name) DO UPDATE SET
     fingerprint = excluded.fingerprint,
     columns     = excluded.columns,
     tokenizer   = excluded.tokenizer,
     version     = excluded.version,
     rebuilt_at  = excluded.rebuilt_at`,
);
const metaDeleteStmt = db.prepare('DELETE FROM _fts_meta WHERE name = ?');

export function defineFtsMirror({
  name,
  baseTable,
  baseUidColumn,
  ftsTable,
  textColumns,
  keywordsColumn = null,
  // Maps the raw value of the base table's keywords column (often a JSON
  // array string) to a flat, unsegmented string ready for kuromoji. The
  // helper then segments the result before writing it to FTS. Defaults to
  // pass-through (assumes keywords is already a flat string).
  keywordsTransform = (raw) => (raw == null ? '' : String(raw)),
  tokenizer = 'unicode61 remove_diacritics 1',
  segment = true,
  stripHtml = false,
  segmenterVersion = 1,
}) {
  if (!name || !baseTable || !baseUidColumn || !ftsTable || !Array.isArray(textColumns) || textColumns.length === 0) {
    throw new Error(`[ftsMirror:${name}] invalid config`);
  }

  const hasKeywords = Boolean(keywordsColumn);
  const ftsColumnList = ['uid', ...textColumns, ...(hasKeywords ? ['keywords'] : [])];
  const fingerprint = computeFingerprint({ columns: ftsColumnList, tokenizer, segmenterVersion, hasKeywords });

  // Track readiness per mirror.
  let resolveReady;
  let rejectReady;
  const readyPromise = new Promise((res, rej) => { resolveReady = res; rejectReady = rej; });
  let ready = false;
  let rebuildProgress = { rebuilt: 0, base_count: null };

  // Lazily prepared statements — recreated after a DROP+CREATE in ensureSchema.
  let writeStmts = null;

  function prepareStmts() {
    if (writeStmts) return writeStmts;
    const ftsCols = ['uid', ...textColumns, ...(hasKeywords ? ['keywords'] : [])];
    const placeholders = ftsCols.map((c) => `@${c}`).join(', ');
    const colList = ftsCols.join(', ');
    const mapTable = `${ftsTable}_uid_map`;
    writeStmts = {
      ins: db.prepare(`INSERT INTO ${ftsTable} (${colList}) VALUES (${placeholders})`),
      delRow: db.prepare(`DELETE FROM ${ftsTable} WHERE rowid = ?`),
      kwUpdateRow: hasKeywords
        ? db.prepare(`UPDATE ${ftsTable} SET keywords = ? WHERE rowid = ?`)
        : null,
      mapSelect: db.prepare(`SELECT rowid FROM ${mapTable} WHERE uid = ?`),
      mapUpsert: db.prepare(
        `INSERT INTO ${mapTable} (uid, rowid) VALUES (?, ?)
         ON CONFLICT(uid) DO UPDATE SET rowid = excluded.rowid`,
      ),
      mapDelete: db.prepare(`DELETE FROM ${mapTable} WHERE uid = ?`),
    };
    return writeStmts;
  }

  function ensureSchema() {
    const meta = metaSelectStmt.get(name);
    const tableExists = db
      .prepare("SELECT 1 FROM sqlite_master WHERE type='table' AND name=?")
      .get(ftsTable);

    const fingerprintMatches = meta && meta.fingerprint === fingerprint;
    const needsCreate = !tableExists || !fingerprintMatches;
    const mapTable = `${ftsTable}_uid_map`;

    if (needsCreate) {
      console.log(`[ftsMirror:${name}] schema (re)create — fingerprint=${fingerprint.slice(0, 8)} ${tableExists ? '(was ' + (meta?.fingerprint?.slice(0, 8) || 'none') + ')' : '(new)'}`);
      const cols = [
        'uid UNINDEXED',
        ...textColumns,
        ...(hasKeywords ? ['keywords'] : []),
      ].join(', ');
      db.exec(`DROP TABLE IF EXISTS ${ftsTable};`);
      db.exec(`DROP TABLE IF EXISTS ${mapTable};`);
      db.exec(`CREATE VIRTUAL TABLE ${ftsTable} USING fts5(${cols}, tokenize='${tokenizer}');`);
      // Clear the meta row — fingerprint is only re-written after a successful rebuild.
      metaDeleteStmt.run(name);
      writeStmts = null;
    }

    // Sidecar uid → rowid map. uid is UNINDEXED in the FTS table, so
    // DELETE/UPDATE by uid would full-scan; routing through rowid (FTS5's
    // native indexed key) via this map makes those O(log N).
    db.exec(
      `CREATE TABLE IF NOT EXISTS ${mapTable} (
         uid   TEXT    NOT NULL PRIMARY KEY,
         rowid INTEGER NOT NULL
       ) WITHOUT ROWID;`,
    );

    // Migration: backfill the map from pre-existing FTS rows in a single txn.
    // Idempotent via INSERT OR REPLACE — a partial backfill converges on retry.
    const mapCount = db.prepare(`SELECT COUNT(*) AS n FROM ${mapTable}`).get().n;
    const ftsCount = db.prepare(`SELECT COUNT(*) AS n FROM ${ftsTable}`).get().n;
    if (mapCount < ftsCount) {
      console.log(`[ftsMirror:${name}] backfilling uid_map (${mapCount} → ${ftsCount})`);
      const t0 = Date.now();
      db.transaction(() => {
        db.exec(
          `INSERT OR REPLACE INTO ${mapTable} (uid, rowid)
           SELECT uid, rowid FROM ${ftsTable}`,
        );
      })();
      console.log(`[ftsMirror:${name}] backfilled uid_map in ${Date.now() - t0}ms`);
    }

    prepareStmts();
  }

  function preprocessText(value) {
    if (value == null) return '';
    let s = String(value);
    if (stripHtml) s = stripHtmlBasic(s);
    return s;
  }

  async function segmentValue(value) {
    const s = preprocessText(value);
    if (!s) return '';
    if (!segment) return s;
    try {
      return await segmentForFts(s);
    } catch (err) {
      console.warn(`[ftsMirror:${name}] segment failed for value, falling back to raw:`, err?.message);
      return s;
    }
  }

  /**
   * Segment a batch of rows. Each input row must carry the uid (named
   * via the base uid column or a `uid` field) and the source text columns
   * keyed by their FTS column name. Returns an array of segmented rows
   * ready for `writeOne` (no order changes, nulls preserved).
   */
  async function segmentRows(rows) {
    if (!Array.isArray(rows) || rows.length === 0) return [];
    return Promise.all(rows.map(async (row) => {
      if (!row) return null;
      const uid = row.uid ?? row[baseUidColumn];
      if (uid == null) return null;
      const out = { uid: String(uid) };
      for (const col of textColumns) {
        try {
          out[col] = await segmentValue(row[col]);
        } catch (err) {
          out[col] = preprocessText(row[col]) || '';
          out._segError = (out._segError || []).concat(`${col}:${err?.message || 'err'}`);
        }
      }
      if (hasKeywords) {
        // Caller may pass already-segmented keywords (from updateKeywords path)
        // or a raw base-row value that needs the transform + segmentation.
        // We segment unconditionally — segmenting an already-segmented string
        // is idempotent (kuromoji on Latin/space-delimited text passes through).
        const flat = keywordsTransform(row.keywords);
        out.keywords = await segmentValue(flat);
      }
      return out;
    }));
  }

  /**
   * Sync segmentation variant. Use when the caller is inside a sync
   * db.transaction and can't await. Falls back to raw text if kuromoji
   * isn't warmed up yet. Returns the same shape as one element of
   * segmentRows()'s output array.
   */
  function segmentRowSync(row) {
    if (!row) return null;
    const uid = row.uid ?? row[baseUidColumn];
    if (uid == null) return null;
    const out = { uid: String(uid) };
    for (const col of textColumns) {
      const pre = preprocessText(row[col]);
      out[col] = segment ? segmentForFtsSync(pre) : pre;
    }
    if (hasKeywords) {
      const flat = keywordsTransform(row.keywords);
      out.keywords = segment ? segmentForFtsSync(flat) : (flat || '');
    }
    return out;
  }

  /** Sync, inside caller's db.transaction. Idempotent (DELETE+INSERT). */
  function writeOne(segmented) {
    if (!segmented?.uid) return;
    const stmts = prepareStmts();
    const uid = String(segmented.uid);
    const existing = stmts.mapSelect.get(uid);
    if (existing) stmts.delRow.run(existing.rowid);
    const params = { uid };
    for (const col of textColumns) params[col] = segmented[col] ?? '';
    if (hasKeywords) params.keywords = segmented.keywords ?? '';
    const info = stmts.ins.run(params);
    stmts.mapUpsert.run(uid, info.lastInsertRowid);
  }

  /** Sync, inside caller's db.transaction. */
  function deleteOne(uid) {
    if (uid == null) return;
    const stmts = prepareStmts();
    const u = String(uid);
    const existing = stmts.mapSelect.get(u);
    if (existing) stmts.delRow.run(existing.rowid);
    stmts.mapDelete.run(u); // unconditional: clears orphan map rows if FTS was wiped externally
  }

  /** Update only the keywords column (for the LLM enricher path). No-op if mirror has no keywords column. */
  function updateKeywords(uid, segmentedKeywords) {
    if (!hasKeywords || uid == null) return;
    const stmts = prepareStmts();
    const existing = stmts.mapSelect.get(String(uid));
    if (!existing) return;
    stmts.kwUpdateRow.run(segmentedKeywords ?? '', existing.rowid);
  }

  /**
   * Atomic prune: DELETE FROM base WHERE <whereSql> RETURNING <uid>, then
   * loop FTS deletes inside one transaction. Use for TTL sweeps so the
   * caller can't forget the FTS half. `whereSql` may reference base columns
   * directly — the table is implied.
   */
  function pruneByCondition(whereSql, params = []) {
    const stmts = prepareStmts();
    const sql = `DELETE FROM ${baseTable} WHERE ${whereSql} RETURNING ${baseUidColumn} AS uid`;
    const tx = db.transaction(() => {
      const deleted = db.prepare(sql).all(...params);
      for (const row of deleted) {
        if (row?.uid == null) continue;
        const u = String(row.uid);
        const existing = stmts.mapSelect.get(u);
        if (existing) stmts.delRow.run(existing.rowid);
        stmts.mapDelete.run(u); // unconditional
      }
      return deleted.length;
    });
    return tx();
  }

  /**
   * Walk the base table in keyset-paginated batches, segment, and DELETE+INSERT
   * each row into FTS. Sequential write transactions release the SQLite write
   * lock between batches so concurrent collectors aren't starved.
   *
   * Trigger logic: rebuild runs when `force === true`, when there's no
   * `_fts_meta` row for this mirror (post-reshape state), or when the FTS
   * table has fewer rows than base (interrupted previous rebuild). The
   * fingerprint row is only written after the loop completes — interrupted
   * rebuilds re-trigger automatically on next boot.
   */
  async function rebuildFromBase({ force = false, batchSize = 200 } = {}) {
    prepareStmts();
    const stmts = writeStmts;
    const ftsCount = db.prepare(`SELECT COUNT(*) AS n FROM ${ftsTable}`).get().n;
    const baseCount = db.prepare(`SELECT COUNT(*) AS n FROM ${baseTable}`).get().n;
    const meta = metaSelectStmt.get(name);
    rebuildProgress = { rebuilt: 0, base_count: baseCount };

    const needsRebuild = force || !meta || meta.fingerprint !== fingerprint || ftsCount < baseCount;
    if (!needsRebuild) {
      return { rebuilt: 0, ftsCount, baseCount, skipped: 'fingerprint_match_and_count_ok' };
    }

    const t0 = Date.now();

    // Resumable rebuilds: only wipe FTS when the schema fingerprint changed
    // (column set / tokenizer / segmenter version differs — the existing
    // rows are wrong shape and must be rebuilt). When fingerprint matches,
    // we're just filling in a `ftsCount < baseCount` gap; committed rows
    // from a previous (killed) rebuild are reusable as-is, so we resume
    // from MAX(uid). The per-batch loop below does DELETE+INSERT per uid,
    // so any row that gets revisited is safely overwritten — and live
    // writes via upsertItemSync stay in sync atomically.
    const mapTable = `${ftsTable}_uid_map`;
    const fingerprintChanged = !meta || meta.fingerprint !== fingerprint;
    let cursor = '';
    if (fingerprintChanged) {
      db.exec(`DELETE FROM ${ftsTable}`);
      db.exec(`DELETE FROM ${mapTable}`);
    } else if (ftsCount > 0) {
      // Cursor from the uid_map (uid is PK there) — querying MAX(uid) on the
      // FTS table itself would full-scan because uid is UNINDEXED.
      const row = db.prepare(`SELECT MAX(uid) AS max_uid FROM ${mapTable}`).get();
      cursor = row?.max_uid ?? '';
      console.log(`[ftsMirror:${name}] resuming rebuild from cursor=${String(cursor).slice(0, 60)} (ftsCount=${ftsCount}, baseCount=${baseCount})`);
    }

    // Keyset pagination by uid. Released-lock between batches — collectors
    // writing to base aren't blocked for long.
    const uidExpr = baseUidColumn;
    const baseSelectColumns = [...textColumns, ...(hasKeywords ? ['keywords'] : [])];
    const selectSql =
      `SELECT ${uidExpr} AS uid, ${baseSelectColumns.map((c) => `"${c}"`).join(', ')} ` +
      `FROM ${baseTable} ` +
      `WHERE ${uidExpr} > @cursor ` +
      `ORDER BY ${uidExpr} ASC LIMIT @limit`;
    const pageStmt = db.prepare(selectSql);

    let rebuilt = 0;
    while (true) {
      const rows = pageStmt.all({ cursor, limit: batchSize });
      if (rows.length === 0) break;

      const segs = await segmentRows(rows);

      const tx = db.transaction((batch) => {
        let n = 0;
        for (const s of batch) {
          if (!s) continue;
          const existing = stmts.mapSelect.get(s.uid);
          if (existing) stmts.delRow.run(existing.rowid);
          const params = { uid: s.uid };
          for (const col of textColumns) params[col] = s[col] ?? '';
          if (hasKeywords) params.keywords = s.keywords ?? '';
          const info = stmts.ins.run(params);
          stmts.mapUpsert.run(s.uid, info.lastInsertRowid);
          n += 1;
        }
        return n;
      });
      rebuilt += tx(segs);
      rebuildProgress = { rebuilt, base_count: baseCount };
      cursor = rows[rows.length - 1].uid;
      if (rows.length < batchSize) break;
    }

    metaUpsertStmt.run({
      name,
      fingerprint,
      columns: JSON.stringify(ftsColumnList),
      tokenizer,
      version: segmenterVersion,
      rebuilt_at: new Date().toISOString(),
    });

    return { rebuilt, ftsCount, baseCount, duration_ms: Date.now() - t0 };
  }

  /**
   * Pre-segment q the same way rows were segmented at write time, then run
   * a parameterized MATCH joined to base. Caller composes additional WHERE
   * clauses, ORDER BY, and LIMIT — keeps cursor encoding & UI logic in routes.
   *
   * Returns raw rows (base columns + `_excerpt`). No JSON parsing.
   */
  async function search({
    q,
    where = [],
    params = {},
    orderBy = null,
    limit = 50,
    snippetColumn = -1,   // -1 = all FTS text columns
    extraJoinSql = '',
    selectColumns = `${baseTable}.*`,
  } = {}) {
    if (!q || !q.trim()) return { rows: [] };
    const segQ = await segmentForFts(q.trim());
    const whereSql = where.length > 0 ? `AND ${where.join(' AND ')}` : '';
    const orderSql = orderBy ? `ORDER BY ${orderBy}` : '';
    const sql =
      `SELECT ${selectColumns}, ` +
      `       snippet(${ftsTable}, ${snippetColumn}, '<mark>', '</mark>', '…', 12) AS _excerpt ` +
      `  FROM ${ftsTable} ` +
      `  JOIN ${baseTable} ON ${baseTable}.${baseUidColumn} = ${ftsTable}.uid ` +
      `  ${extraJoinSql} ` +
      ` WHERE ${ftsTable} MATCH @q ${whereSql} ` +
      `  ${orderSql} ` +
      ` LIMIT @limit`;
    const rows = db.prepare(sql).all({ ...params, q: segQ, limit });
    return { rows };
  }

  function isReady() { return ready; }
  function getProgress() { return { ...rebuildProgress }; }

  function markReady(meta = {}) {
    if (ready) return;
    ready = true;
    resolveReady(meta);
    try {
      broadcastEvent({
        type: 'fts_ready',
        table: name,
        rows: meta.rebuilt ?? rebuildProgress.rebuilt,
        base_count: rebuildProgress.base_count,
        duration_ms: meta.duration_ms ?? null,
        ts: new Date().toISOString(),
      });
    } catch (err) {
      console.warn(`[ftsMirror:${name}] broadcast fts_ready failed:`, err?.message);
    }
  }

  function markFailed(err) {
    if (ready) return;
    rejectReady(err);
  }

  // Auto-ensure schema at definition time. Store modules can call rebuildFromBase
  // themselves or rely on the registry's boot orchestrator.
  ensureSchema();

  return {
    name,
    baseTable,
    baseUidColumn,
    ftsTable,
    ensureSchema,
    segmentRows,
    segmentRowSync,
    writeOne,
    deleteOne,
    updateKeywords,
    pruneByCondition,
    rebuildFromBase,
    search,
    isReady,
    getProgress,
    readyPromise,
    markReady,
    markFailed,
    _internal: { fingerprint, hasKeywords },
  };
}
