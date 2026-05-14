/**
 * Social-posts storage — thin wrapper around intel_items (the polymorphic
 * master) keyed by `<platform_source_id>|<post_uid>`. The legacy
 * social_posts typed table + social_posts_fts mirror were retired in the
 * Phase B cutover; this module preserves the API its callers expect
 * (twitterGeo / niconicoRanking writes, llmEnricher geocode applies).
 */

import db from './database.js';
import {
  applyGeocodeToMaster, applyGeocodeFailToMaster,
  upsertItems as masterUpsertItems,
} from './intelStore.js';

// Mirror-side source_id for a given platform. Mastodon platform here means
// Misskey (uses Mastodon-compatible API); the live mirror writes its rows
// under 'misskey-timeline'. Other platforms use the platform string itself
// where it matches a registered collector, or fall back to 'social-posts'.
function platformToSourceId(platform) {
  if (platform === 'mastodon') return 'misskey-timeline';
  return platform || 'social-posts';
}

function safeJson(s) { try { return JSON.parse(s); } catch { return {}; } }

/**
 * Upsert one or many social posts. Master-only writes. Each post lands under
 * `<platformToSourceId(platform)>|<post_uid>` so backfilled and live rows
 * collide on the same uid (idempotent).
 *
 * Returns the number of rows written.
 */
export async function upsertPosts(posts) {
  if (!Array.isArray(posts) || posts.length === 0) return 0;
  const buckets = new Map();
  for (const p of posts) {
    if (!p?.post_uid) continue;
    const sourceId = platformToSourceId(p.platform);
    const lat = Number.isFinite(p.lat) ? p.lat : null;
    const lon = Number.isFinite(p.lon) ? p.lon : null;
    const item = {
      uid:          `${sourceId}|${p.post_uid}`,
      title:        p.title ?? null,
      body:         p.text ?? null,
      link:         p.url ?? null,
      author:       p.author ?? null,
      language:     p.language ?? null,
      published_at: p.posted_at ?? null,
      lat,
      lon,
      geomSource:   p.geo_source ?? null,
      recordType:   'post',
      subSourceId:  p.platform,
      properties:   {
        ...(typeof p.properties === 'string' ? safeJson(p.properties) : (p.properties || {})),
        platform:   p.platform,
        post_uid:   p.post_uid,
        media_urls: p.media_urls ?? null,
      },
      geometry: (lat != null && lon != null)
        ? { type: 'Point', coordinates: [lon, lat] }
        : null,
    };
    if (!buckets.has(sourceId)) buckets.set(sourceId, []);
    buckets.get(sourceId).push(item);
  }
  let total = 0;
  for (const [sourceId, items] of buckets) {
    const r = await masterUpsertItems(items, sourceId);
    total += r?.count ?? 0;
  }
  return total;
}

export async function upsertPost(post) {
  return upsertPosts([post]);
}

// Look up the master uid for a given post_uid by suffix match. Used by the
// LLM enricher's apply* paths which only know the post_uid. Restricted to
// record_type='post' so we don't accidentally match a station uid.
const stmtMasterPostByUidSuffix = db.prepare(`
  SELECT uid FROM intel_items
   WHERE record_type = 'post'
     AND uid LIKE '%|' || ?
   LIMIT 1
`);

/** Mark a post as successfully LLM-geocoded. Master-only write. */
export async function applyGeocodeOk({ post_uid, lat, lon }) {
  const row = stmtMasterPostByUidSuffix.get(String(post_uid));
  if (!row) return 0;
  return applyGeocodeToMaster({ uid: row.uid, lat, lon });
}

/** Record an LLM-geocoding failure. */
export async function applyGeocodeFail({ post_uid }) {
  const row = stmtMasterPostByUidSuffix.get(String(post_uid));
  if (!row) return 0;
  return applyGeocodeFailToMaster({ uid: row.uid });
}
