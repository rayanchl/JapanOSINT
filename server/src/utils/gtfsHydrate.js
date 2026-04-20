// Download a GTFS operator's feeds using the authoritative Shimada catalogue
// (gtfs_feeds table), run ingestFeedZip on each, and mark the operator
// hydrated. Shared between the HTTP route and the bulk job.
import db from './database.js';
import { ingestFeedZip } from './gtfsIngest.js';
import {
  isOperatorHydrated,
  markOperatorHydrated,
  getAgencyFeeds,
} from './gtfsStore.js';

const USER_AGENT = 'japan-osint/1.0 (+https://github.com/)';
const FETCH_TIMEOUT_MS = 60_000;

// One-flight guard so concurrent HTTP and bulk-job calls for the same orgId
// don't both ingest.
const inflightHydrate = new Map();

/**
 * Fetch with timeout and redirect-following. Returns an ArrayBuffer on 2xx,
 * null otherwise. Logs the reason for skips so the bulk hydrate report is
 * meaningful.
 */
async function fetchFeedZip(url, label) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), FETCH_TIMEOUT_MS);
  try {
    const res = await fetch(url, {
      headers: { 'User-Agent': USER_AGENT, 'Accept': 'application/zip, */*' },
      redirect: 'follow',
      signal: controller.signal,
    });
    if (!res.ok) {
      console.warn(`[gtfsHydrate] ${label} HTTP ${res.status}`);
      return null;
    }
    // Cheap sniff: zip magic is 'PK\x03\x04'. If the body starts with '<' it's
    // almost certainly an HTML landing page (GitHub release list, etc).
    const buf = await res.arrayBuffer();
    const head = new Uint8Array(buf.slice(0, 4));
    if (!(head[0] === 0x50 && head[1] === 0x4b && (head[2] === 0x03 || head[2] === 0x05 || head[2] === 0x07))) {
      console.warn(`[gtfsHydrate] ${label} response is not a zip (first 4 bytes: ${Array.from(head).map(b => b.toString(16)).join(' ')})`);
      return null;
    }
    return buf;
  } catch (err) {
    console.warn(`[gtfsHydrate] ${label} fetch failed: ${err?.message}`);
    return null;
  } finally {
    clearTimeout(timer);
  }
}

/**
 * Hydrate one operator (agency id, e.g. 'a01001'). Default behaviour: skip
 * if `hydrated_at` is already set. Pass `{ force: true }` to re-ingest.
 *
 * Returns { cached: true } or
 *         { cached: false, feedIds, counts, skipped }.
 */
export async function hydrateOperator(orgId, { force = false } = {}) {
  if (!force && isOperatorHydrated(orgId)) return { cached: true };
  const existing = inflightHydrate.get(orgId);
  if (existing) return existing;

  const p = (async () => {
    const feeds = getAgencyFeeds(orgId).filter(
      (f) => f.fixed_current_url && f.api_key_required === 0,
    );
    const feedIds = [];
    let skipped = 0;
    const totals = { routes: 0, trips: 0, stop_times: 0, shapes: 0, calendar: 0 };

    for (const f of feeds) {
      const label = `${orgId}/${f.feed_id}`;
      const buf = await fetchFeedZip(f.fixed_current_url, label);
      if (!buf) { skipped++; continue; }
      try {
        const c = ingestFeedZip(orgId, f.feed_id, buf);
        for (const k of Object.keys(totals)) {
          if (typeof c[k] === 'number') totals[k] += c[k];
        }
        feedIds.push(f.feed_id);
      } catch (err) {
        // AdmZip throws 'Invalid or unsupported zip format.' for HTML masquerading as zip.
        console.warn(`[gtfsHydrate] ${label} ingest failed: ${err?.message}`);
        skipped++;
      }
    }

    // stop_count = distinct stops observed in gtfs_stop_times for this org.
    let stopCount = 0;
    try {
      const row = db.prepare(
        'SELECT COUNT(DISTINCT stop_id) AS c FROM gtfs_stop_times WHERE org_id = ?',
      ).get(orgId);
      stopCount = row?.c || 0;
    } catch { /* leave 0 */ }

    markOperatorHydrated(orgId, orgId, feedIds, { stops: stopCount, trips: totals.trips });
    return { cached: false, feedIds, counts: totals, skipped };
  })();

  inflightHydrate.set(orgId, p);
  try { return await p; }
  finally { inflightHydrate.delete(orgId); }
}
