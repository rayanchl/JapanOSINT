/**
 * Bank of Japan statistics portal — reachability ping + dataset index.
 * https://www.stat-search.boj.or.jp/
 *
 * Non-spatial. Emits a single intel item summarising portal status; can grow
 * later to per-dataset entries once we parse the index.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'boj-stats';
const PROBE_URL = 'https://www.stat-search.boj.or.jp/';

export default async function collectBojStats() {
  const live = await fetchHead(PROBE_URL);
  const items = [{
    uid: intelUid(SOURCE_ID, 'portal'),
    title: 'Bank of Japan statistics portal',
    summary: 'Monetary aggregates · rates · BOP indexes',
    link: PROBE_URL,
    language: 'ja',
    published_at: new Date().toISOString(),
    tags: ['economy', 'statistics', live ? 'reachable' : 'unreachable'],
    properties: {
      org: 'Bank of Japan',
      datasets: ['Monetary aggregates', 'Interest rates', 'Balance of payments'],
      reachable: live,
    },
  }];

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live,
    description: 'Bank of Japan statistics portal reachability + index',
  });
}
