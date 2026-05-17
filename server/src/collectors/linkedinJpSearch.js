/**
 * LinkedIn JP people/company search (linkedin-jp-search).
 *
 * Needs LINKEDIN_COOKIE (li_at session, via envFor) — LinkedIn's Voyager
 * search API requires an authenticated session and a CSRF token derived from
 * the JSESSIONID cookie. Non-spatial intel (public profile/company hits).
 * Without the cookie or on failure we return an honest empty result — never
 * fabricated profiles.
 *
 * Endpoint (unverified shape, requires session cookie + csrf; subject to
 * change): https://www.linkedin.com/voyager/api/search/blended
 * Credential var: LINKEDIN_COOKIE  (full cookie string incl. li_at + JSESSIONID)
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

const SOURCE_ID = 'linkedin-jp-search';
const QUERIES = ['security Japan', 'OSINT Tokyo', 'CISO 日本'];

function envelope(items, source) {
  return {
    kind: 'intel',
    items,
    meta: {
      fetchedAt: new Date().toISOString(),
      recordCount: items.length,
      source,
      description: 'LinkedIn people/company search for JP-relevant terms (requires LINKEDIN_COOKIE)',
    },
  };
}

// JSESSIONID value (quoted) doubles as the csrf-token for Voyager calls.
function csrfFromCookie(cookie) {
  const m = /JSESSIONID="?([^";]+)"?/.exec(cookie || '');
  return m ? m[1] : null;
}

export default async function collectLinkedinJpSearch() {
  const cookie = envFor('LINKEDIN_COOKIE');
  if (!cookie) return envelope([], 'linkedin_jp_search_no_key');
  const csrf = csrfFromCookie(cookie);
  if (!csrf) return envelope([], 'linkedin_jp_search_unavailable');

  const headers = {
    Cookie: cookie,
    'Csrf-Token': csrf,
    'X-RestLi-Protocol-Version': '2.0.0',
    Accept: 'application/json',
    Referer: 'https://www.linkedin.com/search/results/all/',
  };

  const items = [];
  const seen = new Set();
  let gotAny = false;

  for (const q of QUERIES) {
    let data = null;
    try {
      const url = 'https://www.linkedin.com/voyager/api/search/blended'
        + `?keywords=${encodeURIComponent(q)}&origin=GLOBAL_SEARCH_HEADER&count=20`;
      data = await fetchJson(url, { timeoutMs: 15000, headers });
    } catch { data = null; }
    if (!data) continue;
    gotAny = true;

    const elements = data?.data?.elements || data?.elements || [];
    for (const grp of elements) {
      for (const el of grp.elements || [grp]) {
        const urn = el.targetUrn || el.trackingUrn || el.objectUrn;
        const pub = el.publicIdentifier || el.navigationUrl;
        const id = pub || urn;
        if (!id || seen.has(id)) continue;
        seen.add(id);
        const title = el.title?.text || el.headline?.text || String(id);
        items.push({
          uid: `${SOURCE_ID}|${id}`,
          title,
          summary: el.subline?.text || el.headline?.text || null,
          body: el.summary?.text || null,
          link: el.navigationUrl
            || (el.publicIdentifier ? `https://www.linkedin.com/in/${el.publicIdentifier}` : null),
          author: title,
          language: null,
          published_at: null,
          tags: ['linkedin', 'profile', `q:${q}`],
          properties: { urn: urn || null, matched_query: q, source: 'linkedin_voyager' },
        });
      }
    }
  }

  if (!gotAny) return envelope([], 'linkedin_jp_search_unavailable');
  if (!items.length) return envelope([], 'linkedin_jp_search_unavailable');
  return envelope(items, 'linkedin_voyager');
}
