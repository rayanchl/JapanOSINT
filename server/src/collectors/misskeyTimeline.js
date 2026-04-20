/**
 * Misskey.io public timeline — Japanese Fediverse microblog.
 *
 * Endpoint: POST https://misskey.io/api/notes/global-timeline
 *   (anon-read is rate-limited; a free account token unlocks higher
 *   ceilings but is not required for light polling).
 *
 * We pull the most recent N notes, keep those flagged visibility=public,
 * and attach the author's profile `location` field as a geocoder hint.
 * Posts themselves carry no geotag.
 *
 * Empty FeatureCollection when the endpoint rate-limits or when
 * MISSKEY_DISABLED is set.
 */

const MISSKEY_BASE = 'https://misskey.io';
const TIMEOUT_MS = 10000;

export default async function collectMisskeyTimeline() {
  if (process.env.MISSKEY_DISABLED === '1') {
    return empty('misskey_disabled');
  }

  const token = process.env.MISSKEY_TOKEN || null;
  const body = { limit: 30 };
  if (token) body.i = token;

  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(`${MISSKEY_BASE}/api/notes/global-timeline`, {
      method: 'POST',
      signal: controller.signal,
      headers: {
        'content-type': 'application/json',
        'user-agent': 'JapanOSINT/1.0',
      },
      body: JSON.stringify(body),
    });
    clearTimeout(timer);
    if (res.status === 429) return empty('misskey_rate_limited');
    if (!res.ok) return empty(`misskey_http_${res.status}`);
    const notes = await res.json();
    if (!Array.isArray(notes)) return empty('misskey_bad_shape');

    const features = notes
      .filter((n) => n.visibility === 'public' || !n.visibility)
      .map((n) => ({
        type: 'Feature',
        geometry: null,
        properties: {
          id: `MISSKEY_${n.id}`,
          text: (n.text || '').slice(0, 280),
          author: n.user?.username || null,
          author_name: n.user?.name || null,
          author_location: n.user?.location || null,
          reply_count: n.repliesCount ?? 0,
          renote_count: n.renoteCount ?? 0,
          created_at: n.createdAt,
          url: `${MISSKEY_BASE}/notes/${n.id}`,
          source: 'misskey_global_timeline',
        },
      }));

    return {
      type: 'FeatureCollection',
      features,
      _meta: {
        source: 'misskey_live',
        fetchedAt: new Date().toISOString(),
        recordCount: features.length,
        auth: token ? 'token' : 'anonymous',
        description: 'misskey.io public global timeline (Japanese Fediverse)',
      },
      metadata: {},
    };
  } catch (err) {
    console.warn('[misskeyTimeline] fetch failed:', err?.message);
    return empty('misskey_error');
  }
}

function empty(source) {
  return {
    type: 'FeatureCollection',
    features: [],
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: 0,
      description: 'misskey.io timeline (unavailable)',
    },
    metadata: {},
  };
}
