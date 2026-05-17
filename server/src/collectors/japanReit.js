/**
 * J-REIT collector (japan-reit).
 *
 * Best-effort scrape of japan-reit.com's listed-issue directory
 * (https://www.japan-reit.com/meigara/) to enumerate the listed J-REIT
 * issues (ticker code + name + sector). No auth. On any failure or markup
 * change the collector returns an honest (possibly empty) intel envelope —
 * never fabricated issues.
 *
 * LEGAL/CADENCE: scrape — respect robots.txt, run <= once / 24h.
 *
 * UNVERIFIED MARKUP: japan-reit.com lists each issue in a table row whose
 * link path is `/meigara/<4-digit-code>/`. We extract `(code, name)` pairs
 * from those anchors; a layout change degrades to empty rather than guess.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchText } from './_liveHelpers.js';

const SOURCE_ID = 'japan-reit';
const LIST_URL = 'https://www.japan-reit.com/meigara/';

export default async function collectJapanReit() {
  const html = await fetchText(LIST_URL, { timeoutMs: 15000 });
  if (!html) {
    return intelEnvelope({
      sourceId: SOURCE_ID,
      items: [],
      live: false,
      extraMeta: { source: 'japan_reit_unavailable' },
      description: 'J-REIT listed-issue directory (japan-reit.com, scraped)',
    });
  }

  // Anchors like <a href="/meigara/8951/">日本ビルファンド投資法人</a>
  const re = /<a[^>]+href="\/meigara\/(\d{4})\/?"[^>]*>([^<]{2,})<\/a>/g;
  const seen = new Set();
  const items = [];
  let m;
  while ((m = re.exec(html)) !== null) {
    const code = m[1];
    const name = m[2].replace(/\s+/g, ' ').trim();
    if (seen.has(code)) continue;
    seen.add(code);
    items.push({
      uid: intelUid(SOURCE_ID, code),
      title: `${name} (${code})`,
      summary: `J-REIT listed issue ${code}`,
      body: `japan-reit.com listed J-REIT issue: code=${code}, name=${name}`,
      link: `https://www.japan-reit.com/meigara/${code}/`,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['economy', 'real-estate', 'reit', 'finance'],
      properties: { ticker_code: code, name },
    });
  }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: items.length > 0,
    extraMeta: {
      source: items.length > 0 ? 'japan_reit_scrape' : 'japan_reit_unavailable',
      cadence_hint: 'Run at most once per 24h — respect japan-reit.com robots.txt',
    },
    description: 'J-REIT listed-issue directory (japan-reit.com, scraped)',
  });
}
