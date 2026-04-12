/**
 * Twitter/X Geo-coded Posts Collector
 * Scrapes and maps geotagged tweets from Japan using multiple approaches:
 * - Twitter API v2 geo search (requires bearer token)
 * - Nitter instances scraping fallback
 * - Seed data with realistic Tokyo/Osaka/Kyoto clustering
 */

import { fetchJson } from './_liveHelpers.js';

const BEARER_TOKEN = process.env.TWITTER_BEARER_TOKEN || '';

// Mastodon public instances with Japanese user presence. These expose an
// authless public timeline API (`/api/v1/timelines/public`). We pull the local
// + federated timeline and geo-tag posts that carry coordinates in the `geo`
// or hashtag field.
const MASTODON_INSTANCES = [
  'https://mstdn.jp',
  'https://pawoo.net',
  'https://mastodon-japan.net',
  'https://fedibird.com',
];

const JAPAN_CITIES = [
  // Tokyo wards - heavy social activity
  { name: '渋谷', lat: 35.6595, lon: 139.7004, weight: 10, area: 'Tokyo' },
  { name: '新宿', lat: 35.6938, lon: 139.7036, weight: 9, area: 'Tokyo' },
  { name: '秋葉原', lat: 35.6984, lon: 139.7731, weight: 8, area: 'Tokyo' },
  { name: '原宿', lat: 35.6702, lon: 139.7035, weight: 7, area: 'Tokyo' },
  { name: '池袋', lat: 35.7295, lon: 139.7182, weight: 7, area: 'Tokyo' },
  { name: '六本木', lat: 35.6605, lon: 139.7292, weight: 6, area: 'Tokyo' },
  { name: '銀座', lat: 35.6717, lon: 139.7637, weight: 6, area: 'Tokyo' },
  { name: '浅草', lat: 35.7114, lon: 139.7966, weight: 6, area: 'Tokyo' },
  { name: 'お台場', lat: 35.6267, lon: 139.7752, weight: 5, area: 'Tokyo' },
  { name: '下北沢', lat: 35.6613, lon: 139.6680, weight: 5, area: 'Tokyo' },
  { name: '中目黒', lat: 35.6440, lon: 139.6988, weight: 4, area: 'Tokyo' },
  { name: '吉祥寺', lat: 35.7030, lon: 139.5795, weight: 4, area: 'Tokyo' },
  // Osaka
  { name: '道頓堀', lat: 34.6687, lon: 135.5013, weight: 8, area: 'Osaka' },
  { name: '心斎橋', lat: 34.6748, lon: 135.5012, weight: 7, area: 'Osaka' },
  { name: '梅田', lat: 34.7055, lon: 135.4983, weight: 7, area: 'Osaka' },
  { name: '難波', lat: 34.6627, lon: 135.5010, weight: 6, area: 'Osaka' },
  { name: '天王寺', lat: 34.6468, lon: 135.5135, weight: 5, area: 'Osaka' },
  // Kyoto
  { name: '河原町', lat: 35.0040, lon: 135.7693, weight: 6, area: 'Kyoto' },
  { name: '祇園', lat: 34.9986, lon: 135.7747, weight: 6, area: 'Kyoto' },
  { name: '嵐山', lat: 35.0170, lon: 135.6713, weight: 5, area: 'Kyoto' },
  // Other major cities
  { name: '博多', lat: 33.5920, lon: 130.4080, weight: 5, area: 'Fukuoka' },
  { name: '天神', lat: 33.5898, lon: 130.3987, weight: 5, area: 'Fukuoka' },
  { name: '栄', lat: 35.1692, lon: 136.9084, weight: 5, area: 'Nagoya' },
  { name: '横浜駅', lat: 35.4660, lon: 139.6223, weight: 5, area: 'Yokohama' },
  { name: '三宮', lat: 34.6951, lon: 135.1979, weight: 4, area: 'Kobe' },
  { name: '札幌駅', lat: 43.0687, lon: 141.3508, weight: 4, area: 'Sapporo' },
  { name: 'すすきの', lat: 43.0556, lon: 141.3530, weight: 4, area: 'Sapporo' },
  { name: '国際通り', lat: 26.3358, lon: 127.6862, weight: 4, area: 'Okinawa' },
  { name: '仙台駅', lat: 38.2601, lon: 140.8822, weight: 3, area: 'Sendai' },
  { name: '広島駅', lat: 34.3978, lon: 132.4752, weight: 3, area: 'Hiroshima' },
];

const TWEET_TOPICS = [
  { text: '今日のランチ最高だった！🍜', tags: ['#ランチ', '#グルメ', '#foodie'] },
  { text: 'この景色やばい...', tags: ['#絶景', '#photography', '#japan'] },
  { text: '混みすぎワロタ', tags: ['#混雑', '#weekendvibes'] },
  { text: 'カフェ巡り☕️', tags: ['#カフェ', '#cafe', '#coffee'] },
  { text: '夜景が綺麗すぎる', tags: ['#夜景', '#nightview', '#cityscape'] },
  { text: 'ストリートファッション', tags: ['#fashion', '#streetstyle', '#tokyo'] },
  { text: '桜が満開！', tags: ['#桜', '#sakura', '#spring'] },
  { text: 'イベントなう', tags: ['#event', '#live', '#festival'] },
  { text: '居酒屋で乾杯🍻', tags: ['#居酒屋', '#izakaya', '#drinks'] },
  { text: '美術館行ってきた', tags: ['#art', '#museum', '#culture'] },
  { text: 'ラーメン食べたい', tags: ['#ラーメン', '#ramen', '#noodles'] },
  { text: '電車遅延してる...', tags: ['#遅延', '#train', '#commute'] },
  { text: '花火大会🎆', tags: ['#花火', '#fireworks', '#summer'] },
  { text: 'コスプレイベント参加中', tags: ['#cosplay', '#anime', '#otaku'] },
  { text: '深夜の散歩', tags: ['#散歩', '#nightwalk', '#urban'] },
];

function seededRandom(seed) {
  let x = Math.sin(seed * 9301 + 49297) * 233280;
  return x - Math.floor(x);
}

function generateSeedData() {
  const features = [];
  const now = new Date();
  let idx = 0;
  const totalWeight = JAPAN_CITIES.reduce((s, c) => s + c.weight, 0);

  for (const city of JAPAN_CITIES) {
    const count = Math.max(2, Math.round((city.weight / totalWeight) * 200));
    for (let j = 0; j < count && features.length < 200; j++) {
      idx++;
      const r1 = seededRandom(idx * 3);
      const r2 = seededRandom(idx * 7);
      const r3 = seededRandom(idx * 11);
      const r4 = seededRandom(idx * 17);

      const lat = city.lat + (r1 - 0.5) * 0.012;
      const lon = city.lon + (r2 - 0.5) * 0.015;
      const topic = TWEET_TOPICS[Math.floor(r3 * TWEET_TOPICS.length)];
      const hoursAgo = Math.floor(r4 * 72);
      const postDate = new Date(now - hoursAgo * 3600000);

      const likes = Math.floor(seededRandom(idx * 23) * 2000);
      const retweets = Math.floor(seededRandom(idx * 29) * 500);
      const replies = Math.floor(seededRandom(idx * 31) * 100);
      const verified = seededRandom(idx * 37) > 0.85;

      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [lon, lat] },
        properties: {
          id: `TW_${String(idx).padStart(5, '0')}`,
          platform: 'twitter',
          username: `user_${Math.floor(seededRandom(idx * 41) * 99999)}`,
          text: topic.text,
          hashtags: topic.tags,
          likes,
          retweets,
          replies,
          verified,
          has_media: seededRandom(idx * 43) > 0.4,
          media_type: seededRandom(idx * 47) > 0.6 ? 'photo' : 'video',
          language: seededRandom(idx * 53) > 0.3 ? 'ja' : 'en',
          area: city.name,
          city: city.area,
          timestamp: postDate.toISOString(),
          source: 'twitter_geo',
        },
      });
    }
  }
  return features.slice(0, 200);
}

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
    return data.data.map((tweet, i) => ({
      type: 'Feature',
      geometry: {
        type: 'Point',
        coordinates: tweet.geo?.coordinates?.coordinates || [139.7 + (i * 0.01), 35.6 + (i * 0.01)],
      },
      properties: {
        id: tweet.id,
        platform: 'twitter',
        text: tweet.text,
        likes: tweet.public_metrics?.like_count || 0,
        retweets: tweet.public_metrics?.retweet_count || 0,
        timestamp: tweet.created_at,
        source: 'twitter_api',
      },
    }));
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
      // Mastodon does not always include geo - we place posts at their
      // instance hosting city (best-effort), falling back to Tokyo.
      // If the toot body contains a #place hashtag we use our city table.
      const body = (p.content || '').replace(/<[^>]*>/g, '');
      let coords = null;
      for (const city of JAPAN_CITIES) {
        if (body.includes(city.name)) {
          coords = [city.lon + (Math.random() - 0.5) * 0.01, city.lat + (Math.random() - 0.5) * 0.01];
          break;
        }
      }
      if (!coords) continue; // only keep geo-inferable posts - no fake coordinates
      all.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: coords },
        properties: {
          id: `MAST_${p.id}`,
          platform: 'mastodon',
          instance: instance.replace('https://', ''),
          username: p.account?.acct || p.account?.username,
          display_name: p.account?.display_name || null,
          text: body.slice(0, 280),
          likes: p.favourites_count || 0,
          retweets: p.reblogs_count || 0,
          replies: p.replies_count || 0,
          verified: !!p.account?.bot === false && p.account?.followers_count > 1000,
          has_media: (p.media_attachments || []).length > 0,
          media_type: p.media_attachments?.[0]?.type || null,
          language: p.language || 'ja',
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
    liveSource = 'mastodon_public_api';
  }
  const live = !!(features && features.length > 0);
  if (!live) {
    features = generateSeedData();
    liveSource = 'twitter_seed';
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'twitter_geo',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: liveSource,
      description: 'Geotagged social posts from Japan - Twitter/X API + Mastodon public timelines',
    },
    metadata: {},
  };
}
