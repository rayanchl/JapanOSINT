// Phase 5 smoke — per-tenant BYOK + key-edit policy.
//
//   A. credentials: tenant secret is isolated, encrypted, and falls back to
//      the platform (process.env) key; clearing reverts to platform.
//   B. policy: getKeyWritePolicy defaults to owner_only; canManageTenantKeys
//      honours role (owner/admin always) and the owner-chosen policy.
//   C. requirePlatformAdmin: 403 for non-admins, next() for owner/admin.
//
//   node test/phase5_tenant_keys.smoke.mjs

import { randomUUID } from 'crypto';

// Master key must exist before credentials.js derives anything. The smoke is
// run without --env-file, so set a deterministic 32-byte base64 key here.
process.env.SECRETS_MASTER_KEY = Buffer.alloc(32, 7).toString('base64');

import db from '../src/utils/database.js';
import { runTenancyMigration } from '../src/utils/tenancyMigration.js';
import {
  setTenantSecret,
  deleteTenantSecret,
  getTenantSecret,
  resolveCredential,
} from '../src/utils/credentials.js';
import {
  getKeyWritePolicy,
  canManageTenantKeys,
  requirePlatformAdmin,
} from '../src/middleware/keyAccess.js';

function assert(c, m) {
  if (!c) throw new Error(m);
}

const VAR = 'SHODAN_API_KEY';
const A = `t_${randomUUID()}`;
const B = `t_${randomUUID()}`;
const U_OWNER = `u_${randomUUID()}`;
const U_DELEGATE = `u_${randomUUID()}`;
const U_OTHER = `u_${randomUUID()}`;

function mkTenant(id) {
  db.prepare(`INSERT INTO tenants (id, slug, name) VALUES (?, ?, ?)`)
    .run(id, `slug-${id.slice(0, 12)}`, `WS ${id.slice(0, 6)}`);
}

function cleanup() {
  for (const t of [A, B]) {
    db.prepare(`DELETE FROM tenant_secrets WHERE tenant_id = ?`).run(t);
    db.prepare(`DELETE FROM tenants WHERE id = ?`).run(t);
  }
}

function run() {
  runTenancyMigration(); // ensure key_write_* columns + tenant_secrets exist
  cleanup();
  mkTenant(A);
  mkTenant(B);

  // ── A. isolation + encryption + platform fallback ──
  const ENV_BEFORE = process.env[VAR];
  process.env[VAR] = 'PLATFORM_DEFAULT'; // simulate operator .env key

  setTenantSecret(A, VAR, 'TENANT_A_SECRET', { createdBy: U_OWNER });

  assert(getTenantSecret(A, VAR) === 'TENANT_A_SECRET', 'A reveal own key');
  assert(getTenantSecret(B, VAR) === null, 'B has no own key');

  const stored = db.prepare(
    `SELECT encrypted_value FROM tenant_secrets WHERE tenant_id = ? AND key_name = ?`,
  ).get(A, VAR);
  assert(
    Buffer.isBuffer(stored.encrypted_value) &&
      !stored.encrypted_value.toString('utf8').includes('TENANT_A_SECRET'),
    'stored value must be ciphertext, not plaintext',
  );

  const rA = resolveCredential(A, VAR);
  assert(rA && rA.value === 'TENANT_A_SECRET' && rA.source === 'tenant',
    `A resolves to its own key, got ${JSON.stringify(rA)}`);
  const rB = resolveCredential(B, VAR);
  assert(rB && rB.value === 'PLATFORM_DEFAULT' && rB.source === 'platform',
    `B falls back to platform, got ${JSON.stringify(rB)}`);

  deleteTenantSecret(A, VAR);
  const rA2 = resolveCredential(A, VAR);
  assert(rA2 && rA2.source === 'platform',
    `after clear, A falls back to platform, got ${JSON.stringify(rA2)}`);
  console.log('A isolation/encryption/fallback: OK');

  if (ENV_BEFORE === undefined) delete process.env[VAR];
  else process.env[VAR] = ENV_BEFORE;

  // ── B. policy resolution ──
  assert(getKeyWritePolicy(A).policy === 'owner_only', 'default policy owner_only');

  const reqOwner = { role: 'owner', user: { id: U_OWNER }, tenant: { id: A } };
  const reqAdmin = { role: 'admin', user: { id: U_OTHER }, tenant: { id: A } };
  const reqAnalyst = { role: 'analyst', user: { id: U_OTHER }, tenant: { id: A } };
  const reqDelegate = { role: 'analyst', user: { id: U_DELEGATE }, tenant: { id: A } };

  assert(canManageTenantKeys(reqOwner), 'owner always can');
  assert(canManageTenantKeys(reqAdmin), 'admin always can');
  assert(!canManageTenantKeys(reqAnalyst), 'analyst cannot under owner_only');

  db.prepare(`UPDATE tenants SET key_write_policy = 'all_members' WHERE id = ?`).run(A);
  assert(canManageTenantKeys(reqAnalyst), 'analyst can under all_members');

  db.prepare(
    `UPDATE tenants SET key_write_policy = 'selected_member', key_write_member_id = ? WHERE id = ?`,
  ).run(U_DELEGATE, A);
  assert(canManageTenantKeys(reqDelegate), 'delegate can under selected_member');
  assert(!canManageTenantKeys(reqAnalyst), 'non-delegate cannot under selected_member');
  assert(canManageTenantKeys(reqAdmin), 'admin still can under selected_member');
  console.log('B policy resolution: OK');

  // ── C. requirePlatformAdmin guard ──
  function fakeRes() {
    return {
      code: 0,
      body: null,
      status(c) { this.code = c; return this; },
      json(b) { this.body = b; return this; },
    };
  }
  let nexted = false;
  const res1 = fakeRes();
  requirePlatformAdmin({ role: 'analyst' }, res1, () => { nexted = true; });
  assert(!nexted && res1.code === 403, 'analyst blocked from platform overlay');

  nexted = false;
  const res2 = fakeRes();
  requirePlatformAdmin({ role: 'owner' }, res2, () => { nexted = true; });
  assert(nexted && res2.code === 0, 'owner passes platform guard');

  nexted = false;
  requirePlatformAdmin({ role: 'admin' }, fakeRes(), () => { nexted = true; });
  assert(nexted, 'admin passes platform guard');
  console.log('C requirePlatformAdmin: OK');

  cleanup();
  console.log('\nPhase 5 smoke: OK');
}

try {
  run();
  process.exit(0);
} catch (err) {
  console.error('SMOKE FAIL:', err);
  try { cleanup(); } catch { /* ignore */ }
  process.exit(1);
}
