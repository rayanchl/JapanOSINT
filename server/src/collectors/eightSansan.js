/**
 * Eight / Sansan public business-card collector (eight-sansan).
 *
 * Non-spatial intel. Eight exposes no unauthenticated public API and is
 * ToS-rate-limited; live pulls require an account session (EIGHT_SESSION).
 * Without it (or on failure) the collector returns an honest empty result —
 * never fabricated cards. The missing credential surfaces via the
 * apiCredentials map ("needs API key" badge in the Sources tab).
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

export default async function collectEightSansan() {
  const session = envFor('EIGHT_SESSION');
  const base = {
    kind: 'intel',
    meta: {
      fetchedAt: new Date().toISOString(),
      description: 'Public Eight/Sansan business cards (requires EIGHT_SESSION)',
    },
  };
  if (!session) {
    return { ...base, items: [], meta: { ...base.meta, recordCount: 0, source: 'eight_no_session' } };
  }

  const url = 'https://8card.net/api/v2/cards/search?limit=50';
  const data = await fetchJson(url, { timeoutMs: 12000, headers: { Cookie: session } });
  const cards = data?.cards || data?.data;
  if (!Array.isArray(cards)) {
    return { ...base, items: [], meta: { ...base.meta, recordCount: 0, source: 'eight_unavailable' } };
  }

  const items = cards
    .filter((c) => c && (c.id || c.card_id))
    .map((c) => {
      const id = c.id || c.card_id;
      return {
        uid: `eight-sansan|${id}`,
        title: `${c.name || ''} — ${c.title || ''}`.trim(),
        summary: c.company || null,
        body: [c.name, c.title, c.company].filter(Boolean).join(' / '),
        author: c.name || null,
        language: 'ja',
        tags: ['eight', 'business-card'],
        properties: {
          company: c.company || null,
          role: c.title || null,
          email: c.email || null,
          source: 'eight_api',
        },
      };
    });

  return { ...base, items, meta: { ...base.meta, recordCount: items.length, source: 'eight_api' } };
}
