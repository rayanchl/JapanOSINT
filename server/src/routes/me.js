/**
 * GET /api/me — identity + active-tenant descriptor for the client.
 *
 * Drives the iOS onboarding/session UI: after Supabase sign-in the client
 * calls this to learn which tenant the server resolved it into (the personal
 * tenant is auto-provisioned on first authenticated request by the tenant
 * middleware) and what other tenants it can switch to via X-Tenant-Id.
 *
 * Mounted under /api, so requireSupabaseAuth + resolveTenant have already
 * run and populated req.user / req.tenant / req.role before we get here.
 */

import express from 'express';
import db from '../utils/database.js';

const router = express.Router();

router.get('/', (req, res) => {
  if (!req.user || !req.tenant) {
    // Middleware should have populated these; if not, the token was rejected
    // upstream and we never reach here. Defensive 401 just in case.
    return res.status(401).json({ error: 'Not authenticated' });
  }

  const memberships = db.prepare(`
    SELECT t.id, t.slug, t.name, t.plan, m.role
      FROM memberships m
      JOIN tenants t ON t.id = m.tenant_id
     WHERE m.user_id = ?
     ORDER BY m.created_at ASC
  `).all(req.user.id);

  res.json({
    user: {
      id: req.user.id,
      email: req.user.email,
      display_name: req.user.display_name ?? null,
    },
    tenant: {
      id: req.tenant.id,
      slug: req.tenant.slug,
      name: req.tenant.name,
      plan: req.tenant.plan,
      role: req.role ?? null,
    },
    memberships,
  });
});

export default router;
