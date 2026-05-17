import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { randomUUID } from 'crypto';
import { canonicalJson, sha256Hex, hashObject } from '../src/utils/canonical.js';
import { runTenancyMigration } from '../src/utils/tenancyMigration.js';
import { appendAuditEvent, verifyAuditChain } from '../src/utils/auditChain.js';
import db from '../src/utils/database.js';

// Hits the real SQLite DB (repo test convention). Scope every row to a
// throwaway tenant id and delete them in `after` so we never touch real
// audit data.
const TENANT = `__test_audit_${randomUUID()}`;

before(() => {
  // Ensure prev_hash / row_hash / chain_seq columns exist.
  runTenancyMigration();
});

after(() => {
  db.prepare('DELETE FROM audit_events WHERE tenant_id = ?').run(TENANT);
});

test('canonicalJson is key-order stable and hashable', () => {
  const a = canonicalJson({ b: 1, a: { y: 2, x: 3 }, c: [3, 1] });
  const b = canonicalJson({ c: [3, 1], a: { x: 3, y: 2 }, b: 1 });
  assert.equal(a, b, 'key order must not affect canonical form');
  assert.equal(a, '{"a":{"x":3,"y":2},"b":1,"c":[3,1]}');
  // array order is significant
  assert.notEqual(canonicalJson([1, 2]), canonicalJson([2, 1]));
  // non-finite + undefined normalise to null
  assert.equal(canonicalJson({ n: NaN, u: undefined }), '{"n":null,"u":null}');
  const h = sha256Hex('abc');
  assert.match(h, /^[0-9a-f]{64}$/);
  assert.equal(h, sha256Hex('abc'), 'sha256 is deterministic');
  assert.equal(hashObject({ a: 1 }), sha256Hex('{"a":1}'));
});

test('appendAuditEvent builds a verifiable linked chain', () => {
  for (let i = 0; i < 5; i++) {
    appendAuditEvent({
      id: randomUUID(),
      tenantId: TENANT,
      userId: 'u1',
      action: `POST /api/thing/${i}`,
      target: String(i),
      payloadJson: JSON.stringify({ i, status: 200 }),
      ip: '127.0.0.1',
      ua: 'test',
    });
  }
  const res = verifyAuditChain(TENANT);
  assert.equal(res.ok, true, JSON.stringify(res));
  assert.equal(res.count, 5);
  assert.ok(res.tip, 'tip hash present');

  // chain_seq is dense 1..5 and prev_hash links each row to its predecessor
  const rows = db.prepare(
    `SELECT prev_hash, row_hash, chain_seq FROM audit_events
       WHERE tenant_id = ? ORDER BY chain_seq ASC`,
  ).all(TENANT);
  assert.deepEqual(rows.map((r) => r.chain_seq), [1, 2, 3, 4, 5]);
  assert.equal(rows[0].prev_hash, 'GENESIS');
  for (let i = 1; i < rows.length; i++) {
    assert.equal(rows[i].prev_hash, rows[i - 1].row_hash, `link ${i}`);
  }
});

test('tampering with a row is detected', () => {
  const before = verifyAuditChain(TENANT);
  assert.equal(before.ok, true);

  // Mutate a historical row's payload directly in the DB (simulating an
  // attacker editing the log) without recomputing its hash.
  const victim = db.prepare(
    `SELECT id FROM audit_events WHERE tenant_id = ? AND chain_seq = 3`,
  ).get(TENANT);
  db.prepare(
    `UPDATE audit_events SET payload_json = ? WHERE id = ?`,
  ).run(JSON.stringify({ tampered: true }), victim.id);

  const after = verifyAuditChain(TENANT);
  assert.equal(after.ok, false, 'tamper must be detected');
  assert.equal(after.brokenId, victim.id);
  assert.match(after.reason, /row_hash mismatch/);
});

test('deleting a row breaks the chain', () => {
  // Re-seed a clean chain under a fresh tenant for an independent check.
  const t2 = `__test_audit_${randomUUID()}`;
  try {
    for (let i = 0; i < 4; i++) {
      appendAuditEvent({
        id: randomUUID(), tenantId: t2, userId: null,
        action: 'GET /x', target: null,
        payloadJson: '{}', ip: null, ua: null,
      });
    }
    assert.equal(verifyAuditChain(t2).ok, true);
    db.prepare(
      `DELETE FROM audit_events WHERE tenant_id = ? AND chain_seq = 2`,
    ).run(t2);
    const res = verifyAuditChain(t2);
    assert.equal(res.ok, false);
    assert.match(res.reason, /chain_seq gap|linkage/);
  } finally {
    db.prepare('DELETE FROM audit_events WHERE tenant_id = ?').run(t2);
  }
});
