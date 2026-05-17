/**
 * Per-tenant collector usage accounting.
 *
 * Every request-scoped collector / search run that consumes an upstream
 * credential increments one row in `tenant_quotas`, keyed by
 * (tenant_id, source_id, day):
 *
 *   - byok_used      the tenant's own BYOK key paid the upstream bill
 *   - platform_used  the run drew on the shared platform key (the tenant's
 *                     slice of our budget)
 *
 * A run that uses no key at all (free / keyless sources) writes nothing —
 * there is no cost to attribute. Counts are per-run (one collector
 * invocation = one unit), which is the granularity the scheduler/cron and
 * on-demand routes operate at.
 *
 * tenant_quotas is genuinely per-tenant, so all access goes through the
 * enforced `tenantDb` wrapper (binds @tenant_id; rejects positional args).
 */

import { tenantDb } from './tenancy.js';

function utcDay(d = new Date()) {
  return d.toISOString().slice(0, 10); // YYYY-MM-DD (UTC)
}

/**
 * Increment today's counters for (tenant, source). No-op when nothing
 * billable was consumed, or when called outside a tenant context. Never
 * throws — usage accounting must not break a collector response.
 *
 * @param {object} a
 * @param {string} a.tenantId
 * @param {string} a.sourceId
 * @param {boolean} a.usedTenant    a BYOK credential resolved during the run
 * @param {boolean} a.usedPlatform  a platform credential resolved during the run
 */
export function recordCollectorUsage({ tenantId, sourceId, usedTenant, usedPlatform }) {
  if (!tenantId || !sourceId) return;
  const b = usedTenant ? 1 : 0;
  const p = usedPlatform ? 1 : 0;
  if (b === 0 && p === 0) return;
  try {
    tenantDb(tenantId).prepare(`
      INSERT INTO tenant_quotas (tenant_id, source_id, day, platform_used, byok_used)
      VALUES (@tenant_id, @source_id, @day, @p, @b)
      ON CONFLICT (tenant_id, source_id, day) DO UPDATE SET
        platform_used = platform_used + @p,
        byok_used     = byok_used + @b
    `).run({ source_id: sourceId, day: utcDay(), p, b });
  } catch (err) {
    console.warn('[usage] record failed:', err?.message);
  }
}

/**
 * Aggregate a tenant's usage over the trailing `days` window, grouped by
 * source plus a grand total. Backs GET /api/billing/usage.
 */
export function tenantUsage(tenantId, days = 30) {
  const win = Math.min(Math.max(Number(days) || 30, 1), 365);
  const since = utcDay(new Date(Date.now() - win * 86_400_000));
  const bySource = tenantDb(tenantId).prepare(`
    SELECT source_id,
           SUM(platform_used) AS platform_used,
           SUM(byok_used)     AS byok_used,
           MAX(day)           AS last_day
      FROM tenant_quotas
     WHERE tenant_id = @tenant_id AND day >= @since
     GROUP BY source_id
     ORDER BY (SUM(platform_used) + SUM(byok_used)) DESC
  `).all({ since });

  const totals = bySource.reduce(
    (acc, r) => {
      acc.platform_used += r.platform_used || 0;
      acc.byok_used += r.byok_used || 0;
      return acc;
    },
    { platform_used: 0, byok_used: 0 },
  );

  return { window_days: win, since, totals, by_source: bySource };
}
