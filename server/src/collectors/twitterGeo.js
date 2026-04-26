// server/src/collectors/twitterGeo.js
import { fetchJson } from './_liveHelpers.js';
import db from '../utils/database.js';

const BEARER_TOKEN = process.env.TWITTER_BEARER_TOKEN || '';

const MASTODON_INSTANCES = [
  'https://mstdn.jp',
  'https://pawoo.net',
  'https://mastodon-japan.net',
  'https://fedibird.com',
];

const JAPAN_PLACES = [
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
  { name: '道頓堀', lat: 34.6687, lon: 135.5013 },
  { name: '心斎橋', lat: 34.6748, lon: 135.5012 },
  { name: '梅田', lat: 34.7055, lon: 135.4983 },
  { name: '難波', lat: 34.6627, lon: 135.5010 },
  { name: '天王寺', lat: 34.6468, lon: 135.5135 },
  { name: '河原町', lat: 35.0040, lon: 135.7693 },
  { name: '祇園', lat: 34.9986, lon: 135.7747 },
  { name: '嵐山', lat: 35.0170, lon: 135.6713 },
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

const stmtUpsertPost = db.prepare(`
  INSERT INTO social_posts
    (post_uid, platform, author, text, title, url, media_urls, language,
     posted_at, lat, lon, geo_source, properties)
  VALUES
    (@post_uid, @platform, @author, @text, @title, @url, @media_urls, @language,
     @posted_at, @lat, @lon, @geo_source, @properties)
  ON CONFLICT(post_uid) DO UPDATE SET
    text = excluded.text,
    title = excluded.title,
    url = excluded.url,
    media_urls = excluded.media_urls,
    lat = COALESCE(social_posts.lat, excluded.lat),
    lon = COALESCE(social_posts.lon, excluded.lon),
    geo_source = COALESCE(social_posts.geo_source, excluded.geo_source)
`);

const stmtSelectGeocoded = db.prepare(`
  SELECT post_uid, platform, author, text, url, lat, lon, geo_source,
         llm_place_name, fetched_at, properties
  FROM social_posts
  WHERE platform IN ('twitter', 'mastodon')
    AND lat IS NOT NULL AND lon IS NOT NULL
  ORDER BY fetched_at DESC
  LIMIT 5000
`);

async function tryTwitterAPI() {
  if (!BEARER_TOKEN) return false;
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 8000);
  try {
    const url = 'https://api.twitter.com/2/tweets/search/recent?query=place_country:JP has:geo&tweet.fields=geo,created_at,public_metrics&max_results=100';
    const res = await fetch(url, {
      headers: { Authorization: `Bearer ${BEARER_TOKEN}` },
      signal: controller.signal,
    });
    clearTimeout(timeout);
    if (!res.ok) return false;
    const data = await res.json();
    if (!Array.isArray(data?.data)) return false;
    for (const tweet of data.data) {
      const coords = tweet.geo?.coordinates?.coordinates;
      const username = tweet.author?.username || tweet.username || null;
      const tweetUrl = username
        ? `https://twitter.com/${username}/status/${tweet.id}`
        : `https://twitter.com/i/web/status/${tweet.id}`;
      const hasGeo = Array.isArray(coords) && coords.length === 2;
      stmtUpsertPost.run({
        post_uid: `TW_${tweet.id}`,
        platform: 'twitter',
        author: username,
        text: tweet.text,
        title: null,
        url: tweetUrl,
        media_urls: null,
        language: tweet.lang || null,
        posted_at: tweet.created_at || null,
        lat: hasGeo ? coords[1] : null,
        lon: hasGeo ? coords[0] : null,
        geo_source: hasGeo ? 'native_geo' : null,
        properties: JSON.stringify({
          likes: tweet.public_metrics?.like_count || 0,
          retweets: tweet.public_metrics?.retweet_count || 0,
        }),
      });
    }
    return true;
  } catch {
    clearTimeout(timeout);
    return false;
  }
}

async function tryMastodonPublic() {
  let any = false;
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
      const mediaUrls = (p.media_attachments || [])
        .filter((m) => m.type === 'image' && m.url)
        .map((m) => m.url);
      stmtUpsertPost.run({
        post_uid: `MAST_${instance.replace('https://', '')}_${p.id}`,
        platform: 'mastodon',
        author: p.account?.acct || p.account?.username || null,
        text: body.slice(0, 1000),
        title: null,
        url: p.url || null,
        media_urls: mediaUrls.length ? JSON.stringify(mediaUrls) : null,
        language: p.language || 'ja',
        posted_at: p.created_at || null,
        lat: place ? place.lat : null,
        lon: place ? place.lon : null,
        geo_source: place ? 'place_match' : null,
        properties: JSON.stringify({
          instance: instance.replace('https://', ''),
          area: place?.name || null,
          favourites: p.favourites_count || 0,
        }),
      });
      any = true;
    }
  }
  return any;
}

export default async function collectTwitterGeo() {
  const twitterRan = await tryTwitterAPI();
  const mastoRan = await tryMastodonPublic();
  const liveSource = twitterRan ? 'twitter_api' : (mastoRan ? 'mastodon_public_api' : null);

  const rows = stmtSelectGeocoded.all();
  const features = rows.map((r) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [r.lon, r.lat] },
    properties: {
      id: r.post_uid,
      platform: r.platform,
      username: r.author,
      text: r.text?.slice(0, 280) || null,
      url: r.url,
      timestamp: r.fetched_at,
      area: r.llm_place_name,
      source: r.geo_source === 'llm_gsi' ? `${r.platform}+llm` : `${r.platform}_${r.geo_source}`,
    },
  }));

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'twitter_geo',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: features.length > 0,
      live_source: features.length > 0 ? liveSource : null,
      description: 'Geotagged social posts from Japan — Twitter/X API + Mastodon public timelines (live only)',
    },
    metadata: {},
  };
}
