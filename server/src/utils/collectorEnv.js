/**
 * Tenant-aware credential accessor for collectors.
 *
 * Collectors historically called `getEnv(null, 'SOME_API_KEY')`, which
 * hard-bypasses BYOK by forcing platform-only resolution. `envFor(name)`
 * is the drop-in replacement: it reads the in-flight collector run's tenant
 * (via collectorTap's AsyncLocalStorage) and resolves the credential with
 * the full BYOK precedence (tenant secret → process.env → null).
 *
 *   Before:  const key = getEnv(null, 'SHODAN_API_KEY');
 *   After:   const key = envFor('SHODAN_API_KEY');
 *
 * Outside any collector run (scheduler / cron / platform jobs) the tenant
 * is null and resolution falls straight through to process.env — identical
 * to the previous behavior, so scheduler runs are unaffected.
 */

import { resolveCredential } from './credentials.js';
import { currentTenant, noteCredentialUse } from './collectorTap.js';

/**
 * Resolve a credential for the current run's tenant (or platform fallback)
 * and record which source it came from so the run's usage is metered
 * (tenant BYOK vs platform key) into tenant_quotas. Return shape is
 * unchanged (string|null) — drop-in for the previous getEnv-based impl.
 * @param {string} varName
 * @returns {string|null}
 */
export function envFor(varName) {
  const r = resolveCredential(currentTenant(), varName);
  if (r) noteCredentialUse(r.source);
  return r ? r.value : null;
}
