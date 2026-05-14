/**
 * bird.makeup — X/Twitter ActivityPub bridge.
 *
 * bird.makeup republishes public X accounts as ActivityPub actors. We poll
 * a curated list of Japan-relevant accounts (disaster agencies, major
 * news, transit operators) via their ActivityPub `/outbox` endpoints.
 *
 * LEGAL NOTE: The bridge mirrors X content in violation of X's ToS. Avoid
 * using it to bulk-mirror private-account data. The default handle list
 * is limited to public, government/news/transit accounts.
 *
 * No auth. Relatively low volume (one fetch per handle per poll).
 */

const BIRD_BASE = 'https://bird.makeup';
const TIMEOUT_MS = 12000;

// Japan-relevant public accounts. Keep conservative — this list is
// intentionally short to minimise traffic and legal exposure.
const DEFAULT_HANDLES = (process.env.BIRD_HANDLES || [
  'earthquakejapan',    // Earthquake alerts
  'UN_NERV',            // Disaster aggregator
  'TokyoMetro_PR',      // Tokyo Metro PR
  'JR_East_official',   // JR East
  'japantimes',         // Japan Times
  'NHK_PR',             // NHK
].join(',')).split(',').map((s) => s.trim()).filter(Boolean);

async function fetchOutbox(handle) {
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const url = `${BIRD_BASE}/users/${encodeURIComponent(handle)}/outbox?page=true`;
    const res = await fetch(url, {
      signal: controller.signal,
      headers: {
        'accept': 'application/activity+json',
        'user-agent': 'JapanOSINT/1.0',
      },
    });
    clearTimeout(timer);
    if (!res.ok) return null;
    const ct = res.headers.get('content-type') || '';
    if (!ct.includes('json')) return null;
    return await res.json();
  } catch {
    return null;
  }
}

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'bird-makeup-jp';

export default async function collectBirdMakeupJp() {
  const results = await Promise.all(DEFAULT_HANDLES.map(fetchOutbox));
  const items = [];

  for (let i = 0; i < results.length; i++) {
    const outbox = results[i];
    const handle = DEFAULT_HANDLES[i];
    const entries = Array.isArray(outbox?.orderedItems) ? outbox.orderedItems : [];
    for (const entry of entries) {
      const obj = entry.object;
      if (!obj || typeof obj !== 'object') continue;
      const rawText = typeof obj.content === 'string' ? obj.content.replace(/<[^>]+>/g, '').trim() : '';
      if (!rawText) continue;
      const publishedRaw = obj.published || entry.published || null;
      const publishedIso = publishedRaw ? safeIso(publishedRaw) : null;
      items.push({
        uid: intelUid(SOURCE_ID, entry.id, obj.id, `${handle}_${publishedRaw || rawText.slice(0, 32)}`),
        title: rawText.slice(0, 120),
        body: rawText,
        summary: rawText.slice(0, 240),
        link: obj.url || obj.id || null,
        author: handle,
        language: 'ja',
        published_at: publishedIso,
        tags: ['x-twitter', 'bird-makeup', `handle:${handle}`],
        properties: {
          handle,
          remote_id: entry.id || obj.id || null,
        },
      });
    }
  }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    description: 'X posts mirrored via bird.makeup ActivityPub bridge (JP-relevant accounts)',
    extraMeta: {
      handles: DEFAULT_HANDLES,
      env_hint: 'Override with BIRD_HANDLES=comma,separated,handles',
      legal_note: 'bird.makeup content originates from X; use responsibly',
    },
  });
}

function safeIso(s) {
  const d = new Date(s);
  return Number.isNaN(d.getTime()) ? null : d.toISOString();
}
