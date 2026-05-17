/**
 * Audit log read + integrity endpoints. Behind /api so auth + tenant
 * middleware always populate req.tenant first; every query is scoped to
 * the active tenant's chain.
 *
 * Routes:
 *   GET /api/audit          recent audit events for the active tenant
 *   GET /api/audit/verify   re-walk the tenant's tamper-evident hash chain
 *
 * The trail is append-only and the chain is self-verifying, so there are
 * deliberately no mutation routes here.
 */

import express from 'express';
import { tenantDb } from '../utils/tenancy.js';
import { verifyAuditChain } from '../utils/auditChain.js';

const router = express.Router();

router.get('/', (req, res) => {
  const limit = Math.min(500, Math.max(1, Number(req.query.limit) || 100));
  const offset = Math.max(0, Number(req.query.offset) || 0);
  // Prepared per-request through tenantDb: well after runTenancyMigration()
  // (boot) so prev_hash/row_hash exist, and the tenant id is injected so
  // the chain can only ever be read for the active tenant.
  const rows = tenantDb(req.tenant.id).prepare(`
    SELECT id, user_id, action, target, ts, ip, ua,
           prev_hash, row_hash, chain_seq
      FROM audit_events
     WHERE tenant_id = @tenant_id
     ORDER BY chain_seq DESC
     LIMIT @limit OFFSET @offset
  `).all({ limit, offset });
  res.json({ data: rows, page: { limit, offset, count: rows.length } });
});

router.get('/verify', (req, res) => {
  const result = verifyAuditChain(req.tenant.id);
  // 200 with ok:false (not an error status) — a tampered chain is a valid,
  // important answer the caller must be able to read and act on.
  res.json(result);
});

export default router;
