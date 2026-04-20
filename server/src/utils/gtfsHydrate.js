// Download a GTFS-JP operator's feeds, run ingestFeedZip on each, and mark
// the operator hydrated. Shared between the HTTP route and the bulk job.
import db from './database.js';
import { ingestFeedZip } from './gtfsIngest.js';
import { isOperatorHydrated, markOperatorHydrated } from './gtfsStore.js';

const GTFS_API = 'https://api.gtfs-data.jp/v2';

// One-flight guard so concurrent HTTP and bulk-job calls for the same orgId
// don't both ingest.
const inflightHydrate = new Map();

/**
 * Hydrate one operator. Default: skip when already hydrated (cached:true).
 * Pass `{ force: true }` to re-ingest even if hydrated_at is set.
 *
 * Returns { cached:true } or { cached:false, feedIds, counts }.
 */
export async function hydrateOperator(orgId, { force = false } = {}) {
  if (!force && isOperatorHydrated(orgId)) return { cached: true };
  const existing = inflightHydrate.get(orgId);
  if (existing) return existing;

  const p = (async () => {
    const feedsRes = await fetch(`${GTFS_API}/organizations/${orgId}/feeds`);
    if (!feedsRes.ok) throw new Error(`feed list HTTP ${feedsRes.status}`);
    const body = await feedsRes.json();
    const feeds = Array.isArray(body?.body) ? body.body : [];
    const feedIds = [];
    const totals = { routes: 0, trips: 0, stop_times: 0, shapes: 0, calendar: 0 };
    for (const f of feeds) {
      const feedId = f.feed_id || f.id;
      if (!feedId) continue;
      const zipRes = await fetch(
        `${GTFS_API}/organizations/${orgId}/feeds/${feedId}/files/archive.zip`,
      );
      if (!zipRes.ok) continue;
      const buf = await zipRes.arrayBuffer();
      try {
        const c = ingestFeedZip(orgId, feedId, buf);
        for (const k of Object.keys(totals)) {
          if (typeof c[k] === 'number') totals[k] += c[k];
        }
        feedIds.push(feedId);
      } catch (err) {
        console.error(`[gtfsHydrate] ${orgId}/${feedId} ingest failed:`, err?.message);
      }
    }
    let stopCount = 0;
    try {
      const row = db.prepare(
        'SELECT COUNT(DISTINCT stop_id) AS c FROM gtfs_stop_times WHERE org_id = ?',
      ).get(orgId);
      stopCount = row?.c || 0;
    } catch { /* leave 0 */ }
    markOperatorHydrated(orgId, orgId, feedIds, { stops: stopCount, trips: totals.trips });
    return { cached: false, feedIds, counts: totals };
  })();

  inflightHydrate.set(orgId, p);
  try { return await p; }
  finally { inflightHydrate.delete(orgId); }
}
