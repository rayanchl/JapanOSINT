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

export default async function collectBirdMakeupJp() {
  const results = await Promise.all(DEFAULT_HANDLES.map(fetchOutbox));
  const features = [];

  for (let i = 0; i < results.length; i++) {
    const outbox = results[i];
    const handle = DEFAULT_HANDLES[i];
    const items = Array.isArray(outbox?.orderedItems) ? outbox.orderedItems : [];
    for (const item of items) {
      const obj = item.object;
      if (!obj || typeof obj !== 'object') continue;
      const rawText = typeof obj.content === 'string' ? obj.content.replace(/<[^>]+>/g, '') : '';
      if (!rawText) continue;
      features.push({
        type: 'Feature',
        geometry: null,
        properties: {
          id: `BIRD_${handle}_${item.id || obj.id}`,
          handle,
          text: rawText.slice(0, 280),
          published: obj.published || item.published || null,
          url: obj.url || obj.id || null,
          source: 'bird_makeup',
        },
      });
    }
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: features.length ? 'bird_makeup_live' : 'bird_makeup_unavailable',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      handles: DEFAULT_HANDLES,
      env_hint: 'Override with BIRD_HANDLES=comma,separated,handles',
      description: 'X posts mirrored via bird.makeup ActivityPub bridge (JP-relevant accounts)',
      legal_note: 'bird.makeup content originates from X; use responsibly',
    },
    metadata: {},
  };
}
