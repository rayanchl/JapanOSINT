/**
 * LIFULL HOME'S for-sale collector (lifull-homes).
 *
 * homes.co.jp is operated by LIFULL. To keep this source genuinely
 * distinct from the `homes-co` rental collector, this one scrapes the
 * for-sale used-apartment section
 * (https://www.homes.co.jp/mansion/chuko/{slug}/) and reads the published
 * listing total per prefecture as a for-sale-market-size proxy. No auth.
 * On any failure or markup change the prefecture is omitted; honest
 * (possibly empty) intel envelope — never fabricated counts.
 *
 * LEGAL/CADENCE: scrape — respect robots.txt, run <= once / 24h.
 *
 * UNVERIFIED MARKUP: the "N件" total regex is best-effort; a miss degrades
 * to empty for that prefecture.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchText } from './_liveHelpers.js';

const SOURCE_ID = 'lifull-homes';

const HOMES_SLUGS = {
  '北海道': 'hokkaido', '宮城県': 'miyagi', '埼玉県': 'saitama', '千葉県': 'chiba',
  '東京都': 'tokyo', '神奈川県': 'kanagawa', '愛知県': 'aichi', '京都府': 'kyoto',
  '大阪府': 'osaka', '兵庫県': 'hyogo', '広島県': 'hiroshima', '福岡県': 'fukuoka',
  '新潟県': 'niigata', '静岡県': 'shizuoka', '岡山県': 'okayama', '熊本県': 'kumamoto',
  '沖縄県': 'okinawa',
};

export default async function collectLifullHomes() {
  const items = [];
  for (const [prefJa, slug] of Object.entries(HOMES_SLUGS)) {
    const url = `https://www.homes.co.jp/mansion/chuko/${slug}/`;
    const html = await fetchText(url, { timeoutMs: 15000 });
    let count = null;
    if (html) {
      const m = html.match(/([0-9,]{2,})\s*件/);
      if (m) {
        const n = parseInt(m[1].replace(/,/g, ''), 10);
        count = Number.isFinite(n) ? n : null;
      }
    }
    if (count == null) { await new Promise((r) => setTimeout(r, 250)); continue; }
    items.push({
      uid: intelUid(SOURCE_ID, slug),
      title: `LIFULL HOME'S used-apartment listings — ${prefJa}`,
      summary: `${count.toLocaleString()} for-sale used apartments`,
      body: `LIFULL HOME'S used-apartment (中古マンション) section for ${prefJa} reported ${count} listings.`,
      link: url,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['economy', 'real-estate', 'for-sale', 'landprice'],
      properties: { prefecture_ja: prefJa, prefecture_slug: slug, forsale_listings: count, segment: 'used_apartment' },
    });
    await new Promise((r) => setTimeout(r, 250));
  }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: items.length > 0,
    extraMeta: {
      source: items.length > 0 ? 'lifull_homes_scrape' : 'lifull_homes_unavailable',
      cadence_hint: 'Run at most once per 24h — respect HOME\'S robots.txt',
    },
    description: 'LIFULL HOME\'S used-apartment for-sale listing counts per prefecture (scraped)',
  });
}
