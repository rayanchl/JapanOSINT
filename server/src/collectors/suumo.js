/**
 * Suumo rental-price collector (suumo).
 *
 * Best-effort scrape of Suumo's prefecture rental landing pages
 * (https://suumo.jp/chintai/{slug}/) to read the published listing total
 * per prefecture as a rental-market-size proxy. No auth. On any failure,
 * or if the page markup changes, the affected prefecture is simply omitted
 * and the collector returns an honest (possibly empty) intel envelope —
 * never fabricated counts.
 *
 * LEGAL/CADENCE: this is a scrape. Respect Suumo robots.txt; run at most
 * once per 24h. Polling is sequential with a small gap.
 *
 * UNVERIFIED MARKUP: the "N件" total regex below mirrors the verified
 * pattern in suumoRentalDensity.js but Suumo can change layout without
 * notice; a miss degrades to empty for that prefecture.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchText } from './_liveHelpers.js';

const SOURCE_ID = 'suumo';

const SUUMO_SLUGS = {
  '北海道': 'hokkaido', '青森県': 'aomori', '岩手県': 'iwate', '宮城県': 'miyagi',
  '秋田県': 'akita', '山形県': 'yamagata', '福島県': 'fukushima',
  '茨城県': 'ibaraki', '栃木県': 'tochigi', '群馬県': 'gumma', '埼玉県': 'saitama',
  '千葉県': 'chiba', '東京都': 'tokyo', '神奈川県': 'kanagawa',
  '新潟県': 'nigata', '富山県': 'toyama', '石川県': 'ishikawa', '福井県': 'fukui',
  '山梨県': 'yamanashi', '長野県': 'nagano', '岐阜県': 'gifu', '静岡県': 'shizuoka',
  '愛知県': 'aichi', '三重県': 'mie',
  '滋賀県': 'shiga', '京都府': 'kyoto', '大阪府': 'osaka', '兵庫県': 'hyogo',
  '奈良県': 'nara', '和歌山県': 'wakayama',
  '鳥取県': 'tottori', '島根県': 'shimane', '岡山県': 'okayama', '広島県': 'hiroshima', '山口県': 'yamaguchi',
  '徳島県': 'tokushima', '香川県': 'kagawa', '愛媛県': 'ehime', '高知県': 'kochi',
  '福岡県': 'fukuoka', '佐賀県': 'saga', '長崎県': 'nagasaki', '熊本県': 'kumamoto',
  '大分県': 'oita', '宮崎県': 'miyazaki', '鹿児島県': 'kagoshima', '沖縄県': 'okinawa',
};

export default async function collectSuumo() {
  const items = [];
  for (const [prefJa, slug] of Object.entries(SUUMO_SLUGS)) {
    const html = await fetchText(`https://suumo.jp/chintai/${slug}/`, { timeoutMs: 15000 });
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
      title: `Suumo rental listings — ${prefJa}`,
      summary: `${count.toLocaleString()} active rental listings`,
      body: `Suumo prefecture rental landing page (${slug}) reported ${count} listings.`,
      link: `https://suumo.jp/chintai/${slug}/`,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['economy', 'real-estate', 'rental'],
      properties: { prefecture_ja: prefJa, prefecture_slug: slug, rental_listings: count },
    });
    await new Promise((r) => setTimeout(r, 250));
  }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: items.length > 0,
    extraMeta: {
      source: items.length > 0 ? 'suumo_scrape' : 'suumo_unavailable',
      cadence_hint: 'Run at most once per 24h — respect Suumo robots.txt',
    },
    description: 'Suumo per-prefecture rental listing counts (market-size proxy, scraped)',
  });
}
