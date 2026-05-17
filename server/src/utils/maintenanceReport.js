// Phase 4 — fix-success metrics + daily digest.
//
// One read-only view over the maintenance pod for the admin UI and a daily
// log line: what got auto-fixed, what's quarantined, what's awaiting PR
// review, and the fix-success rate per failure class / per source.
//
// Pure aggregation — no writes, no LLM, safe to call from an HTTP handler.

import cron from 'node-cron';
import {
  getRepairTotals,
  getRepairSuccessBreakdown,
  getQuarantinedSources,
  getRecentRepairs,
} from './database.js';
import { llmConcurrencySnapshot } from './llmConcurrency.js';

const DIGEST_CRON = process.env.MAINT_DIGEST_CRON || '0 8 * * *'; // daily 08:00
const DIGEST_WINDOW_H = Number(process.env.MAINT_DIGEST_WINDOW_HOURS || 24);

function rate(success, fail) {
  const total = success + fail;
  return total === 0 ? null : Number((success / total).toFixed(3));
}

/**
 * The full maintenance digest over the trailing `hours`. Shape is stable —
 * the iOS admin tab and the daily log both read it.
 */
export function buildMaintenanceDigest({ hours = DIGEST_WINDOW_H } = {}) {
  const totals = Object.fromEntries(
    getRepairTotals(hours).map((r) => [r.status, r.n]),
  );
  const { byClass, bySource } = getRepairSuccessBreakdown(hours);
  const quarantined = getQuarantinedSources();

  // `verified` rows are three different things. Segment so an operator can
  // tell "needs my action" from "already handled":
  //   - awaiting_pr:    url_swap with a PR open  -> review & merge the PR
  //   - awaiting_apply: url_swap, no PR (dry/parked) -> apply or flip REPAIR_GIT
  //   - auto_dismissed: transient that recovered on its own -> nothing to do
  const verified = getRecentRepairs('verified', hours);
  const swaps = verified.filter((r) => r.action === 'url_swap');

  return {
    generated_at: new Date().toISOString(),
    window_hours: hours,
    totals: {
      verified: totals.verified || 0,
      merged: totals.merged || 0,
      rejected: totals.rejected || 0,
      needs_human: totals.needs_human || 0,
      error: totals.error || 0,
    },
    success_by_class: byClass.map((c) => ({
      class: c.klass,
      success: c.success,
      fail: c.fail,
      needs_human: c.needs_human,
      success_rate: rate(c.success, c.fail),
    })),
    worst_sources: bySource.map((s) => ({
      source_id: s.source_id,
      success: s.success,
      fail: s.fail,
      success_rate: rate(s.success, s.fail),
    })),
    quarantined: quarantined.map((q) => ({
      source_id: q.id,
      name: q.name,
      category: q.category,
      since: q.quarantined_at,
      until: q.quarantined_until,
      active: !!q.active, // false = half-open, awaiting a healthy probe
      reason: q.quarantine_reason,
    })),
    auto_fixed: getRecentRepairs('merged', hours),
    awaiting_review: {
      awaiting_pr: swaps.filter((r) => r.pr_url),
      awaiting_apply: swaps.filter((r) => !r.pr_url),
    },
    auto_dismissed: verified.filter((r) => r.action === 'auto_dismiss'),
    needs_human: getRecentRepairs('needs_human', hours),
    concurrency: llmConcurrencySnapshot(),
  };
}

/** Compact one-screen summary for the daily cron log. */
export function logDailyDigest() {
  try {
    const d = buildMaintenanceDigest();
    const t = d.totals;
    const classes = d.success_by_class
      .map((c) => `${c.class}=${c.success}/${c.success + c.fail}`)
      .join(' ');
    console.log(
      [
        '────────────────────────────────────────────────────────────────',
        ` Maintenance digest (last ${d.window_hours}h)`,
        `   auto-fixed(merged)=${t.merged}  awaiting-PR=${d.awaiting_review.awaiting_pr.length}  awaiting-apply=${d.awaiting_review.awaiting_apply.length}  auto-dismissed=${d.auto_dismissed.length}`,
        `   rejected=${t.rejected}  needs_human=${t.needs_human}  error=${t.error}`,
        `   quarantined=${d.quarantined.length}` +
          (d.quarantined.length
            ? ` [${d.quarantined.map((q) => q.source_id + (q.active ? '' : '*')).join(', ')}]  (*=half-open)`
            : ''),
        `   success by class: ${classes || '(none)'}`,
        '────────────────────────────────────────────────────────────────',
      ].join('\n'),
    );
  } catch (err) {
    console.warn('[maint] daily digest failed:', err?.message);
  }
}

let digestTask = null;

/**
 * Schedule the daily digest log. Always safe to call — it only reads the
 * DB, so it's useful even on boxes where the LLM pod itself is disabled
 * (you still see quarantines and the empty-but-honest totals).
 */
export function startDigestCron() {
  if (digestTask) return digestTask;
  // A typo'd MAINT_DIGEST_CRON must not abort server boot — validate first,
  // and still guard the schedule call. Worst case the digest just doesn't
  // run; everything else (probes, triage, repair) is unaffected.
  if (!cron.validate(DIGEST_CRON)) {
    console.warn(`[maint] invalid MAINT_DIGEST_CRON "${DIGEST_CRON}" — daily digest disabled`);
    return null;
  }
  try {
    digestTask = cron.schedule(DIGEST_CRON, logDailyDigest);
    console.log(`[maint] daily digest scheduled (${DIGEST_CRON})`);
  } catch (err) {
    console.warn(`[maint] daily digest schedule failed: ${err?.message}`);
    digestTask = null;
  }
  return digestTask;
}

export function stopDigestCron() {
  if (digestTask) {
    digestTask.stop();
    digestTask = null;
  }
}
