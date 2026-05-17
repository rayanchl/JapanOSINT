/**
 * Disaster information feed — saigai-info (intel-kind).
 *
 * Real: JMA's public disaster-prevention XML feed (PULL型, 高頻度).
 *   https://www.data.jma.go.jp/developer/xml/feed/extra.xml
 * This Atom feed lists JMA "extra" disaster bulletins (warnings, advisories,
 * volcanic, tsunami, etc.). Each entry becomes one intel item. No fabrication:
 * on fetch/parse failure we return an honest empty items list.
 */

import { fetchText } from './_liveHelpers.js';
import { intelUid, intelHashKey } from '../utils/intelHelpers.js';

const FEED_URL = 'https://www.data.jma.go.jp/developer/xml/feed/extra.xml';
const SOURCE_ID = 'saigai-info';

function decodeEntities(s) {
  if (!s) return s;
  return s
    .replace(/<!\[CDATA\[([\s\S]*?)\]\]>/g, '$1')
    .replace(/&lt;/g, '<')
    .replace(/&gt;/g, '>')
    .replace(/&quot;/g, '"')
    .replace(/&apos;/g, "'")
    .replace(/&#(\d+);/g, (_, n) => String.fromCharCode(parseInt(n, 10)))
    .replace(/&amp;/g, '&');
}

function tag(xml, name) {
  const m = xml.match(new RegExp(`<${name}\\b[^>]*>([\\s\\S]*?)<\\/${name}>`, 'i'));
  return m ? decodeEntities(m[1]).trim() : null;
}

function linkHref(xml) {
  const m = xml.match(/<link\b[^>]*\bhref=["']([^"']+)["']/i);
  return m ? decodeEntities(m[1]).trim() : null;
}

function emptyResult(extraMeta = {}) {
  return {
    kind: 'intel',
    items: [],
    meta: {
      fetchedAt: new Date().toISOString(),
      recordCount: 0,
      source: 'saigai_info',
      description: 'JMA disaster-prevention bulletins (warnings / advisories / volcanic / tsunami)',
      ...extraMeta,
    },
  };
}

export default async function collectSaigaiInfo() {
  let xml;
  try {
    xml = await fetchText(FEED_URL, { timeoutMs: 12000 });
  } catch {
    return emptyResult({ source: 'saigai_info_unavailable' });
  }
  if (!xml || typeof xml !== 'string') {
    return emptyResult({ source: 'saigai_info_unavailable' });
  }

  const items = [];
  const entryRe = /<entry\b[^>]*>([\s\S]*?)<\/entry>/gi;
  let m;
  while ((m = entryRe.exec(xml)) !== null) {
    const e = m[1];
    const title = tag(e, 'title');
    const link = linkHref(e);
    const id = tag(e, 'id');
    const updated = tag(e, 'updated');
    const author = tag(e, 'name');
    const content = tag(e, 'content');
    if (!title && !link) continue;
    items.push({
      uid: intelUid(SOURCE_ID, id, link, intelHashKey(title, updated)),
      title: title || '(無題)',
      summary: content || title || '',
      body: content || title || '',
      link: link || null,
      published_at: updated || null,
      tags: ['disaster', 'jma', 'saigai'],
      properties: {
        feed_id: id || null,
        author: author || 'JMA',
      },
    });
  }

  if (items.length === 0) {
    return emptyResult({ source: 'saigai_info_unavailable' });
  }

  return {
    kind: 'intel',
    items,
    meta: {
      fetchedAt: new Date().toISOString(),
      recordCount: items.length,
      source: 'saigai_info',
      description: 'JMA disaster-prevention bulletins (warnings / advisories / volcanic / tsunami)',
    },
  };
}
