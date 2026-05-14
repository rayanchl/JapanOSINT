/**
 * Yahoo! Auctions — seller intel / listings.
 * https://auctions.yahoo.co.jp/
 *
 * Yahoo Auctions remains the top JP marketplace. Per-seller history is
 * public — useful for stolen-goods cross-ref + supply-chain intel
 * (industrial parts, dual-use electronics).
 *
 * Site forbids automated mass scraping in ToS — this stub is a portal
 * probe only; deliberate human-rate-limited fetches per-seller are the
 * right pattern, not bulk crawl.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'yahoo-auctions';
const PROBE_URL = 'https://auctions.yahoo.co.jp/';

export default async function collectYahooAuctionsSellers() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'Yahoo! Auctions sellers / listings',
      summary: 'Top JP marketplace — per-seller history searchable; ToS-limited',
      link: PROBE_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['marketplace', 'yahoo', 'auctions', live ? 'reachable' : 'unreachable', 'tos-caveat'],
      properties: { operator: 'Yahoo! Japan', reachable: live, tos_caveat: true },
    }],
    live,
    description: 'Yahoo! Auctions — seller intel / listings (ToS-rate-limited)',
  });
}
