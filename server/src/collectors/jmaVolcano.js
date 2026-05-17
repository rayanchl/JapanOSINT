/**
 * JMA Volcano information collector (jma-volcano) — kind:'intel'.
 *
 * The JMA bosai volcano sub-tree (https://www.jma.go.jp/bosai/volcano/) is a
 * JavaScript SPA with no stable key-free JSON list endpoint (every guessed
 * data/*.json path 404s). The documented, key-free public feed for volcanic
 * activity is the JMA developer XML feed:
 *   https://www.data.jma.go.jp/developer/xml/feed/eqvol.xml
 * which carries earthquake AND volcano bulletins (火山, 噴火警報, 降灰予報…).
 * We parse that Atom feed and keep only volcano-related entries, emitting each
 * as an intel item. Honest empty on fetch failure — never fabricated.
 */

import { fetchText } from './_liveHelpers.js';
import { intelEnvelope, intelUid, parseFeed } from '../utils/intelHelpers.js';

const SOURCE_ID = 'jma-volcano';
const FEED_URL = 'https://www.data.jma.go.jp/developer/xml/feed/eqvol.xml';

// Keep entries whose title indicates volcanic activity (not earthquake-only).
const VOLCANO_RE = /(火山|噴火|降灰|Volcan|volcan|Ash Fall|ashfall)/i;

export default async function collectJmaVolcano() {
  const xml = await fetchText(FEED_URL, { timeoutMs: 10000 }).catch(() => null);
  if (!xml) {
    return intelEnvelope({
      sourceId: SOURCE_ID,
      items: [],
      live: false,
      description: 'JMA volcano bulletins (eqvol XML feed) — upstream unavailable',
    });
  }

  const entries = parseFeed(xml).filter((e) => VOLCANO_RE.test(`${e.title || ''} ${e.description || ''}`));
  const items = entries.map((e) => {
    let publishedIso = null;
    if (e.pubDate) {
      const d = new Date(e.pubDate);
      if (!Number.isNaN(d.getTime())) publishedIso = d.toISOString();
    }
    return {
      uid: intelUid(SOURCE_ID, e.guid, e.link, e.title),
      title: e.title || null,
      summary: (e.description || e.title || '').slice(0, 240) || null,
      body: e.description || null,
      link: e.link || null,
      author: e.author || '気象庁 Japan Meteorological Agency',
      language: 'ja',
      published_at: publishedIso,
      tags: ['volcano', 'jma', 'hazard'],
      properties: { guid: e.guid || null },
    };
  });

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: items.length > 0,
    description: 'JMA volcanic activity bulletins (噴火警報・降灰予報) from the eqvol XML feed',
  });
}
