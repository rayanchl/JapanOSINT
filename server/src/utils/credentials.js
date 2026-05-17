/**
 * Credential resolution for upstream-API-keyed sources.
 *
 * Resolution order:
 *   1. tenant_secrets[tenantId][varName]     ← BYOK (decrypted on demand)
 *   2. process.env[varName]                  ← platform default
 *   3. null                                  ← gated; source unusable
 *
 * Encryption: AES-256-GCM with a per-tenant 32-byte key derived from the
 * SECRETS_MASTER_KEY (env, base64 or hex) via HKDF-SHA256. The encrypted
 * BLOB is laid out as:
 *     [12-byte nonce][16-byte auth tag][N-byte ciphertext]
 *
 * No third-party deps — Node's `crypto` module covers HKDF + AES-GCM.
 * Master key rotation is a separate worker that re-wraps every row against
 * a new master; not implemented here.
 */

import {
  createCipheriv, createDecipheriv, randomBytes, hkdfSync,
} from 'crypto';
import { tenantDb } from './tenancy.js';

const ALGO = 'aes-256-gcm';
const NONCE_LEN = 12;
const TAG_LEN = 16;
const KEY_LEN = 32;

// HKDF info string. Different per use to keep keys isolated by purpose.
const HKDF_INFO = Buffer.from('JapanOSINT.tenant_secrets.v1');

let _masterKey = null;
function getMasterKey() {
  if (_masterKey) return _masterKey;
  const raw = process.env.SECRETS_MASTER_KEY;
  if (!raw) {
    throw new Error('SECRETS_MASTER_KEY is not set; cannot encrypt tenant secrets');
  }
  // Accept either base64 (recommended) or hex.
  let key;
  if (/^[a-f0-9]{64}$/i.test(raw)) {
    key = Buffer.from(raw, 'hex');
  } else {
    key = Buffer.from(raw, 'base64');
  }
  if (key.length < 32) {
    throw new Error('SECRETS_MASTER_KEY must decode to at least 32 bytes');
  }
  _masterKey = key.slice(0, 32);
  return _masterKey;
}

function deriveTenantKey(tenantId) {
  const master = getMasterKey();
  return Buffer.from(
    hkdfSync('sha256', master, Buffer.from(tenantId), HKDF_INFO, KEY_LEN)
  );
}

function encrypt(tenantId, plaintext) {
  const key = deriveTenantKey(tenantId);
  const nonce = randomBytes(NONCE_LEN);
  const cipher = createCipheriv(ALGO, key, nonce);
  const ct = Buffer.concat([cipher.update(plaintext, 'utf8'), cipher.final()]);
  const tag = cipher.getAuthTag();
  return Buffer.concat([nonce, tag, ct]);
}

function decrypt(tenantId, blob) {
  if (!Buffer.isBuffer(blob) || blob.length < NONCE_LEN + TAG_LEN) {
    throw new Error('tenant_secret blob too short');
  }
  const key = deriveTenantKey(tenantId);
  const nonce = blob.slice(0, NONCE_LEN);
  const tag = blob.slice(NONCE_LEN, NONCE_LEN + TAG_LEN);
  const ct = blob.slice(NONCE_LEN + TAG_LEN);
  const decipher = createDecipheriv(ALGO, key, nonce);
  decipher.setAuthTag(tag);
  return Buffer.concat([decipher.update(ct), decipher.final()]).toString('utf8');
}

// ── Public API ──────────────────────────────────────────────────────────────

/**
 * Look up a credential for a tenant.
 *
 * Returns:
 *   { value, source }  // source ∈ 'tenant' | 'platform'
 *   null               // gated — no key available
 *
 * Pass `tenantId = null` for platform-scoped callers (scheduler / cron /
 * collectors with no request context); we fall straight through to
 * process.env.
 */
/**
 * Convenience for callers that only need the string value: returns the
 * credential value or null. Identical resolution order to
 * resolveCredential — drop-in replacement for `process.env[name]`.
 *
 *   process.env.SHODAN_API_KEY   →   getEnv(tenantId, 'SHODAN_API_KEY')
 *
 * Pass tenantId=null for platform-only resolution (scheduler / cron paths
 * that have no request context).
 */
export function getEnv(tenantId, varName) {
  const r = resolveCredential(tenantId, varName);
  return r ? r.value : null;
}

export function resolveCredential(tenantId, varName) {
  if (tenantId) {
    const row = tenantDb(tenantId).prepare(`
      SELECT encrypted_value, fallback_to_platform
        FROM tenant_secrets
       WHERE tenant_id = @tenant_id AND key_name = @key_name
    `).get({ key_name: varName });
    if (row && row.encrypted_value) {
      try {
        const value = decrypt(tenantId, row.encrypted_value);
        touchLastUsed(tenantId, varName);
        return { value, source: 'tenant' };
      } catch (err) {
        console.error(`[credentials] decrypt failed for ${tenantId}/${varName}:`, err.message);
        // Decrypt failure: fall through only if the tenant has opted in.
        if (!row.fallback_to_platform) return null;
      }
    }
  }

  const env = process.env[varName];
  if (typeof env === 'string' && env.trim().length > 0) {
    return { value: env, source: 'platform' };
  }
  return null;
}

/**
 * Upsert a tenant's BYOK secret.
 *
 * @param {string} tenantId
 * @param {string} varName
 * @param {string} plaintext
 * @param {object} opts                 - { fallbackToPlatform, createdBy }
 */
export function setTenantSecret(tenantId, varName, plaintext, opts = {}) {
  if (!plaintext || typeof plaintext !== 'string') {
    throw new Error('plaintext must be a non-empty string');
  }
  const blob = encrypt(tenantId, plaintext);
  const fallback = opts.fallbackToPlatform === false ? 0 : 1;
  tenantDb(tenantId).prepare(`
    INSERT INTO tenant_secrets (tenant_id, key_name, encrypted_value, fallback_to_platform, created_by)
    VALUES (@tenant_id, @key_name, @encrypted_value, @fallback, @created_by)
    ON CONFLICT (tenant_id, key_name) DO UPDATE SET
      encrypted_value = excluded.encrypted_value,
      fallback_to_platform = excluded.fallback_to_platform,
      last_used_at = NULL
  `).run({
    key_name: varName,
    encrypted_value: blob,
    fallback,
    created_by: opts.createdBy || null,
  });
}

/**
 * Decrypt and return ONLY this tenant's own stored secret (never the
 * platform/.env fallback). Returns null when the tenant has no row for
 * `varName`. Used by the BYOK reveal flow so a user can see the key they
 * themselves entered without exposing the operator's platform key.
 */
export function getTenantSecret(tenantId, varName) {
  if (!tenantId) return null;
  const row = tenantDb(tenantId).prepare(`
    SELECT encrypted_value FROM tenant_secrets
     WHERE tenant_id = @tenant_id AND key_name = @key_name
  `).get({ key_name: varName });
  if (!row || !row.encrypted_value) return null;
  try {
    return decrypt(tenantId, row.encrypted_value);
  } catch (err) {
    console.error(`[credentials] reveal decrypt failed for ${tenantId}/${varName}:`, err.message);
    return null;
  }
}

export function deleteTenantSecret(tenantId, varName) {
  tenantDb(tenantId).prepare(`
    DELETE FROM tenant_secrets WHERE tenant_id = @tenant_id AND key_name = @key_name
  `).run({ key_name: varName });
}

export function listTenantSecrets(tenantId) {
  // Never return plaintext. Just metadata for the Integrations UI.
  return tenantDb(tenantId).prepare(`
    SELECT key_name, fallback_to_platform, created_at, last_used_at
      FROM tenant_secrets
     WHERE tenant_id = @tenant_id
     ORDER BY key_name
  `).all();
}

function touchLastUsed(tenantId, varName) {
  // Async-ish: fire-and-forget update on read path. Tiny write, low contention.
  try {
    tenantDb(tenantId).prepare(`
      UPDATE tenant_secrets SET last_used_at = datetime('now')
       WHERE tenant_id = @tenant_id AND key_name = @key_name
    `).run({ key_name: varName });
  } catch { /* read path — never throw on metadata write */ }
}
