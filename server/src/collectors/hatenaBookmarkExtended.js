/**
 * Hatena Bookmark — extended hot-entry feed (hatena-bookmark-extended).
 *
 * Key-free, public. Extension of the existing `hatena-bookmark` collector:
 * instead of only the top hotentry RSS, this pulls the full hot-entry list
 * across Hatena's category feeds (general + per-category) for a much wider
 * Japanese-web pulse. Non-spatial intel. On failure → honest empty.
 *
 * Feeds (verified, key-free RSS/RDF):
 *   https://b.hatena.ne.jp/hotentry.rss
 *   https://b.hatena.ne.jp/hotentry/{it,social,economics,life,knowledge,fun}.rss
 *   https://b.hatena.ne.jp/entrylist.rss   (broader recent list)
 */

import { fetchText } from './_liveHelpers.js';

const SOURCE_ID = 'hatena-bookmark-extended';

const FEEDS = [
  'https://b.hatena.ne.jp/hotentry.rss',
  'https://b.hatena.ne.jp/hotentry/it.rss',
  'https://b.hatena.ne.jp/hotentry/social.rss',
  'https://b.hatena.ne.jp/hotentry/economics.rss',
  'https://b.hatena.ne.jp/hotentry/life.rss',
  'https://b.hatena.ne.jp/hotentry/knowledge.rss',
  'https://b.hatena.ne.jp/hotentry/fun.rss',
  'https://b.hatena.ne.jp/entrylist.rss',
];

function decodeXmlEntities(s) {
  if (!s) return s;
  return s
    .replace(/&#x([0-9a-fA-F]+);/g, (_, h) => String.fromCodePoint(parseInt(h, 16)))
    .replace(/&#(\d+);/g, (_, d) => String.fromCodePoint(parseInt(d, 10)))
    .replace(/&amp;/g, '&').replace(/&lt;/g, '<').replace(/&gt;/g, '>')
    .replace(/&quot;/g, '"').replace(/&apos;/g, "'");
}

function parseRdfXml(xml) {
  const items = [];
  const itemRe = /<item\b[^>]*>([\s\S]*?)<\/item>/g;
  let m;
  while ((m = itemRe.exec(xml)) !== null) {
    const body = m[1];
    const get = (tag) => {
      const r = new RegExp(`<${tag}\\b[^>]*>([\\s\\S]*?)</${tag}>`);
      const mm = body.match(r);
      return mm ? mm[1].trim() : null;
    };
    const stripCdata = (s) => (s ? s.replace(/^<!\[CDATA\[|\]\]>$/g, '') : s);
    const clean = (tag) => decodeXmlEntities(stripCdata(get(tag)));
    items.push({
      title: clean('title'),
      link: clean('link'),
      date: clean('dc:date'),
      bookmarks: parseInt(clean('hatena:bookmarkcount') || '0', 10) || 0,
      subject: clean('dc:subject'),
      description: clean('description'),
    });
  }
  return items;
}

function safeIso(s) {
  if (!s) return null;
  const d = new Date(s);
  return Number.isNaN(d.getTime()) ? null : d.toISOString();
}

function envelope(items, source) {
  return {
    kind: 'intel',
    items,
    meta: {
      fetchedAt: new Date().toISOString(),
      recordCount: items.length,
      source,
      description: 'Hatena Bookmark extended hot-entry feeds (all categories, key-free)',
    },
  };
}

export default async function collectHatenaBookmarkExtended() {
  const items = [];
  const seen = new Set();
  let gotAny = false;

  for (const feed of FEEDS) {
    let xml = null;
    try {
      xml = await fetchText(feed, { timeoutMs: 15000 });
    } catch { xml = null; }
    if (!xml) continue;
    gotAny = true;
    const category = (/hotentry\/([a-z]+)\.rss/.exec(feed) || [])[1]
      || (feed.includes('entrylist') ? 'recent' : 'general');
    for (const it of parseRdfXml(xml)) {
      if (!it.link || seen.has(it.link)) continue;
      seen.add(it.link);
      items.push({
        uid: `${SOURCE_ID}|${it.link}`,
        title: it.title,
        summary: `${it.bookmarks} bookmarks${it.subject ? ` · ${it.subject}` : ''}`,
        body: it.description || null,
        link: it.link,
        author: null,
        language: 'ja',
        published_at: safeIso(it.date),
        tags: ['hatena', 'trending', `feed:${category}`, it.subject ? `cat:${it.subject}` : null]
          .filter(Boolean),
        properties: {
          bookmarks: it.bookmarks,
          subject: it.subject || null,
          feed_category: category,
          source: 'hatena_rss',
        },
      });
    }
  }

  if (!gotAny) return envelope([], 'hatena_bookmark_extended_unavailable');
  if (!items.length) return envelope([], 'hatena_bookmark_extended_unavailable');
  return envelope(items, 'hatena_rss');
}
