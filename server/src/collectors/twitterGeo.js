/**
 * Twitter/X Geo-coded Posts Collector
 *
 * Real posts only:
 *  - Twitter API v2 geo search (requires TWITTER_BEARER_TOKEN)
 *  - Mastodon public timelines from Japan-centric instances (fallback),
 *    geo-located by place-name match on real toot content.
 *
 * No synthetic/seed data. Returns an empty FeatureCollection when no live
 * posts are available.
 */

import { fetchJson } from './_liveHelpers.js';

const BEARER_TOKEN = process.env.TWITTER_BEARER_TOKEN || '';

// Mastodon public instances with Japanese user presence. These expose an
// authless public timeline API (`/api/v1/timelines/public`).
const MASTODON_INSTANCES = [
  'https://mstdn.jp',
  'https://pawoo.net',
  'https://mastodon-japan.net',
  'https://fedibird.com',
];

// Place-name → real coordinates lookup. Used only to geo-locate a real
// Mastodon post whose body literally mentions one of these Japanese places.
const JAPAN_PLACES = [
  // Tokyo
  { name: '渋谷', lat: 35.6595, lon: 139.7004 },
  { name: '新宿', lat: 35.6938, lon: 139.7036 },
  { name: '秋葉原', lat: 35.6984, lon: 139.7731 },
  { name: '原宿', lat: 35.6702, lon: 139.7035 },
  { name: '池袋', lat: 35.7295, lon: 139.7182 },
  { name: '六本木', lat: 35.6605, lon: 139.7292 },
  { name: '銀座', lat: 35.6717, lon: 139.7637 },
  { name: '浅草', lat: 35.7114, lon: 139.7966 },
  { name: 'お台場', lat: 35.6267, lon: 139.7752 },
  { name: '下北沢', lat: 35.6613, lon: 139.6680 },
  { name: '中目黒', lat: 35.6440, lon: 139.6988 },
  { name: '吉祥寺', lat: 35.7030, lon: 139.5795 },
  // Osaka
  { name: '道頓堀', lat: 34.6687, lon: 135.5013 },
  { name: '心斎橋', lat: 34.6748, lon: 135.5012 },
  { name: '梅田', lat: 34.7055, lon: 135.4983 },
  { name: '難波', lat: 34.6627, lon: 135.5010 },
  { name: '天王寺', lat: 34.6468, lon: 135.5135 },
  // Kyoto
  { name: '河原町', lat: 35.0040, lon: 135.7693 },
  { name: '祇園', lat: 34.9986, lon: 135.7747 },
  { name: '嵐山', lat: 35.0170, lon: 135.6713 },
  // Other major cities
  { name: '博多', lat: 33.5920, lon: 130.4080 },
  { name: '天神', lat: 33.5898, lon: 130.3987 },
  { name: '栄', lat: 35.1692, lon: 136.9084 },
  { name: '横浜駅', lat: 35.4660, lon: 139.6223 },
  { name: '三宮', lat: 34.6951, lon: 135.1979 },
  { name: '札幌駅', lat: 43.0687, lon: 141.3508 },
  { name: 'すすきの', lat: 43.0556, lon: 141.3530 },
  { name: '国際通り', lat: 26.3358, lon: 127.6862 },
  { name: '仙台駅', lat: 38.2601, lon: 140.8822 },
  { name: '広島駅', lat: 34.3978, lon: 132.4752 },
];

async function tryTwitterAPI() {
  if (!BEARER_TOKEN) return null;
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 8000);
  try {
    const url = 'https://api.twitter.com/2/tweets/search/recent?query=place_country:JP has:geo&tweet.fields=geo,created_at,public_metrics&max_results=100';
    const res = await fetch(url, {
      headers: { Authorization: `Bearer ${BEARER_TOKEN}` },
      signal: controller.signal,
    });
    clearTimeout(timeout);
    if (!res.ok) return null;
    const data = await res.json();
    if (!data.data) return null;
    return data.data
      .filter((t) => Array.isArray(t.geo?.coordinates?.coordinates))
      .map((tweet) => {
        const username = tweet.author?.username || tweet.username || null;
        const url = username
          ? `https://twitter.com/${username}/status/${tweet.id}`
          : `https://twitter.com/i/web/status/${tweet.id}`;
        return {
          type: 'Feature',
          geometry: {
            type: 'Point',
            coordinates: tweet.geo.coordinates.coordinates,
          },
          properties: {
            id: tweet.id,
            platform: 'twitter',
            username,
            text: tweet.text,
            url,
            likes: tweet.public_metrics?.like_count || 0,
            retweets: tweet.public_metrics?.retweet_count || 0,
            timestamp: tweet.created_at,
            source: 'twitter_api',
          },
        };
      });
  } catch {
    clearTimeout(timeout);
    return null;
  }
}

async function tryMastodonPublic() {
  const all = [];
  for (const instance of MASTODON_INSTANCES) {
    const posts = await fetchJson(
      `${instance}/api/v1/timelines/public?local=true&limit=40`,
      { timeoutMs: 7000 },
    );
    if (!Array.isArray(posts)) continue;
    for (const p of posts) {
      const body = (p.content || '').replace(/<[^>]*>/g, '');
      let place = null;
      for (const candidate of JAPAN_PLACES) {
        if (body.includes(candidate.name)) { place = candidate; break; }
      }
      if (!place) continue; // only keep posts we can geo-locate from real text
      all.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [place.lon, place.lat] },
        properties: {
          id: `MAST_${p.id}`,
          platform: 'mastodon',
          instance: instance.replace('https://', ''),
          username: p.account?.acct || p.account?.username,
          display_name: p.account?.display_name || null,
          text: body.slice(0, 280),
          url: p.url || null,
          likes: p.favourites_count || 0,
          retweets: p.reblogs_count || 0,
          replies: p.replies_count || 0,
          verified: !!p.account?.bot === false && p.account?.followers_count > 1000,
          has_media: (p.media_attachments || []).length > 0,
          media_type: p.media_attachments?.[0]?.type || null,
          language: p.language || 'ja',
          area: place.name,
          timestamp: p.created_at,
          source: 'mastodon_public_api',
        },
      });
    }
  }
  return all.length > 0 ? all : null;
}

export default async function collectTwitterGeo() {
  let features = await tryTwitterAPI();
  let liveSource = 'twitter_api';
  if (!features || features.length === 0) {
    features = await tryMastodonPublic();
    liveSource = features && features.length > 0 ? 'mastodon_public_api' : null;
  }
  const list = features || [];
  const live = list.length > 0;

  return {
    type: 'FeatureCollection',
    features: list,
    _meta: {
      source: 'twitter_geo',
      fetchedAt: new Date().toISOString(),
      recordCount: list.length,
      live,
      live_source: live ? liveSource : null,
      description: 'Geotagged social posts from Japan — Twitter/X API + Mastodon public timelines (live only)',
    },
    metadata: {},
  };
}
