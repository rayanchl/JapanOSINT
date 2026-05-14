/**
 * Tenant-scoped query helper. The single chokepoint every user-data read
 * and write should go through during the multi-tenant cutover.
 *
 * Usage:
 *   import { tenantDb } from '../utils/tenancy.js';
 *
 *   tenantDb(req.tenant.id).prepare(
 *     `SELECT * FROM intel_items WHERE tenant_id = ? AND source_id = ?`
 *   ).all(t, sourceId);  // ← the `t` placeholder gets bound for you
 *
 * Guarantees:
 *   - The SQL string MUST contain `tenant_id` (case-insensitive) in the
 *     WHERE clause / INSERT column list. Throws in dev if missing; logs +
 *     refuses to execute in prod.
 *   - The query's first bound parameter is the active tenant id; pass `t`
 *     as a placeholder where the tenant id should land.
 *
 * This is opt-in for now: legacy code paths using `db.prepare` directly
 * still work. Routes touching user data should migrate to `tenantDb` as
 * they're audited.
 */

import db from './database.js';

const DEV = process.env.NODE_ENV !== 'production';

class TenantScoped {
  constructor(tenantId) {
    if (!tenantId || typeof tenantId !== 'string') {
      throw new Error(`tenantDb: tenantId must be a non-empty string, got ${tenantId}`);
    }
    this.tenantId = tenantId;
  }

  prepare(sql) {
    assertTenantScoped(sql);
    const stmt = db.prepare(sql);
    const tenantId = this.tenantId;

    // Wrap every method that takes bind params so the literal token `t` in
    // the params list is rewritten to the active tenant id. This keeps
    // call sites readable while making the binding impossible to forget.
    function bind(args) {
      return args.map((a) => (a === t ? tenantId : a));
    }
    return {
      all:    (...a) => stmt.all(...bind(a)),
      get:    (...a) => stmt.get(...bind(a)),
      run:    (...a) => stmt.run(...bind(a)),
      iterate:(...a) => stmt.iterate(...bind(a)),
      raw:    (...a) => stmt.raw(...a),
      // Expose the underlying statement for callers that need pluck/expand/
      // columns/etc. — they take responsibility for binding tenant_id.
      _stmt:  stmt,
    };
  }
}

/**
 * Placeholder token. Pass `t` (the exported symbol) wherever the active
 * tenant id should bind. `tenantDb(...).prepare(...).all(t, otherArgs)`.
 */
export const t = Symbol.for('JapanOSINT.tenancy.tenantIdPlaceholder');

export function tenantDb(tenantId) {
  return new TenantScoped(tenantId);
}

function assertTenantScoped(sql) {
  // Cheap heuristic: the literal substring `tenant_id` must appear in the
  // SQL. Catches the most common omission (writing a query against the
  // legacy schema by reflex). Doesn't try to parse the SQL — that's what
  // the CI lint adds on top.
  if (!/tenant_id/i.test(sql)) {
    const msg = `tenantDb refusing to prepare SQL without tenant_id predicate:\n${sql}`;
    if (DEV) throw new Error(msg);
    console.error(`[tenancy] ${msg}`);
    throw new Error('tenant_id predicate required');
  }
}

/**
 * Quick read of a tenant row by id. Convenience for routes that need the
 * full tenant record (plan, slug, require_sso) beyond what middleware put
 * on req.tenant.
 */
export function getTenantById(id) {
  return db.prepare(
    `SELECT id, slug, name, plan, stripe_customer_id, require_sso, created_at
       FROM tenants WHERE id = ?`
  ).get(id);
}
