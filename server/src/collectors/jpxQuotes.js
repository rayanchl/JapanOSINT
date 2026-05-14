/**
 * JPX — Tokyo Stock Exchange daily statistics.
 * https://www.jpx.co.jp/markets/statistics-equities/
 *
 * Daily CSV equity statistics — top movers, sector breakdowns, Nikkei225
 * weights. Complements EDINET (statutory filings) + TDnet (timely
 * disclosure) with the price-side signal.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'jpx-quotes';
const PROBE_URL = 'https://www.jpx.co.jp/markets/statistics-equities/';

export default async function collectJpxQuotes() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'JPX equity statistics',
      summary: 'TSE daily equity stats — top movers, sectors, Nikkei225 components',
      link: PROBE_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['equity', 'jpx', 'tse', live ? 'reachable' : 'unreachable'],
      properties: { operator: 'Japan Exchange Group', reachable: live },
    }],
    live,
    description: 'JPX equity statistics portal',
  });
}
