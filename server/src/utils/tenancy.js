/**
 * Enforced tenant-scoped query helper.
 *
 * Every read/write of a genuinely per-tenant table MUST go through
 * `tenantDb(tenantId)`. The guarantee is structural, not by convention:
 *
 *   1. `prepare(sql)` throws unless the SQL binds the named parameter
 *      `@tenant_id`. You cannot forget the predicate — the statement won't
 *      compile through this helper.
 *   2. Statements take a NAMED-parameter object (positional args are
 *      rejected). The helper *injects* `tenant_id` itself, overriding any
 *      caller-supplied value, so a query can never run for the wrong
 *      tenant — the caller doesn't get to choose the tenant id.
 *   3. Fail closed: a missing/!named binding throws (in every NODE_ENV).
 *      A missing tenant predicate is a cross-tenant data-leak bug; it must
 *      never silently execute.
 *
 * Usage:
 *   import { tenantDb } from '../utils/tenancy.js';
 *
 *   tenantDb(req.tenant.id)
 *     .prepare('SELECT * FROM alert_rules WHERE id = @id AND tenant_id = @tenant_id')
 *     .get({ id });                       // tenant_id injected for you
 *
 * Scope: genuinely per-tenant tables serving ONE tenant's request —
 * `alert_rules`, `alert_events`, `tenant_secrets`, `audit_events` (reads),
 * `memberships` (tenant-scoped reads).
 *
 * DELIBERATE EXCEPTIONS (must use raw `db`, each documented at the site):
 *   - utils/alertEngine.js  — evaluates the shared intel pool against
 *     EVERY tenant's rules; intentionally cross-tenant.
 *   - utils/auditChain.js   — append-only hash-chained writer for any
 *     tenant id incl. the synthetic 'platform' scope.
 *   - routes/breakGlass.js  — platform-scope audit writes during an outage.
 *   - middleware/tenant.js / utils/tenancyMigration.js — tenancy infra
 *     that operates across/over the tenant tables themselves.
 * `intel_items` is platform-global by design (see intelStore.js) and is
 * NEVER routed through here.
 */

import db from './database.js';

class TenantStatement {
  constructor(stmt, tenantId) {
    this._stmt = stmt;
    this._tenantId = tenantId;
  }

  // Reject positional args and force the tenant id we resolved — the
  // caller cannot target another tenant even by passing tenant_id.
  _params(p) {
    if (p === undefined) p = {};
    if (p === null || typeof p !== 'object' || Array.isArray(p)) {
      throw new Error(
        '[tenancy] tenant-scoped statements take a named-parameter object, not positional args'
      );
    }
    return { ...p, tenant_id: this._tenantId };
  }

  get(p) { return this._stmt.get(this._params(p)); }
  all(p) { return this._stmt.all(this._params(p)); }
  run(p) { return this._stmt.run(this._params(p)); }
}

class TenantDb {
  constructor(tenantId) {
    this._tenantId = tenantId;
  }

  prepare(sql) {
    if (!/@tenant_id\b/.test(sql)) {
      throw new Error(
        '[tenancy] tenant-scoped query must bind @tenant_id — refusing to ' +
        `prepare:\n${String(sql).trim().slice(0, 240)}`
      );
    }
    return new TenantStatement(db.prepare(sql), this._tenantId);
  }

  // Run several scoped statements atomically and return the callback's
  // result. The callback receives this TenantDb so nested statements stay
  // tenant-scoped. Executes immediately (unlike raw better-sqlite3).
  transaction(fn) {
    return db.transaction(() => fn(this))();
  }
}

/**
 * @param {string} tenantId  active tenant (req.tenant.id). Required.
 * @returns {TenantDb}
 */
export function tenantDb(tenantId) {
  if (!tenantId || typeof tenantId !== 'string') {
    throw new Error('[tenancy] tenantDb(tenantId) requires a non-empty tenant id');
  }
  return new TenantDb(tenantId);
}
