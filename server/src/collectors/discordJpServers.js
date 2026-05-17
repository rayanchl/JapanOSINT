/**
 * Discord JP public server discovery (discord-jp-servers).
 *
 * Key-free best-effort. Public Discord server-listing directories expose
 * JSON/HTML for Japan-tagged servers:
 *   - Disboard:  https://disboard.org/ja/servers/tag/japanese  (HTML)
 *   - top.gg:    https://top.gg/api/v1/...                       (keyed; skipped)
 * We parse Disboard's public server listing (HTML). On any failure we return
 * an honest empty result — no fabricated servers.
 *
 * Unverified at runtime (HTML structure may change): the Disboard listing
 * markup. Credential var: none (public listing).
 */

import { fetchText } from './_liveHelpers.js';

const SOURCE_ID = 'discord-jp-servers';
const LIST_URLS = [
  'https://disboard.org/ja/servers/tag/japanese',
  'https://disboard.org/ja/servers/tag/日本',
];

function envelope(items, source) {
  return {
    kind: 'intel',
    items,
    meta: {
      fetchedAt: new Date().toISOString(),
      recordCount: items.length,
      source,
      description: 'Japan-tagged public Discord servers from Disboard listing (key-free best-effort)',
    },
  };
}

function decodeEntities(s) {
  if (!s) return s;
  return s
    .replace(/&#x([0-9a-fA-F]+);/g, (_, h) => String.fromCodePoint(parseInt(h, 16)))
    .replace(/&#(\d+);/g, (_, d) => String.fromCodePoint(parseInt(d, 10)))
    .replace(/&amp;/g, '&').replace(/&lt;/g, '<').replace(/&gt;/g, '>')
    .replace(/&quot;/g, '"').replace(/&#39;/g, "'").replace(/&apos;/g, "'");
}

// Disboard server cards link to /server/join/<id>?... and carry a name in the
// card. We extract (server id, name) pairs from the listing HTML defensively.
function parseDisboard(html) {
  const out = [];
  const seen = new Set();
  const re = /\/server\/join\/(\d{6,25})[^"']*["'][^>]*>([\s\S]*?)<\/a>/g;
  let m;
  while ((m = re.exec(html)) !== null) {
    const id = m[1];
    if (seen.has(id)) continue;
    seen.add(id);
    const text = decodeEntities(m[2].replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim());
    out.push({ id, name: text || `Discord server ${id}` });
  }
  return out;
}

export default async function collectDiscordJpServers() {
  const items = [];
  const seen = new Set();
  let gotAny = false;

  for (const url of LIST_URLS) {
    let html = null;
    try {
      html = await fetchText(url, { timeoutMs: 15000 });
    } catch { html = null; }
    if (!html) continue;
    gotAny = true;
    for (const s of parseDisboard(html)) {
      if (seen.has(s.id)) continue;
      seen.add(s.id);
      items.push({
        uid: `${SOURCE_ID}|${s.id}`,
        title: s.name,
        summary: 'Japan-tagged public Discord server (Disboard listing)',
        body: null,
        link: `https://disboard.org/server/${s.id}`,
        author: null,
        language: 'ja',
        published_at: null,
        tags: ['discord', 'server', 'japanese'],
        properties: { server_id: s.id, source: 'disboard' },
      });
    }
  }

  if (!gotAny) return envelope([], 'discord_jp_servers_unavailable');
  if (!items.length) return envelope([], 'discord_jp_servers_unavailable');
  return envelope(items, 'disboard');
}
