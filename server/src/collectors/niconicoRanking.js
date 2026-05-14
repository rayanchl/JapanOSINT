/**
 * Niconico — content ranking (live + video).
 *
 * Niconico's nvapi is geofenced to JP IPs. We try a few endpoints and
 * gracefully degrade. Free, no auth.
 *
 * Endpoints:
 *   GET https://nvapi.nicovideo.jp/v1/ranking/teiban/genre/all   (overall)
 *   GET https://live.nicovideo.jp/api/v1/ranking?ranking=…       (live)
 *
 * Persistence model mirrors `twitterGeo.js`: every fetched item is upserted
 * into `social_posts` (platform='niconico') with lat/lon NULL. The shared
 * LLM enricher (`enrichSocialGeocode`) later infers a place from
 * title/description and fills lat/lon. The endpoint returns *only* the
 * geocoded subset — empty FeatureCollection until enrichment lands a row.
 * No fake placeholder coords.
 */

import db from '../utils/database.js';
import { upsertPosts } from '../utils/socialPostsStore.js';

const URL_OVERALL = 'https://nvapi.nicovideo.jp/v1/ranking/teiban/genre/all?term=24h';
const URL_LIVE = 'https://live.nicovideo.jp/api/v1/ranking?ranking=most_active&term=last24h&page=1&pageSize=20';
const TIMEOUT_MS = 12000;

// Post-cutover: niconico posts in intel_items under sub_source_id='niconico'.
const stmtSelectGeocoded = db.prepare(`
  SELECT
    substr(uid, instr(uid, '|') + 1)                AS post_uid,
    COALESCE(sub_source_id, source_id)              AS platform,
    author,
    body                                            AS text,
    title,
    link                                            AS url,
    lat, lon,
    geom_source                                     AS geo_source,
    json_extract(properties, '$.llm_place_name')    AS llm_place_name,
    fetched_at,
    properties
    FROM intel_items
   WHERE record_type = 'post'
     AND sub_source_id = 'niconico'
     AND lat IS NOT NULL AND lon IS NOT NULL
   ORDER BY fetched_at DESC
   LIMIT 5000
`);

async function tryFetch(url, headers) {
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(url, { signal: ctrl.signal, headers });
    clearTimeout(t);
    if (!res.ok) return { err: `HTTP ${res.status}` };
    return await res.json();
  } catch (err) { return { err: err?.message || 'fetch_failed' }; }
}

function parseIso(s) {
  if (!s) return null;
  const d = new Date(s);
  return Number.isNaN(d.getTime()) ? null : d.toISOString();
}

function safeJson(s) {
  if (!s) return {};
  try { return JSON.parse(s); } catch { return {}; }
}

function videoToPost(it, i) {
  if (!it?.id || !it?.title) return null;
  const thumb = it?.thumbnail?.listingUrl || it?.thumbnail?.url || it?.thumbnail?.largeUrl || null;
  return {
    post_uid: `NICO_VIDEO_${it.id}`,
    platform: 'niconico',
    author: it?.owner?.name || null,
    text: it.shortDescription || null,
    title: it.title,
    url: `https://www.nicovideo.jp/watch/${it.id}`,
    media_urls: thumb ? JSON.stringify([thumb]) : null,
    language: 'ja',
    posted_at: parseIso(it.registeredAt),
    lat: null,
    lon: null,
    geo_source: null,
    properties: JSON.stringify({
      kind: 'video',
      rank: i + 1,
      view_count: it?.count?.view ?? null,
      comment_count: it?.count?.comment ?? null,
      mylist_count: it?.count?.mylist ?? null,
      like_count: it?.count?.like ?? null,
      duration_s: it.duration ?? null,
      thumbnail_url: thumb,
      owner_id: it?.owner?.id ?? null,
    }),
  };
}

function liveToPost(p, i) {
  if (!p?.id || !p?.title) return null;
  const thumb = p?.thumbnail?.large || p?.thumbnail?.middle || p?.thumbnail?.small || null;
  return {
    post_uid: `NICO_LIVE_${p.id}`,
    platform: 'niconico',
    author: p?.programProvider?.name || p?.communityName || p?.channelName || null,
    text: null,
    title: p.title,
    url: `https://live.nicovideo.jp/watch/${p.id}`,
    media_urls: thumb ? JSON.stringify([thumb]) : null,
    language: 'ja',
    posted_at: parseIso(p.beginAt),
    lat: null,
    lon: null,
    geo_source: null,
    properties: JSON.stringify({
      kind: 'live',
      rank: i + 1,
      viewer_count: p?.statistics?.viewers ?? p?.viewers ?? null,
      comment_count: p?.statistics?.comments ?? p?.comments ?? null,
      timeshift_reservations: p?.statistics?.timeshiftReservations ?? null,
      end_at: parseIso(p.endAt),
      provider_type: p?.providerType || null,
      thumbnail_url: thumb,
    }),
  };
}

function rowToFeature(row) {
  const props = safeJson(row.properties);
  return {
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [row.lon, row.lat] },
    properties: {
      id: row.post_uid,
      platform: row.platform,
      kind: props.kind || null,
      rank: props.rank ?? null,
      username: row.author,
      title: row.title,
      text: row.text?.slice(0, 280) || null,
      url: row.url,
      thumbnail_url: props.thumbnail_url || null,
      view_count: props.view_count ?? null,
      viewer_count: props.viewer_count ?? null,
      comment_count: props.comment_count ?? null,
      duration_s: props.duration_s ?? null,
      timestamp: row.fetched_at,
      area: row.llm_place_name,
      source: row.geo_source === 'llm_gsi' ? 'niconico+llm' : `niconico_${row.geo_source}`,
    },
  };
}

export default async function collectNiconicoRanking() {
  const headers = {
    accept: 'application/json',
    'x-frontend-id': '6',
    'x-frontend-version': '0',
    'user-agent': 'japanosint-collector',
  };

  const [overall, live] = await Promise.all([
    tryFetch(URL_OVERALL, headers),
    tryFetch(URL_LIVE, headers),
  ]);

  const videoItems = (overall?.data?.items || []).slice(0, 30);
  const liveItems = (live?.data?.programs || live?.programs || []).slice(0, 30);

  const batch = [
    ...videoItems.map(videoToPost),
    ...liveItems.map(liveToPost),
  ].filter(Boolean);

  if (batch.length > 0) {
    try {
      await upsertPosts(batch);
    } catch (err) {
      console.warn('[niconicoRanking] upsertPosts failed:', err?.message);
    }
  }

  const rows = stmtSelectGeocoded.all();
  const features = rows.map(rowToFeature);

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'niconico_ranking',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: features.length > 0,
      live_source: features.length > 0 ? 'niconico_ranking' : null,
      ingested: batch.length,
      overall_err: overall?.err || null,
      live_err: live?.err || null,
      description: 'Niconico — overall video + most-active live ranking; geocoded via LLM (JP-IP only)',
    },
    metadata: {},
  };
}
