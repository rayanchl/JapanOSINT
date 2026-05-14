/**
 * Mercari — Japanese consumer-to-consumer marketplace.
 *
 * No official public API. The site uses internal XHR endpoints. Rather
 * than scrape the full marketplace, this collector tracks the handful
 * of "trending category" slots exposed on the landing page.
 *
 * LEGAL NOTE: Mercari's ToS references external prohibited-acts
 * guidelines that likely disallow automated extraction at scale. Use
 * at low cadence (>= daily) and keep it to trending-only, not bulk
 * listing scrape.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchJson } from './_liveHelpers.js';

const SOURCE_ID = 'mercari-trending';
const BASE = 'https://jp.mercari.com';
const TIMEOUT_MS = 12000;

// Stable-ish internal endpoint returning trending keywords. If Mercari
// refactors, this collector degrades silently to an empty feed.
const TRENDING_PATH = '/v1/web/suggest/trends';

export default async function collectMercariTrending() {
  const data = await fetchJson(`${BASE}${TRENDING_PATH}`, {
    timeoutMs: TIMEOUT_MS,
    headers: { 'user-agent': 'JapanOSINT/1.0' },
  });
  let items = [];
  if (data) {
    const list = Array.isArray(data?.trends) ? data.trends : Array.isArray(data) ? data : [];
    items = list.slice(0, 50).map((it, i) => {
      const keyword = typeof it === 'string' ? it : (it.keyword || it.name || null);
      if (!keyword) return null;
      return {
        uid: intelUid(SOURCE_ID, keyword, `rank_${i + 1}`),
        title: keyword,
        summary: `Trending #${i + 1}`,
        link: `${BASE}/search?keyword=${encodeURIComponent(keyword)}`,
        language: 'ja',
        tags: ['mercari', 'trending', `rank:${i + 1}`],
        properties: { keyword, rank: i + 1 },
      };
    }).filter(Boolean);
  }
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: items.length > 0,
    description: 'Mercari trending search keywords (Japan consumer demand signal)',
  });
}
