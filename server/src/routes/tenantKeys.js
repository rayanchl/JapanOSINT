/**
 * Per-tenant BYOK key management.
 *
 * Backs the iOS "API keys" tab. Each workspace stores its own keys in the
 * AES-256-GCM-encrypted `tenant_secrets` store (utils/credentials.js); a
 * workspace with no key of its own transparently falls back to the operator's
 * platform/.env key (fallback_to_platform defaults to 1), so the app keeps
 * working out of the box and a user key silently overrides.
 *
 * Mounted under /api, so requireSupabaseAuth + resolveTenant have already set
 * req.user / req.tenant / req.role. Edit rights are governed by the
 * owner-chosen policy (middleware/keyAccess.js).
 *
 * Route order matters: '/policy' is declared before '/:name' so the literal
 * path isn't captured by the param route.
 */

import { Router } from 'express';
import { tenantDb } from '../utils/tenancy.js';
import { getAllKnownVarNames } from '../utils/apiCredentials.js';
import {
  listTenantSecrets,
  setTenantSecret,
  deleteTenantSecret,
  getTenantSecret,
} from '../utils/credentials.js';
import { canManageTenantKeys, getKeyWritePolicy } from '../middleware/keyAccess.js';

const router = Router();

const POLICIES = new Set(['owner_only', 'selected_member', 'all_members']);

function knownVar(name) {
  return getAllKnownVarNames().find((v) => v.name === name) || null;
}

// GET /api/tenant-keys — every known var with this workspace's status.
// Never returns values. `source`: 'tenant' (own BYOK key) | 'platform'
// (operator default in process.env) | null (gated, no key anywhere).
router.get('/', (req, res) => {
  try {
    const tenantId = req.tenant.id;
    const byok = new Set(listTenantSecrets(tenantId).map((r) => r.key_name));
    const items = getAllKnownVarNames().map(({ name, role }) => {
      let source = null;
      if (byok.has(name)) {
        source = 'tenant';
      } else {
        const env = process.env[name];
        if (typeof env === 'string' && env.trim().length > 0) source = 'platform';
      }
      return { name, role, byok: byok.has(name), set: source !== null, source };
    });
    const { policy, memberId } = getKeyWritePolicy(tenantId);
    res.json({
      items,
      canManage: canManageTenantKeys(req),
      policy,
      policyMemberId: memberId,
    });
  } catch (err) {
    console.error('[tenantKeys] list failed:', err.message);
    res.status(500).json({ error: 'Failed to list keys' });
  }
});

// GET /api/tenant-keys/policy — current policy + the member roster the owner
// picks a delegate from. Readable by anyone in the workspace.
router.get('/policy', (req, res) => {
  try {
    const tenantId = req.tenant.id;
    const { policy, memberId } = getKeyWritePolicy(tenantId);
    const members = tenantDb(tenantId).prepare(`
      SELECT u.id, u.email, m.role
        FROM memberships m
        JOIN users u ON u.id = m.user_id
       WHERE m.tenant_id = @tenant_id
       ORDER BY m.created_at ASC
    `).all();
    res.json({ policy, memberId, members });
  } catch (err) {
    console.error('[tenantKeys] policy read failed:', err.message);
    res.status(500).json({ error: 'Failed to read policy' });
  }
});

// PUT /api/tenant-keys/policy — owner-only. body { policy, memberId? }.
router.put('/policy', (req, res) => {
  if (req.role !== 'owner') {
    return res.status(403).json({ error: 'Only the workspace owner can change this policy' });
  }
  const policy = req.body?.policy;
  if (!POLICIES.has(policy)) {
    return res.status(400).json({ error: 'Invalid policy' });
  }
  let memberId = null;
  if (policy === 'selected_member') {
    memberId = typeof req.body?.memberId === 'string' ? req.body.memberId : null;
    const ok = memberId && tenantDb(req.tenant.id).prepare(
      `SELECT 1 FROM memberships WHERE tenant_id = @tenant_id AND user_id = @user_id`
    ).get({ user_id: memberId });
    if (!ok) {
      return res.status(400).json({ error: 'memberId must be a member of this workspace' });
    }
  }
  try {
    // `tenants` keyed by id == the active tenant id; the @tenant_id binding
    // is injected by tenantDb so this can only ever touch the caller's row.
    tenantDb(req.tenant.id).prepare(
      `UPDATE tenants SET key_write_policy = @policy, key_write_member_id = @member_id
        WHERE id = @tenant_id`
    ).run({ policy, member_id: memberId });
    res.json({ policy, memberId });
  } catch (err) {
    console.error('[tenantKeys] policy write failed:', err.message);
    res.status(500).json({ error: 'Failed to write policy' });
  }
});

// GET /api/tenant-keys/:name — reveal THIS workspace's own key only (never
// the platform value). Face-ID gated client-side. 403 if the caller can't
// manage keys; 404 for unknown vars (can't probe arbitrary env).
router.get('/:name', (req, res) => {
  const { name } = req.params;
  if (!knownVar(name)) return res.status(404).json({ error: 'Unknown key' });
  if (!canManageTenantKeys(req)) {
    return res.status(403).json({ error: 'Not permitted to view this workspace\'s keys' });
  }
  res.json({ name, value: getTenantSecret(req.tenant.id, name) });
});

// PUT /api/tenant-keys/:name — body { value }. Empty string clears the
// workspace key (collectors fall back to the platform key automatically).
router.put('/:name', (req, res) => {
  const { name } = req.params;
  if (!knownVar(name)) return res.status(400).json({ error: 'Unknown key' });
  if (!canManageTenantKeys(req)) {
    return res.status(403).json({ error: 'Not permitted to edit this workspace\'s keys' });
  }
  const value = typeof req.body?.value === 'string' ? req.body.value : '';
  try {
    if (value.length === 0) {
      deleteTenantSecret(req.tenant.id, name);
    } else {
      setTenantSecret(req.tenant.id, name, value, { createdBy: req.user.id });
    }
    const byok = value.length > 0;
    const env = process.env[name];
    const platform = typeof env === 'string' && env.trim().length > 0;
    res.json({
      name,
      byok,
      set: byok || platform,
      source: byok ? 'tenant' : platform ? 'platform' : null,
    });
  } catch (err) {
    console.error('[tenantKeys] write failed:', err.message);
    res.status(500).json({ error: 'Failed to write key' });
  }
});

export default router;
