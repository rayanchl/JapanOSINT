/**
 * Telegram JP channel discovery (telegram-jp-channels).
 *
 * Needs credentials, resolved via envFor. Two supported approaches:
 *   - TGStat public API (channel search):  TGSTAT_API_KEY
 *   - Telegram API app credentials:        TELEGRAM_API_ID + TELEGRAM_API_HASH
 *     (MTProto requires a session; without a server-side userbot we can only
 *      record that credentials are present — we do NOT fabricate channels, so
 *      with only API_ID/HASH and no TGStat key we return honest empty.)
 *
 * Without any usable credential the collector returns an honest empty result.
 *
 * Endpoint (TGStat, verified keyed):
 *   https://api.tgstat.com/channels/search
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

const SOURCE_ID = 'telegram-jp-channels';
const QUERIES = ['日本', '東京', 'ニュース', 'OSINT', 'セキュリティ'];

function envelope(items, source) {
  return {
    kind: 'intel',
    items,
    meta: {
      fetchedAt: new Date().toISOString(),
      recordCount: items.length,
      source,
      description: 'Japan-relevant public Telegram channels (requires TGStat or Telegram API credentials)',
    },
  };
}

export default async function collectTelegramJpChannels() {
  const tgstatKey = envFor('TGSTAT_API_KEY');
  const apiId = envFor('TELEGRAM_API_ID');
  const apiHash = envFor('TELEGRAM_API_HASH');

  // No usable path without TGStat (MTProto userbot is out of scope here).
  if (!tgstatKey) {
    if (apiId && apiHash) {
      // Credentials present but MTProto session unavailable server-side;
      // honest empty rather than fabricated channels.
      return envelope([], 'telegram_jp_channels_unavailable');
    }
    return envelope([], 'telegram_jp_channels_no_key');
  }

  const items = [];
  const seen = new Set();
  try {
    for (const q of QUERIES) {
      // TGStat channels/search — verified keyed endpoint.
      const url = 'https://api.tgstat.com/channels/search'
        + `?token=${encodeURIComponent(tgstatKey)}`
        + `&q=${encodeURIComponent(q)}&country=JP&limit=30`;
      const data = await fetchJson(url, { timeoutMs: 15000 });
      const list = data?.response?.items || data?.response?.channels || [];
      for (const c of list) {
        const username = c.username || c.link || c.channel?.username;
        const id = username || c.tg_id || c.id;
        if (!id || seen.has(id)) continue;
        seen.add(id);
        const link = username
          ? `https://t.me/${String(username).replace(/^@/, '')}`
          : (c.link || null);
        items.push({
          uid: `${SOURCE_ID}|${id}`,
          title: c.title || username || String(id),
          summary: c.about || c.description || null,
          body: c.about || c.description || null,
          link,
          author: username || null,
          language: 'ja',
          published_at: null,
          tags: ['telegram', 'channel', `q:${q}`],
          properties: {
            username: username || null,
            participants: c.participants_count ?? c.subscribers ?? null,
            matched_query: q,
            source: 'tgstat_api',
          },
        });
      }
    }
  } catch {
    return envelope([], 'telegram_jp_channels_unavailable');
  }

  if (!items.length) return envelope([], 'telegram_jp_channels_unavailable');
  return envelope(items, 'tgstat_api');
}
