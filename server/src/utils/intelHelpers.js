/**
 * Tiny helpers for collectors that emit `kind: 'intel'`. Keeps the per-file
 * migrations small and consistent.
 */

import crypto from 'node:crypto';

/**
 * Build a stable item uid: "<source_id>|<key>" where <key> is one of the
 * caller-provided fields, or a sha1 fallback over the row.
 */
export function intelUid(sourceId, ...candidates) {
  for (const c of candidates) {
    if (c == null) continue;
    const s = String(c);
    if (s.length > 0) return `${sourceId}|${s}`;
  }
  return `${sourceId}|${crypto.randomBytes(8).toString('hex')}`;
}

/**
 * Stable hash key for rows that have no obvious identifier — e.g. RSS items
 * without guid. Hashes the (canonical) URL+title so reruns coalesce.
 */
export function intelHashKey(...parts) {
  const h = crypto.createHash('sha1');
  for (const p of parts) {
    if (p == null) continue;
    h.update(String(p));
    h.update('|');
  }
  return h.digest('hex').slice(0, 20);
}

/**
 * Wrap an array of items into the canonical intel envelope.
 */
export function intelEnvelope({ sourceId, items, description, fetchedAt = null, live = null, ttlMs = null, extraMeta = {} }) {
  return {
    kind: 'intel',
    items,
    meta: {
      source_id: sourceId,
      fetchedAt: fetchedAt || new Date().toISOString(),
      recordCount: items.length,
      live: live != null ? !!live : items.length > 0,
      description,
      ...(ttlMs != null ? { ttlMs } : {}),
      ...extraMeta,
    },
  };
}

// ── Tiny RSS / Atom parser ─────────────────────────────────────────────────
// Just enough to pull title / link / description / pubDate out of well-formed
// public feeds. Not a general XML parser; doesn't handle namespaces or CDATA
// edge cases beyond the common case.

function decodeEntities(s) {
  if (!s) return s;
  return s
    .replace(/<!\[CDATA\[([\s\S]*?)\]\]>/g, '$1')
    .replace(/&lt;/g, '<')
    .replace(/&gt;/g, '>')
    .replace(/&quot;/g, '"')
    .replace(/&apos;/g, "'")
    .replace(/&#(\d+);/g, (_, n) => String.fromCharCode(parseInt(n, 10)))
    .replace(/&#x([0-9a-fA-F]+);/g, (_, n) => String.fromCharCode(parseInt(n, 16)))
    .replace(/&amp;/g, '&');
}

function pick(xml, tag) {
  const re = new RegExp(`<${tag}\\b[^>]*>([\\s\\S]*?)<\\/${tag}>`, 'i');
  const m = xml.match(re);
  return m ? decodeEntities(m[1]).trim() : null;
}

function pickAttr(xml, tag, attr) {
  const re = new RegExp(`<${tag}\\b[^>]*\\b${attr}=["']([^"']+)["']`, 'i');
  const m = xml.match(re);
  return m ? decodeEntities(m[1]).trim() : null;
}

/**
 * Extract items from an RSS 2.0 feed. Returns an array of { title, link,
 * description, pubDate, guid, author }.
 */
export function parseRss(xml) {
  const items = [];
  const itemRe = /<item\b[^>]*>([\s\S]*?)<\/item>/gi;
  let m;
  while ((m = itemRe.exec(xml)) !== null) {
    const block = m[1];
    items.push({
      title:       pick(block, 'title'),
      link:        pick(block, 'link') || pickAttr(block, 'link', 'href'),
      description: pick(block, 'description'),
      pubDate:     pick(block, 'pubDate') || pick(block, 'dc:date'),
      guid:        pick(block, 'guid'),
      author:      pick(block, 'author') || pick(block, 'dc:creator'),
    });
  }
  return items;
}

/**
 * Extract entries from an Atom feed.
 */
export function parseAtom(xml) {
  const items = [];
  const entryRe = /<entry\b[^>]*>([\s\S]*?)<\/entry>/gi;
  let m;
  while ((m = entryRe.exec(xml)) !== null) {
    const block = m[1];
    items.push({
      title:       pick(block, 'title'),
      link:        pickAttr(block, 'link', 'href') || pick(block, 'link'),
      description: pick(block, 'summary') || pick(block, 'content'),
      pubDate:     pick(block, 'updated') || pick(block, 'published'),
      guid:        pick(block, 'id'),
      author:      pick(block, 'name'),
    });
  }
  return items;
}

/** Try RSS first, fall back to Atom. */
export function parseFeed(xml) {
  const rss = parseRss(xml);
  if (rss.length > 0) return rss;
  return parseAtom(xml);
}

/**
 * Convert a feed entry into an intel item. `language` defaults to 'ja' (most
 * Japanese feeds); set explicitly for English-language feeds.
 */
export function feedItemToIntel(sourceId, entry, { language = 'ja', tags = [] } = {}) {
  const uid = intelUid(
    sourceId,
    entry.guid,
    entry.link,
    entry.title ? intelHashKey(entry.title, entry.pubDate) : null,
  );
  let publishedIso = null;
  if (entry.pubDate) {
    const d = new Date(entry.pubDate);
    if (!Number.isNaN(d.getTime())) publishedIso = d.toISOString();
  }
  return {
    uid,
    title: entry.title || null,
    body: entry.description || null,
    summary: entry.description ? entry.description.slice(0, 240) : null,
    link: entry.link || null,
    author: entry.author || null,
    language,
    published_at: publishedIso,
    tags,
    properties: entry.guid ? { guid: entry.guid } : {},
  };
}
