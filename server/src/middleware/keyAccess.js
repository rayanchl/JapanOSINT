/**
 * Authorization helpers for API-key surfaces.
 *
 * Two distinct gates:
 *   - requirePlatformAdmin  → the global process.env overlay (apiKeysStore).
 *     Operator-only: server-wide default keys. owner|admin of the active
 *     tenant may touch it; everyone else gets 403.
 *   - canManageTenantKeys   → the per-workspace BYOK store (tenant_secrets).
 *     owner|admin can always edit; otherwise the owner-chosen
 *     `tenants.key_write_policy` decides.
 *
 * Both run after `resolveTenant` (middleware/tenant.js) has set req.user /
 * req.tenant / req.role. `req.tenant` only carries a thin column subset, so
 * the policy is read straight from the DB by tenant id.
 */

import { tenantDb } from '../utils/tenancy.js';

const ADMIN_ROLES = new Set(['owner', 'admin']);

/** Express guard: 403 unless the active membership is owner or admin. */
export function requirePlatformAdmin(req, res, next) {
  if (!req.role || !ADMIN_ROLES.has(req.role)) {
    return res.status(403).json({ error: 'Admin role required' });
  }
  return next();
}

// Platform-operator allowlist. Every signed-in user gets an auto-provisioned
// tenant where they are `owner`, so tenant role is NOT a platform-operator
// signal. Operator-only surfaces (server restart, raw DB explorer) must be
// gated on an explicit env allowlist instead. Fail closed: if neither list
// is configured, nobody is an operator.
const OPERATOR_EMAILS = new Set(
  (process.env.PLATFORM_OPERATOR_EMAILS || '')
    .split(',').map((s) => s.trim().toLowerCase()).filter(Boolean)
);
const OPERATOR_IDS = new Set(
  (process.env.PLATFORM_OPERATOR_IDS || '')
    .split(',').map((s) => s.trim()).filter(Boolean)
);

/**
 * Express guard for platform-operator-only routes (/api/admin, /api/db).
 * Checks the authenticated Supabase identity against an explicit env
 * allowlist — independent of tenant membership/role. 403 otherwise.
 */
export function requirePlatformOperator(req, res, next) {
  const u = req.supabaseUser;
  if (!u) return res.status(401).json({ error: 'Auth required' });
  if (OPERATOR_EMAILS.size === 0 && OPERATOR_IDS.size === 0) {
    return res
      .status(403)
      .json({ error: 'Platform operator access not configured' });
  }
  const email = (u.email || '').toLowerCase();
  if ((email && OPERATOR_EMAILS.has(email)) || OPERATOR_IDS.has(u.id)) {
    return next();
  }
  return res.status(403).json({ error: 'Platform operator role required' });
}

/**
 * The owner-configured edit policy for a tenant's own API keys.
 * Returns { policy, memberId }. Defaults to owner_only if the row/columns
 * are somehow absent (defensive — the migration adds them at boot).
 */
export function getKeyWritePolicy(tenantId) {
  const row = tenantDb(tenantId)
    .prepare(
      `SELECT key_write_policy AS policy, key_write_member_id AS memberId
         FROM tenants WHERE id = @tenant_id`
    )
    .get();
  return {
    policy: row?.policy || 'owner_only',
    memberId: row?.memberId || null,
  };
}

/**
 * Whether the current request may set/clear the active tenant's BYOK keys.
 * owner|admin always may. Otherwise the owner-chosen policy applies:
 *   all_members      → any signed-in member
 *   selected_member  → only the pinned delegate (key_write_member_id)
 *   owner_only       → nobody else
 */
export function canManageTenantKeys(req) {
  if (req.role && ADMIN_ROLES.has(req.role)) return true;
  const { policy, memberId } = getKeyWritePolicy(req.tenant.id);
  if (policy === 'all_members') return true;
  if (policy === 'selected_member') return req.user?.id === memberId;
  return false; // owner_only
}
