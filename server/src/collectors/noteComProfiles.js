/**
 * note.com author/profile search collector (note-com-profiles).
 *
 * Non-spatial intel. note.com's public v2 search API needs no key; we query
 * a set of JP-relevant terms and emit author profiles as intel items. On
 * failure the collector returns an honest empty result — no fabricated data.
 */

import { fetchJson } from './_liveHelpers.js';

const QUERIES = ['東京', 'OSINT', '防災', 'スタートアップ', 'セキュリティ'];

export default async function collectNoteComProfiles() {
  const seen = new Set();
  const items = [];

  for (const q of QUERIES) {
    const url = `https://note.com/api/v2/searches?context=user&q=${encodeURIComponent(q)}&size=20`;
    const data = await fetchJson(url, { timeoutMs: 12000 });
    const users = data?.data?.users?.contents || data?.data?.contents || [];
    for (const u of users) {
      const urlname = u.urlname || u.id;
      if (!urlname || seen.has(urlname)) continue;
      seen.add(urlname);
      items.push({
        uid: `note-com-profiles|${urlname}`,
        title: u.nickname || u.name || urlname,
        summary: u.profile || null,
        body: u.profile || null,
        link: `https://note.com/${urlname}`,
        author: u.nickname || urlname,
        language: 'ja',
        published_at: u.created_at || null,
        tags: ['note.com', 'profile', `q:${q}`],
        properties: {
          urlname,
          followers: u.followerCount ?? null,
          notes: u.noteCount ?? null,
          matched_query: q,
          source: 'note_api',
        },
      });
    }
  }

  return {
    kind: 'intel',
    items,
    meta: {
      fetchedAt: new Date().toISOString(),
      recordCount: items.length,
      source: items.length ? 'note_api' : 'note_unavailable',
      description: 'note.com author search by JP-relevant queries (key-free public API)',
    },
  };
}
