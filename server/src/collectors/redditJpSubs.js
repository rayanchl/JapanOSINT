/**
 * Reddit JP-relevant subreddits (r/japan, r/japanlife, r/Tokyo, r/newsokur).
 * https://www.reddit.com/r/{name}/.json
 *
 * Free, anonymous, no key. Posts often mention specific places, events,
 * news. Surfaces English-language OSINT corpora for JP.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchJson } from './_liveHelpers.js';

const SOURCE_ID = 'reddit-jp-subs';
const SUBS = ['japan', 'japanlife', 'Tokyo', 'newsokur', 'JapanFinance'];
const HEADERS = { 'User-Agent': 'JapanOSINT/1.0 (research)' };

export default async function collectRedditJpSubs() {
  const items = [];
  let anyLive = false;
  for (const sub of SUBS) {
    let posts = [];
    try {
      const r = await fetchJson(`https://www.reddit.com/r/${sub}/new.json?limit=25`, { timeoutMs: 10000, headers: HEADERS });
      posts = (r?.data?.children || []).map((c) => c.data);
      if (posts.length > 0) anyLive = true;
    } catch { /* ignore */ }
    for (const p of posts.slice(0, 25)) {
      items.push({
        uid: intelUid(SOURCE_ID, p.id),
        title: p.title || `r/${sub} post`,
        summary: (p.selftext || '').slice(0, 240) || null,
        link: p.url || `https://www.reddit.com${p.permalink}`,
        language: 'en',
        published_at: p.created_utc ? new Date(p.created_utc * 1000).toISOString() : null,
        tags: ['reddit', sub, 'social'],
        properties: { subreddit: sub, score: p.score, comments: p.num_comments, author: p.author },
      });
    }
  }
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: anyLive,
    description: 'Reddit JP-relevant subreddits (japan, japanlife, Tokyo, newsokur, JapanFinance)',
  });
}
