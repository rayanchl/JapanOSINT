/**
 * GrayhatWarfare — exposed S3 / GCS bucket index. Filter by JP-keyword to
 * surface misconfigured buckets owned by JP organisations.
 *
 * Auth: GRAYHAT_API_KEY (paid tier required for the API; free tier is web
 * UI only). https://buckets.grayhatwarfare.com/account
 *
 * Endpoint:
 *   GET https://buckets.grayhatwarfare.com/api/v2/buckets?keywords=<csv>&limit=100
 *   header: Authorization: Bearer <key>
 */

const BASE = 'https://buckets.grayhatwarfare.com/api/v2/buckets';
const TIMEOUT_MS = 15000;

const DEFAULT_KEYWORDS = (process.env.GRAYHAT_KEYWORDS || [
  'co.jp', 'go.jp', 'japan', 'tokyo',
  'rakuten', 'mizuho', 'mufg', 'smbc',
  'softbank', 'docomo', 'kddi', 'ntt',
].join(',')).split(',').map((s) => s.trim()).filter(Boolean);

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'grayhat-buckets';

export default async function collectGrayhatBuckets() {
  const key = process.env.GRAYHAT_API_KEY;
  if (!key) {
    return intelEnvelope({
      sourceId: SOURCE_ID,
      items: [],
      live: false,
      description: 'GrayhatWarfare exposed buckets — JP keyword filter',
      extraMeta: { env_hint: 'Set GRAYHAT_API_KEY (paid tier required: https://buckets.grayhatwarfare.com/account)' },
    });
  }

  const params = new URLSearchParams({ keywords: DEFAULT_KEYWORDS.join(','), limit: '100' });

  let items = [];
  let live = false;
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(`${BASE}?${params}`, {
      signal: ctrl.signal,
      headers: { accept: 'application/json', authorization: `Bearer ${key}` },
    });
    clearTimeout(timer);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const json = await res.json();
    const buckets = Array.isArray(json.buckets) ? json.buckets
      : Array.isArray(json.results) ? json.results
      : Array.isArray(json) ? json : [];

    items = buckets.map((b, i) => ({
      uid: intelUid(SOURCE_ID, b.id, b.bucket || b.name, `idx_${i}`),
      title: b.bucket || b.name || `bucket-${i}`,
      summary: `${b.type || 'unknown'} · ${b.filesCount ?? b.files_count ?? '?'} files`,
      language: 'en',
      published_at: b.lastUpdated || b.last_updated || null,
      tags: ['exposed-bucket', b.type ? `provider:${b.type}` : null].filter(Boolean),
      properties: {
        bucket: b.bucket || b.name || null,
        type: b.type || null,
        files_count: b.filesCount ?? b.files_count ?? null,
        last_updated: b.lastUpdated || b.last_updated || null,
        public: b.public ?? null,
        region: b.region || null,
        provider: b.provider || null,
      },
    }));
    live = items.length > 0;
  } catch (err) {
    console.warn('[grayhatBuckets] fetch failed:', err?.message);
  }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live,
    description: 'GrayhatWarfare exposed buckets — JP keyword set',
    extraMeta: { keywords: DEFAULT_KEYWORDS },
  });
}
