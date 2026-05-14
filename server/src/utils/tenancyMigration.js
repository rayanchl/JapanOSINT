/**
 * Tenancy schema migration. Additive only — does not touch existing
 * user-data tables (intel_items, app_preferences, fetch_log, …) yet so the
 * legacy single-tenant app keeps working unchanged until routes are wired
 * to the new model.
 *
 * Run unconditionally at boot. Idempotent via `CREATE TABLE IF NOT EXISTS`.
 * Seeds a `legacy` tenant on first run so any future tenant_id columns can
 * default-backfill to it.
 *
 * See docs/PHASE1.md for the multi-tenant cutover plan.
 */

import db from './database.js';

export function runTenancyMigration() {
  db.exec(`
    -- Tenants: one row per organisation. Plan controls rate-limit slice,
    -- platform-key quota, and feature gates (e.g. require_sso for enterprise).
    CREATE TABLE IF NOT EXISTS tenants (
      id                  TEXT PRIMARY KEY,
      slug                TEXT UNIQUE NOT NULL,
      name                TEXT NOT NULL,
      plan                TEXT NOT NULL DEFAULT 'free'
                          CHECK(plan IN ('free','pro','team','enterprise')),
      stripe_customer_id  TEXT,
      require_sso         INTEGER NOT NULL DEFAULT 0,
      created_at          TEXT NOT NULL DEFAULT (datetime('now'))
    );

    -- Users: one row per authenticated identity. Joined to Supabase via
    -- supabase_user_id; future SSO users will join via the same row.
    CREATE TABLE IF NOT EXISTS users (
      id                 TEXT PRIMARY KEY,
      supabase_user_id   TEXT UNIQUE,
      email              TEXT NOT NULL,
      display_name       TEXT,
      created_at         TEXT NOT NULL DEFAULT (datetime('now'))
    );

    CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);

    -- Memberships: (user, tenant, role). A user can belong to multiple
    -- tenants; for v1 we create one personal tenant per user at sign-up.
    CREATE TABLE IF NOT EXISTS memberships (
      user_id    TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
      tenant_id  TEXT NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
      role       TEXT NOT NULL DEFAULT 'analyst'
                 CHECK(role IN ('owner','admin','analyst','viewer')),
      created_at TEXT NOT NULL DEFAULT (datetime('now')),
      PRIMARY KEY (user_id, tenant_id)
    );

    CREATE INDEX IF NOT EXISTS idx_memberships_tenant ON memberships(tenant_id);

    -- Audit events: every state-changing action; 10% sample of reads.
    CREATE TABLE IF NOT EXISTS audit_events (
      id           TEXT PRIMARY KEY,
      tenant_id    TEXT NOT NULL,
      user_id      TEXT,
      action       TEXT NOT NULL,
      target       TEXT,
      payload_json TEXT,
      ts           TEXT NOT NULL DEFAULT (datetime('now')),
      ip           TEXT,
      ua           TEXT
    );

    CREATE INDEX IF NOT EXISTS idx_audit_tenant_ts
      ON audit_events(tenant_id, ts DESC);

    -- Tenant secrets (BYOK). Per-tenant key derived from master via HKDF;
    -- encrypted_value is AES-256-GCM ciphertext with the nonce prefixed.
    -- fallback_to_platform: when the user's key fails (401/quota), should
    -- the collector silently retry with the platform key? Default yes for
    -- Free/Pro/Team; the Enterprise default flips this to 0 (compliance).
    CREATE TABLE IF NOT EXISTS tenant_secrets (
      tenant_id            TEXT NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
      key_name             TEXT NOT NULL,
      encrypted_value      BLOB NOT NULL,
      fallback_to_platform INTEGER NOT NULL DEFAULT 1,
      created_by           TEXT,
      created_at           TEXT NOT NULL DEFAULT (datetime('now')),
      last_used_at         TEXT,
      PRIMARY KEY (tenant_id, key_name)
    );

    -- Per-day per-source quota counters. Used by rate-limit middleware.
    -- platform_used decrements the tenant's slice of the platform-key budget;
    -- byok_used is informational (the tenant pays the upstream bill).
    CREATE TABLE IF NOT EXISTS tenant_quotas (
      tenant_id       TEXT NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
      source_id       TEXT NOT NULL,
      day             TEXT NOT NULL,
      platform_used   INTEGER NOT NULL DEFAULT 0,
      byok_used       INTEGER NOT NULL DEFAULT 0,
      PRIMARY KEY (tenant_id, source_id, day)
    );

    -- Machine-to-machine API keys. Hashed with SHA-256 at rest; full key
    -- is shown once on creation. scopes_json is a [] of route classes.
    CREATE TABLE IF NOT EXISTS tenant_api_keys (
      id           TEXT PRIMARY KEY,
      tenant_id    TEXT NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
      name         TEXT NOT NULL,
      hashed_key   TEXT NOT NULL UNIQUE,
      scopes_json  TEXT NOT NULL DEFAULT '[]',
      created_by   TEXT,
      created_at   TEXT NOT NULL DEFAULT (datetime('now')),
      last_used_at TEXT,
      revoked_at   TEXT
    );

    CREATE INDEX IF NOT EXISTS idx_apikeys_tenant ON tenant_api_keys(tenant_id);

    -- Pointer to a per-tenant SAML/SCIM connection in BoxyHQ Jackson (or
    -- equivalent). One row each for SSO + SCIM. Owned by Jackson; we just
    -- store the connection id so we can call its admin API.
    CREATE TABLE IF NOT EXISTS tenant_idp_connections (
      tenant_id     TEXT NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
      kind          TEXT NOT NULL CHECK(kind IN ('saml','scim')),
      provider_id   TEXT NOT NULL,        -- Jackson connection id
      status        TEXT NOT NULL DEFAULT 'pending',
      last_synced   TEXT,
      created_at    TEXT NOT NULL DEFAULT (datetime('now')),
      PRIMARY KEY (tenant_id, kind)
    );

    -- IdP group -> app role mapping (used by SCIM auto-provisioning when
    -- group memberships arrive).
    CREATE TABLE IF NOT EXISTS sso_group_role_map (
      tenant_id   TEXT NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
      group_name  TEXT NOT NULL,
      role        TEXT NOT NULL CHECK(role IN ('owner','admin','analyst','viewer')),
      PRIMARY KEY (tenant_id, group_name)
    );
  `);

  // Seed the legacy tenant. Existing single-tenant data will migrate into
  // this row when tenant_id columns are added in a follow-up.
  db.prepare(`
    INSERT OR IGNORE INTO tenants (id, slug, name, plan)
    VALUES ('legacy', 'legacy', 'Legacy (pre-multi-tenant)', 'enterprise')
  `).run();
}
