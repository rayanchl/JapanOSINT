// Nationwide GTFS hydrate: iterates every operator on gtfs-data.jp and
// hydrates each serially. Skips operators hydrated within `fresherThanDays`.
// Persists per-operator state via markOperatorHydrated so a restart resumes
// from the first operator whose hydrated_at is still NULL or stale.

import db from './database.js';
import { hydrateOperator } from './gtfsHydrate.js';
import { listUpstreamOperatorIds, listStaleOperatorIds } from './gtfsStore.js';

let _inflight = null;

function ensureOperatorRow(orgId) {
  // Insert a placeholder row so the stale-list query surfaces brand-new
  // operators. Does nothing if the row already exists.
  db.prepare(`
    INSERT OR IGNORE INTO gtfs_operators
      (org_id, org_name, hydrated_at, feed_ids, stop_count, trip_count)
    VALUES (?, ?, NULL, '[]', 0, 0)
  `).run(orgId, orgId);
}

export async function runBulkHydrate({ fresherThanDays = 7 } = {}) {
  if (_inflight) return _inflight;
  _inflight = (async () => {
    const started = Date.now();
    console.log('[gtfsBulkHydrate] discovering upstream operators…');
    let upstream = [];
    try {
      upstream = await listUpstreamOperatorIds();
    } catch (err) {
      console.error('[gtfsBulkHydrate] catalogue fetch failed:', err?.message);
      return { ok: 0, fail: 0, total: 0 };
    }
    for (const orgId of upstream) ensureOperatorRow(orgId);

    const stale = listStaleOperatorIds(fresherThanDays);
    // Only touch operators still present in the upstream catalogue (drops
    // decommissioned operators from the run).
    const upstreamSet = new Set(upstream);
    const queue = stale.filter((id) => upstreamSet.has(id));
    console.log(
      `[gtfsBulkHydrate] ${queue.length} operators to hydrate (of ${upstream.length} upstream)`,
    );

    let ok = 0, fail = 0;
    for (let i = 0; i < queue.length; i++) {
      const orgId = queue[i];
      try {
        await hydrateOperator(orgId, { force: true });
        ok++;
      } catch (err) {
        fail++;
        console.error(`[gtfsBulkHydrate] ${orgId} failed: ${err?.message}`);
      }
      if (i % 10 === 0 || i === queue.length - 1) {
        const elapsed = ((Date.now() - started) / 1000).toFixed(1);
        console.log(
          `[gtfsBulkHydrate] ${i + 1}/${queue.length} — ok:${ok} fail:${fail} elapsed:${elapsed}s`,
        );
      }
    }
    const totalMin = ((Date.now() - started) / 1000 / 60).toFixed(1);
    console.log(
      `[gtfsBulkHydrate] DONE — ok:${ok} fail:${fail} in ${totalMin} min`,
    );
    return { ok, fail, total: queue.length };
  })().finally(() => { _inflight = null; });
  return _inflight;
}

export function isBulkHydrateInFlight() {
  return _inflight !== null;
}
