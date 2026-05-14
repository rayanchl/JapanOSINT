# Phase 1 — Multi-tenancy + Auth foundation

Tracking doc for the multi-tenant cutover. Captures the locked tech
decisions, the files that are already in the tree, and what still needs to
land before the system goes live.

## Locked tech decisions

| Decision | Pick | Why |
| --- | --- | --- |
| Tenant isolation | Shared DB, `tenant_id` everywhere | Same model 99% of vertical SaaS uses; defer physical isolation to the first on-prem deal. |
| Primary auth | Supabase Auth | Free to 50k MAU; handles email + OAuth + magic link. |
| Auth gateway pattern | Supabase JWT for normal users; backend verifies HS256 with `SUPABASE_JWT_SECRET`. Future: also accept Jackson-issued JWTs for SSO users. | Two issuers, one user model. |
| Org / membership | Custom (`tenants`, `memberships`, `tenant_idp_connections`) | Built once, reused by every auth path. |
| API key resolution | `tenant_secrets` → `process.env` → `null`. BYOK is opt-in; platform keys are the default. | Matches the "DB keeps its own keys, user can override" call. |
| Billing | Stripe Billing (Phase 1 / Week 5). | Webhook flips `tenants.plan`; plan drives rate-limit + quota slice. |
| Break-glass | Env-gated TOTP admin path. Off by default. | Supabase outage must not lock the platform out of itself. |

## Already in the tree

| File | Role |
| --- | --- |
| `server/src/utils/tenancyMigration.js` | Additive DB migration. Creates `tenants`, `users`, `memberships`, `audit_events`, `tenant_secrets`, `tenant_quotas`, `tenant_api_keys`, `tenant_idp_connections`, `sso_group_role_map`. Seeds `legacy` tenant. Idempotent; runs every boot from `src/index.js`. |
| `server/src/middleware/auth.js` | Verifies Supabase HS256 JWT via `jose`. Bypassed entirely unless `MULTI_TENANT_ENABLED=1`. Attaches `req.supabaseUser`. |
| `server/src/middleware/tenant.js` | Materialises `users` + `tenants` + `memberships` rows on first sign-in. Picks active tenant from `X-Tenant-Id` or first membership. Attaches `req.user`, `req.tenant`, `req.role`. |
| `server/src/utils/tenancy.js` | `tenantDb(tenantId).prepare(sql)` wrapper. Refuses SQL that does not mention `tenant_id` (throws in dev, errors in prod). Use the exported `t` symbol as the placeholder for the active tenant id when binding. |
| `server/src/utils/credentials.js` | `resolveCredential(tenantId, varName)` returns `{value, source}` or `null`. AES-256-GCM with per-tenant key derived via HKDF-SHA256 from `SECRETS_MASTER_KEY`. `fallback_to_platform` flag honoured. |
| `server/src/routes/breakGlass.js` | `POST /admin/break-glass/login` mounts when `BREAK_GLASS_ENABLED=1`. Verifies TOTP from `ADMIN_TOTP_SECRET`, issues a 1h HS256 JWT signed with `BREAK_GLASS_JWT_SECRET`. Audited loudly. |

## Required env vars

| Var | Where it's used | Notes |
| --- | --- | --- |
| `MULTI_TENANT_ENABLED` | `middleware/auth.js`, `middleware/tenant.js` | Set to `1` to turn on auth + tenant resolution. Leave unset for the legacy single-tenant boot. |
| `SUPABASE_JWT_SECRET` | `middleware/auth.js` | HS256 shared secret from Supabase project settings. Required when `MULTI_TENANT_ENABLED=1`. |
| `SECRETS_MASTER_KEY` | `utils/credentials.js` | Base64 or hex, ≥32 bytes. Master from which per-tenant keys are derived. Rotate quarterly. |
| `BREAK_GLASS_ENABLED` | `routes/breakGlass.js` | Set to `1` only during an incident. The router 404s when off. |
| `ADMIN_TOTP_SECRET` | `routes/breakGlass.js` | Base32 TOTP secret. Use the standard authenticator-app format. |
| `BREAK_GLASS_JWT_SECRET` | `routes/breakGlass.js` | HS256 secret for the synthetic admin JWT. Independent from Supabase's. |

A sample lives in `.env.example`.

## Still to do (Phase 1, in order)

### Week 2 — DONE ✓

* [x] `apiCredentials.js` swept through `getEnv(tenantId, name)`. Both
  `getCredentialStatus` and `getProbeAuthHeaders` accept an optional
  `tenantId`; null passes through to env (legacy / scheduler paths).
* [x] Audit log writer middleware (`server/src/middleware/audit.js`).
  Mutations always logged; 10% sample of reads. Body fields matching
  `password|secret|token|api[_-]?key|authorization` redacted.
* [x] Rate limiter middleware (`server/src/middleware/rateLimit.js`).
  Token bucket per `(tenant_id, route_class)`. Classes: `read` 60 rpm,
  `search` 30 rpm, `mutate` 10 rpm. Plan multipliers: free 0.25×, pro 1×,
  team 4×, enterprise unlimited. `Retry-After` + `X-RateLimit-*` headers.
* [x] `tenant_id` columns added to `intel_items` (and `app_preferences` is
  iOS-side / SwiftData so no-op). Indexes:
  `idx_intel_items_tenant_fetched(tenant_id, fetched_at DESC)`,
  `idx_intel_items_tenant_source(tenant_id, source_id)`.
* [x] Middleware wired into `/api/*` behind `MULTI_TENANT_ENABLED`. Order:
  `requireSupabaseAuth → resolveTenant → rateLimit → auditWriter`. Health
  check (`/api/health`) is mounted BEFORE the auth stack so monitoring
  works during an outage.

### Remaining Week 2 (collector-side, deferred to its own session)

* [ ] Sweep direct `process.env.X` reads inside individual collectors
  (~25-30 sites under `server/src/collectors/`). The hub
  (`apiCredentials.js`) already routes through `getEnv`, but module-load
  top-level captures like `const KEY = process.env.X` need to be moved
  inside their function bodies and threaded through with a tenantId. The
  scheduler/cron paths can pass `null` as the tenantId (platform-only).
  Hot sites: `cameraDiscovery.js` (Windy, Shodan, YouTube), `censysJapan.js`,
  `flightAdsb.js`, `marineTraffic.js`, `edinetFilings.js`, `fofaJp.js`,
  `quake360Jp.js`, `greynoiseJp.js`, `grayhatBuckets.js`,
  `wifiNetworksShodan.js`, `shodanIot.js`, `nasaFirmsJp.js`,
  `houjinBangou.js`, `dehashedBreach.js`, the OPENSKY / AERODATABOX pair.
  Mechanical but high blast-radius — split across at least two PRs.

### Week 3

* [ ] Add `tenant_id` columns to existing user-data tables (`intel_items`,
  `app_preferences`, `fetch_log` if user-scoped). Use
  `ADD COLUMN tenant_id TEXT NOT NULL DEFAULT 'legacy'` so existing rows
  backfill. Add `(tenant_id, fetched_at DESC)` + `(tenant_id, source_id)`
  indexes.
* [ ] Roles enforcement: `requires('admin')` decorator on routes that
  mutate billing / integrations / memberships.
* [ ] Invites: `POST /tenants/:id/invites` issues a magic-link via Supabase
  Admin SDK; webhook sync writes the membership with the chosen role.
* [ ] REST API keys: `tenant_api_keys` CRUD endpoints + bearer-token auth
  middleware that recognises `Authorization: Bearer sk_live_…`.
* [ ] Web: Settings → Team page, Settings → API keys page, Settings →
  Integrations page (platform-key vs BYOK toggle per source with live quota
  display).

### Week 4 — Jackson (SAML + SCIM)

* [ ] Deploy BoxyHQ Jackson as a sidecar (Docker). Wire admin API behind the
  owner role. Per-tenant connection storage in `tenant_idp_connections`.
* [ ] Backend accepts Jackson-issued JWTs in addition to Supabase JWTs.
  Update `middleware/auth.js` to try both issuers via JWKS.
* [ ] SCIM webhook → `memberships` sync. `sso_group_role_map` for group →
  role mapping.
* [ ] Web: Settings → SSO page with SAML wizard + SCIM endpoint generator.
* [ ] Conflict resolution for IdP-deprovisioned users (transfer BYOK
  secrets + API keys to tenant owner; audit-log).

### Week 5 — Stripe + hardening

* [ ] Stripe Billing: products + checkout + webhook. Plan drives
  rate-limit + quota slice + `require_sso` for Enterprise.
* [ ] Secret rotation worker: re-wrap every `tenant_secrets.encrypted_value`
  against a new master every 90 days.
* [ ] Data export endpoint: streams tenant's `intel_items` + `cameras` as
  JSONL. GDPR / Japan APPI freebie.
* [ ] `tenants.require_sso` enforcement: reject non-SSO logins to a tenant
  when set. Closes the shadow-account compliance gap.

## Risks to watch

1. **Tenant-id leakage** — any direct `db.prepare(...)` against a
   user-data table that omits `tenant_id` is a data-leak vector. The
   `tenantDb` wrapper enforces this for new code; existing code paths
   need a separate sweep tracked in Week 3.
2. **Two JWT issuers (Week 4)** — middleware will need to handle both
   Supabase and Jackson tokens cleanly. Test matrix: (Supabase user,
   Jackson user) × (free tenant, enterprise SSO-required tenant).
3. **SCIM event storms (Week 4)** — a new customer onboarding can fire
   thousands of SCIM creates in minutes. Queue + worker on our side.
4. **Encryption master key rotation** — `SECRETS_MASTER_KEY` rotation
   must re-wrap every `tenant_secrets` row before the old key is
   discarded. Worker stub in Week 5.

## Quick smoke test

Run from the server directory:

```bash
node -e "
import('./src/utils/tenancyMigration.js').then(m => m.runTenancyMigration());
"
```

Re-running is safe — every `CREATE TABLE` is `IF NOT EXISTS` and the
`legacy` tenant seed is `INSERT OR IGNORE`.

For the credential round-trip:

```bash
SECRETS_MASTER_KEY=$(openssl rand -base64 32) node -e "
import('./src/utils/credentials.js').then(({setTenantSecret, resolveCredential}) => {
  setTenantSecret('legacy', 'EXAMPLE_KEY', 'sk_test_123');
  console.log(resolveCredential('legacy', 'EXAMPLE_KEY'));
});
"
```
