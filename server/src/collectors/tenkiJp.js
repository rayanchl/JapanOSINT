/**
 * tenki.jp (Japan Weather Association) collector (tenki-jp) — kind:'intel'.
 *
 * tenki.jp has no public API and no RSS feed; it is a dynamic site. This is a
 * best-effort HTML scrape of the homepage forecast strip. The page markup is
 * UNVERIFIED and may change at any time, so on any fetch/parse failure this
 * returns an honest empty intel result (with a reachability-only portal item)
 * — never fabricated weather values.
 */

import { fetchText } from './_liveHelpers.js';
import { intelEnvelope, intelUid, intelHashKey } from '../utils/intelHelpers.js';

const SOURCE_ID = 'tenki-jp';
const HOME = 'https://tenki.jp/';

function decode(s) {
  return (s || '')
    .replace(/&amp;/g, '&').replace(/&lt;/g, '<').replace(/&gt;/g, '>')
    .replace(/&quot;/g, '"').replace(/&#(\d+);/g, (_, n) => String.fromCharCode(+n))
    .replace(/\s+/g, ' ').trim();
}

export default async function collectTenkiJp() {
  const html = await fetchText(HOME, { timeoutMs: 10000 }).catch(() => null);
  const reachable = !!html;

  const items = [];
  if (html) {
    // Best-effort: pull the "今日の天気" telop blocks if present in the markup.
    // Markup is UNVERIFIED — guarded so any structural change yields no items.
    const re = /<(?:p|span|div)[^>]*class="[^"]*weather-telop[^"]*"[^>]*>([\s\S]*?)<\/(?:p|span|div)>/gi;
    let m;
    let n = 0;
    while ((m = re.exec(html)) !== null && n < 12) {
      const telop = decode(m[1].replace(/<[^>]+>/g, ' '));
      if (!telop || telop.length > 40) continue;
      n++;
      items.push({
        uid: intelUid(SOURCE_ID, intelHashKey(telop, n)),
        title: `tenki.jp 天気: ${telop}`,
        summary: telop,
        body: telop,
        link: HOME,
        author: '日本気象協会 tenki.jp',
        language: 'ja',
        published_at: new Date().toISOString(),
        tags: ['weather', 'tenki-jp', 'forecast'],
        properties: { telop },
      });
    }
  }

  if (items.length === 0) {
    items.push({
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'tenki.jp (日本気象協会)',
      summary: 'Japan Weather Association forecast portal',
      body: 'No structured forecast could be scraped (no public API/RSS); portal reachability only.',
      link: HOME,
      author: '日本気象協会 tenki.jp',
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['weather', 'tenki-jp', reachable ? 'reachable' : 'unreachable'],
      properties: { reachable, machine_readable: false },
    });
  }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: reachable,
    description: 'tenki.jp (Japan Weather Association) — best-effort forecast scrape',
  });
}
