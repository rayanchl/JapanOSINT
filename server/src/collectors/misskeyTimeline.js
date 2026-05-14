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

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'misskey-timeline';
const MISSKEY_BASE = 'https://misskey.io';
const TIMEOUT_MS = 10000;

export default async function collectMisskeyTimeline() {
  if (process.env.MISSKEY_DISABLED === '1') {
    return intelEnvelope({ sourceId: SOURCE_ID, items: [], live: false, description: 'misskey.io timeline (disabled)' });
  }

  const token = process.env.MISSKEY_TOKEN || null;
  const body = { limit: 30 };
  if (token) body.i = token;

  let items = [];
  let live = false;
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(`${MISSKEY_BASE}/api/notes/global-timeline`, {
      method: 'POST',
      signal: ctrl.signal,
      headers: { 'content-type': 'application/json', 'user-agent': 'JapanOSINT/1.0' },
      body: JSON.stringify(body),
    });
    clearTimeout(timer);
    if (res.ok) {
      const notes = await res.json();
      if (Array.isArray(notes)) {
        items = notes
          .filter((n) => n.visibility === 'public' || !n.visibility)
          .map((n) => ({
            uid: intelUid(SOURCE_ID, n.id),
            title: (n.text || '').slice(0, 120) || (n.user?.username ? `@${n.user.username}` : 'note'),
            body: n.text || null,
            summary: (n.text || '').slice(0, 240) || null,
            link: `${MISSKEY_BASE}/notes/${n.id}`,
            author: n.user?.username || null,
            language: 'ja',
            published_at: n.createdAt || null,
            tags: ['fediverse', 'misskey'],
            properties: {
              author_name: n.user?.name || null,
              author_location: n.user?.location || null,
              reply_count: n.repliesCount ?? 0,
              renote_count: n.renoteCount ?? 0,
            },
          }));
        live = items.length > 0;
      }
    }
  } catch (err) {
    console.warn('[misskeyTimeline] fetch failed:', err?.message);
  }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live,
    description: 'misskey.io public global timeline (Japanese Fediverse)',
    extraMeta: { auth: token ? 'token' : 'anonymous' },
  });
}
