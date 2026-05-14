/**
 * JR + Tokyo Metro per-station boarding (乗降客数) statistics.
 * https://www.jreast.co.jp/passenger/
 * https://www.tokyometro.jp/corporate/enterprise/passenger_rail/
 *
 * Annual per-station pax counts. Cross-ref with `mlit-n02-stations` for
 * choropleth — densest stations = highest-value targets.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'jr-boarding-stats';
const SOURCES = [
  { op: 'JR-East',     url: 'https://www.jreast.co.jp/passenger/' },
  { op: 'Tokyo-Metro', url: 'https://www.tokyometro.jp/corporate/enterprise/passenger_rail/' },
];

export default async function collectJrBoardingStats() {
  const items = [];
  let anyLive = false;
  for (const s of SOURCES) {
    const live = await fetchHead(s.url).catch(() => false);
    if (live) anyLive = true;
    items.push({
      uid: intelUid(SOURCE_ID, s.op),
      title: `${s.op} — per-station boarding stats`,
      summary: 'Annual passenger boardings (乗降客数) by station',
      link: s.url,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['transit', 'rail', 'boarding', live ? 'reachable' : 'unreachable'],
      properties: { operator: s.op, reachable: live },
    });
  }
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: anyLive,
    description: 'JR / Tokyo Metro per-station boarding statistics',
  });
}
