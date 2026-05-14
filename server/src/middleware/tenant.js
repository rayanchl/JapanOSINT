/**
 * Tenant resolution. Runs after `requireSupabaseAuth` has populated
 * `req.supabaseUser`.
 *
 * Responsibilities:
 *   1. First-seen hook: if the Supabase user id is unknown, create the
 *      local `users` row, a personal `tenants` row (slug = email-prefix),
 *      and an `owner` membership. Idempotent — concurrent requests are
 *      safe because we use UNIQUE constraints + IGNORE on the insert.
 *   2. Tenant pick: for users with multiple memberships, prefer the
 *      X-Tenant-Id header; fall back to the first membership ordered by
 *      created_at. Single-membership users always resolve unambiguously.
 *   3. Attach req.user (local users row) + req.tenant (tenants row) +
 *      req.role (string).
 *
 * Pure DB work — no network calls.
 */

import { randomUUID } from 'crypto';
import db from '../utils/database.js';
import { MULTI_TENANT_ENABLED } from './auth.js';

export async function resolveTenant(req, res, next) {
  if (!MULTI_TENANT_ENABLED) return next();
  if (!req.supabaseUser) {
    return res.status(401).json({ error: 'Auth required' });
  }

  try {
    const user = ensureUser(req.supabaseUser);
    const memberships = listMemberships(user.id);

    // First sign-in: no memberships yet. Provision a personal tenant.
    if (memberships.length === 0) {
      const tenant = createPersonalTenant(user);
      addMembership(user.id, tenant.id, 'owner');
      memberships.push({ tenant_id: tenant.id, role: 'owner', ...tenant });
    }

    // Header-based tenant selection for users in multiple tenants. Ignored
    // if the header points at a tenant the user isn't a member of.
    const requested = req.headers['x-tenant-id'];
    let picked = null;
    if (typeof requested === 'string' && requested) {
      picked = memberships.find((m) => m.tenant_id === requested) || null;
    }
    if (!picked) picked = memberships[0];

    const tenant = db.prepare(
      `SELECT id, slug, name, plan, require_sso FROM tenants WHERE id = ?`
    ).get(picked.tenant_id);
    if (!tenant) {
      return res.status(500).json({ error: 'Tenant row vanished' });
    }

    req.user = user;
    req.tenant = tenant;
    req.role = picked.role;
    return next();
  } catch (err) {
    console.error('[tenant] resolution failed:', err.stack || err);
    return res.status(500).json({ error: 'Tenant resolution failed' });
  }
}

// ── DB helpers ──────────────────────────────────────────────────────────────

function ensureUser({ id: supabaseId, email }) {
  const existing = db.prepare(
    `SELECT id, supabase_user_id, email, display_name FROM users WHERE supabase_user_id = ?`
  ).get(supabaseId);
  if (existing) return existing;

  const localId = randomUUID();
  const safeEmail = email || `${supabaseId}@unknown.local`;
  db.prepare(`
    INSERT INTO users (id, supabase_user_id, email)
    VALUES (?, ?, ?)
    ON CONFLICT(supabase_user_id) DO NOTHING
  `).run(localId, supabaseId, safeEmail);

  return db.prepare(
    `SELECT id, supabase_user_id, email, display_name FROM users WHERE supabase_user_id = ?`
  ).get(supabaseId);
}

function listMemberships(userId) {
  return db.prepare(
    `SELECT user_id, tenant_id, role, created_at
       FROM memberships
      WHERE user_id = ?
      ORDER BY created_at ASC`
  ).all(userId);
}

function createPersonalTenant(user) {
  const id = randomUUID();
  const slug = slugify(user.email);
  const name = `${user.email}'s workspace`;
  db.prepare(`
    INSERT INTO tenants (id, slug, name, plan)
    VALUES (?, ?, ?, 'free')
  `).run(id, slug, name);
  return { id, slug, name, plan: 'free' };
}

function addMembership(userId, tenantId, role) {
  db.prepare(`
    INSERT OR IGNORE INTO memberships (user_id, tenant_id, role)
    VALUES (?, ?, ?)
  `).run(userId, tenantId, role);
}

/**
 * Build a tenant slug from an email. Collisions are allowed at the DB level
 * (UNIQUE on tenants.slug); on collision we append a 6-char random tail.
 */
function slugify(email) {
  const base = String(email || 'user')
    .toLowerCase()
    .replace(/@.*/, '')
    .replace(/[^a-z0-9]+/g, '-')
    .replace(/^-+|-+$/g, '')
    .slice(0, 40) || 'user';

  // Check for collision. If taken, append random tail.
  const taken = db.prepare(`SELECT 1 FROM tenants WHERE slug = ?`).get(base);
  if (!taken) return base;
  return `${base}-${randomUUID().slice(0, 6)}`;
}
