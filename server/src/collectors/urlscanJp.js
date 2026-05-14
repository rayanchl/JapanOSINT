/**
 * URLscan.io — recent public scans matching JP-relevant domains.
 *
 * Use cases:
 *   - phishing kits impersonating JP banks (mizuho, mufg, smbc, japanpost,
 *     rakuten, sbi, paypay, jcb, aeon)
 *   - exposed admin/login panels on .jp / .co.jp
 *   - watering-hole pages targeting JP audiences
 *
 * Auth: URLSCAN_API_KEY optional (anonymous works at lower rate). Sign up:
 *   https://urlscan.io/user/signup
 *
 * Endpoint:
 *   GET https://urlscan.io/api/v1/search/?q=<lucene-query>&size=100
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'urlscan-jp';
const BASE = 'https://urlscan.io/api/v1/search/';
const TIMEOUT_MS = 15000;

// Lucene query: JP-located scans. Anonymous users cannot use leading-wildcard
// or regex queries, so we filter on `page.country:JP`. Authenticated callers
// (URLSCAN_API_KEY) can override with richer queries — see env_hint.
const DEFAULT_QUERY = process.env.URLSCAN_QUERY || 'page.country:JP';

export default async function collectUrlscanJp() {
  const params = new URLSearchParams({ q: DEFAULT_QUERY, size: '100' });
  const headers = { accept: 'application/json' };
  if (process.env.URLSCAN_API_KEY) headers['API-Key'] = process.env.URLSCAN_API_KEY;

  let items = [];
  let live = false;
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(`${BASE}?${params}`, { signal: ctrl.signal, headers });
    clearTimeout(timer);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const json = await res.json();
    const results = Array.isArray(json.results) ? json.results : [];
    items = results.map((r, i) => ({
      uid: intelUid(SOURCE_ID, r._id, `idx_${i}`),
      title: r.page?.url || r.page?.domain || `scan-${i}`,
      summary: [r.page?.domain, r.page?.country, r.page?.asnname].filter(Boolean).join(' · ') || null,
      link: r.result || (r._id ? `https://urlscan.io/result/${r._id}/` : null),
      language: 'en',
      published_at: r.task?.time || null,
      tags: ['urlscan', r.verdicts?.overall?.malicious ? 'malicious' : null, ...(Array.isArray(r.task?.tags) ? r.task.tags.slice(0, 5) : [])].filter(Boolean),
      properties: {
        uuid: r._id || null,
        url: r.page?.url || r.task?.url || null,
        domain: r.page?.domain || null,
        ip: r.page?.ip || null,
        asn: r.page?.asn || null,
        asn_name: r.page?.asnname || null,
        country: r.page?.country || null,
        city: r.page?.city || null,
        screenshot: r.screenshot || null,
        verdicts_malicious: r.verdicts?.overall?.malicious ?? null,
        verdicts_score: r.verdicts?.overall?.score ?? null,
      },
    }));
    live = items.length > 0;
  } catch (err) {
    console.warn('[urlscanJp] fetch failed:', err?.message);
  }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live,
    description: 'URLscan.io recent scans — JP-domain / JP-brand phishing focus',
    extraMeta: { query: DEFAULT_QUERY, env_hint: 'Set URLSCAN_API_KEY for higher rate limits' },
  });
}
