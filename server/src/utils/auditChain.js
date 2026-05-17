/**
 * Tamper-evident audit chain.
 *
 * Every audit row is linked to the previous row in the same tenant's
 * chain: `prev_hash` is the tip's `row_hash`, and `row_hash` is the
 * SHA-256 of the canonical tuple of this row's stored fields (including
 * prev_hash and chain_seq). Editing or deleting any historical row
 * changes its row_hash, which breaks the prev_hash linkage of every later
 * row — so tampering is detectable by re-walking the chain, with no
 * external timestamping service required.
 *
 * Ordering: the chain is ordered by `chain_seq`, a strictly increasing
 * per-tenant counter assigned in the same transaction as the insert
 * (tip.chain_seq + 1). We deliberately do NOT order by ts or id — `id` is
 * a random UUID and `ts` can collide at millisecond resolution, either of
 * which would let verification re-order rows and flag a false tamper.
 *
 * Genesis: rows written before the chain migration have NULL hashes. The
 * chain begins at the first row appended through this module; its
 * prev_hash is the fixed sentinel GENESIS and its chain_seq is 1.
 *
 * Concurrency: read-tip + insert run inside one better-sqlite3
 * transaction. SQLite is single-writer, so transactions serialise and two
 * near-simultaneous requests cannot fork the chain.
 */

// TENANCY EXCEPTION (intentional): the audit writer is a system component
// that appends hash-chained rows for ANY tenant id, including the synthetic
// 'platform' scope (break-glass, system events). It manages tenant_id itself
// and must not be constrained to a single active tenant — uses raw `db`,
// not tenantDb(). Tenant-scoped audit *reads* go through tenantDb (routes/audit.js).
import db from './database.js';
import { canonicalJson, sha256Hex } from './canonical.js';

const GENESIS = 'GENESIS';

// Statements are prepared lazily, not at module load. The `chain_seq`
// column is added by runTenancyMigration() (an ALTER), but ESM import
// hoisting evaluates this module *before* index.js calls that migration —
// preparing at import time would compile against a column that doesn't
// exist yet and crash boot. Preparing on first use defers compilation to
// after the migration has run.
let _stmtTip;
let _stmtInsert;
function stmtTip() {
  _stmtTip ||= db.prepare(`
    SELECT chain_seq, row_hash FROM audit_events
     WHERE tenant_id = ? AND row_hash IS NOT NULL
     ORDER BY chain_seq DESC
     LIMIT 1
  `);
  return _stmtTip;
}
function stmtInsert() {
  _stmtInsert ||= db.prepare(`
    INSERT INTO audit_events
      (id, tenant_id, user_id, action, target, payload_json, ts, ip, ua,
       prev_hash, row_hash, chain_seq)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
  `);
  return _stmtInsert;
}

/**
 * The exact field set the row_hash commits to. Must stay stable and must
 * only reference columns we persist verbatim, so verification can
 * recompute it from the stored row alone.
 */
function rowHashOf({
  id, tenantId, userId, action, target, payloadJson, ts, ip, ua,
  prevHash, chainSeq,
}) {
  return sha256Hex(canonicalJson({
    id,
    tenant_id: tenantId,
    user_id: userId ?? null,
    action,
    target: target ?? null,
    payload_json: payloadJson ?? null,
    ts,
    ip: ip ?? null,
    ua: ua ?? null,
    prev_hash: prevHash,
    chain_seq: chainSeq,
  }));
}

/**
 * Append one chained audit row. `ts` is computed here (not in SQL) so the
 * value the hash commits to is exactly the value stored. Returns
 * { row_hash, chain_seq }. Throws only on a genuine DB failure — the
 * audit middleware swallows that so a logging failure never breaks a
 * request.
 */
export const appendAuditEvent = db.transaction((evt) => {
  const tip = stmtTip().get(evt.tenantId);
  const prevHash = tip?.row_hash || GENESIS;
  const chainSeq = (tip?.chain_seq || 0) + 1;
  const ts = evt.ts || new Date().toISOString();
  const rowHash = rowHashOf({ ...evt, ts, prevHash, chainSeq });
  stmtInsert().run(
    evt.id,
    evt.tenantId,
    evt.userId ?? null,
    evt.action,
    evt.target ?? null,
    evt.payloadJson ?? null,
    ts,
    evt.ip ?? null,
    evt.ua ?? null,
    prevHash,
    rowHash,
    chainSeq,
  );
  return { row_hash: rowHash, chain_seq: chainSeq };
});

/**
 * Re-walk a tenant's chained audit rows oldest→newest, recomputing each
 * row_hash and checking that each row's prev_hash equals the previous
 * row's row_hash. Returns where the chain breaks, if anywhere.
 */
export function verifyAuditChain(tenantId, { limit = 100000 } = {}) {
  const rows = db.prepare(`
    SELECT id, tenant_id, user_id, action, target, payload_json, ts, ip, ua,
           prev_hash, row_hash, chain_seq
      FROM audit_events
     WHERE tenant_id = ? AND row_hash IS NOT NULL
     ORDER BY chain_seq ASC
     LIMIT ?
  `).all(tenantId, Math.max(1, Number(limit) || 100000));

  let expectedPrev = GENESIS;
  let expectedSeq = 1;
  for (let i = 0; i < rows.length; i++) {
    const r = rows[i];
    if (r.chain_seq !== expectedSeq) {
      return {
        ok: false, count: rows.length, brokenAt: i,
        brokenId: r.id, reason: `chain_seq gap (expected ${expectedSeq}, got ${r.chain_seq})`,
      };
    }
    if (r.prev_hash !== expectedPrev) {
      return {
        ok: false, count: rows.length, brokenAt: i,
        brokenId: r.id, reason: 'prev_hash linkage mismatch (row inserted/removed)',
      };
    }
    const recomputed = rowHashOf({
      id: r.id,
      tenantId: r.tenant_id,
      userId: r.user_id,
      action: r.action,
      target: r.target,
      payloadJson: r.payload_json,
      ts: r.ts,
      ip: r.ip,
      ua: r.ua,
      prevHash: r.prev_hash,
      chainSeq: r.chain_seq,
    });
    if (recomputed !== r.row_hash) {
      return {
        ok: false, count: rows.length, brokenAt: i,
        brokenId: r.id, reason: 'row_hash mismatch (row altered)',
      };
    }
    expectedPrev = r.row_hash;
    expectedSeq += 1;
  }
  return {
    ok: true,
    count: rows.length,
    tip: expectedPrev === GENESIS ? null : expectedPrev,
  };
}
