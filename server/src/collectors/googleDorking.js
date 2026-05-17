/**
 * Google dorking for JP-relevant exposure discovery (google-dorking).
 *
 * Needs a search API key. Two supported providers, resolved via envFor:
 *   - Google Programmable Search (Custom Search JSON API):
 *       GOOGLE_CSE_KEY  + GOOGLE_CSE_CX
 *   - SerpAPI (Google engine):
 *       SERPAPI_KEY
 * Without any key the collector returns an honest empty result. We never
 * scrape google.com directly (ToS / blocking) and never fabricate hits.
 *
 * Endpoints:
 *   https://www.googleapis.com/customsearch/v1   (verified, keyed)
 *   https://serpapi.com/search.json              (verified, keyed)
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

const SOURCE_ID = 'google-dorking';

// JP-relevant OSINT discovery dorks: exposed indices / configs / docs on
// Japanese (.jp / .go.jp / .co.jp) infrastructure. Public-data discovery only.
const DORKS = [
  'site:go.jp filetype:xlsx',
  'site:co.jp intitle:"index of" "backup"',
  'site:lg.jp filetype:pdf "個人情報"',
  'site:jp inurl:admin intitle:login',
  'site:ac.jp filetype:env OR filetype:sql',
];

function envelope(items, source) {
  return {
    kind: 'intel',
    items,
    meta: {
      fetchedAt: new Date().toISOString(),
      recordCount: items.length,
      source,
      description: 'Google dork search results over JP infrastructure (requires search API key)',
    },
  };
}

async function viaGoogleCse(key, cx) {
  const items = [];
  for (const q of DORKS) {
    const url = 'https://www.googleapis.com/customsearch/v1'
      + `?key=${encodeURIComponent(key)}&cx=${encodeURIComponent(cx)}`
      + `&num=10&q=${encodeURIComponent(q)}`;
    const data = await fetchJson(url, { timeoutMs: 15000 });
    for (const r of data?.items || []) {
      items.push({
        uid: `${SOURCE_ID}|${r.link}`,
        title: r.title || r.link,
        summary: r.snippet || null,
        body: r.snippet || null,
        link: r.link,
        author: r.displayLink || null,
        language: 'ja',
        published_at: null,
        tags: ['google-dork', 'cse', `q:${q}`],
        properties: { dork: q, display_link: r.displayLink || null, source: 'google_cse_api' },
      });
    }
  }
  return items;
}

async function viaSerpApi(key) {
  const items = [];
  for (const q of DORKS) {
    const url = 'https://serpapi.com/search.json'
      + `?engine=google&gl=jp&hl=ja&num=10&api_key=${encodeURIComponent(key)}`
      + `&q=${encodeURIComponent(q)}`;
    const data = await fetchJson(url, { timeoutMs: 20000 });
    for (const r of data?.organic_results || []) {
      items.push({
        uid: `${SOURCE_ID}|${r.link}`,
        title: r.title || r.link,
        summary: r.snippet || null,
        body: r.snippet || null,
        link: r.link,
        author: r.displayed_link || null,
        language: 'ja',
        published_at: r.date || null,
        tags: ['google-dork', 'serpapi', `q:${q}`],
        properties: { dork: q, position: r.position ?? null, source: 'serpapi' },
      });
    }
  }
  return items;
}

export default async function collectGoogleDorking() {
  const cseKey = envFor('GOOGLE_CSE_KEY');
  const cseCx = envFor('GOOGLE_CSE_CX');
  const serpKey = envFor('SERPAPI_KEY');

  if (!((cseKey && cseCx) || serpKey)) return envelope([], 'google_dorking_no_key');

  try {
    const items = cseKey && cseCx
      ? await viaGoogleCse(cseKey, cseCx)
      : await viaSerpApi(serpKey);
    if (!items.length) return envelope([], 'google_dorking_unavailable');
    return envelope(items, cseKey && cseCx ? 'google_cse_api' : 'serpapi');
  } catch {
    return envelope([], 'google_dorking_unavailable');
  }
}
