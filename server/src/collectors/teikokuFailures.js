/**
 * Teikoku Databank — corporate failure listings.
 * https://www.tdb.co.jp/tosan/syosai/
 *
 * Monthly bankruptcy / liquidation listings. TDB summary is free; full
 * detail PDFs are paid. Useful for cross-ref with EDINET / TDnet to spot
 * stress signals.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'teikoku-failures';
const PROBE_URL = 'https://www.tdb.co.jp/tosan/syosai/';

export default async function collectTeikokuFailures() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'Teikoku Databank — corporate failure listings',
      summary: 'Monthly TDB bankruptcy / liquidation summary',
      link: PROBE_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['bankruptcy', 'tdb', 'corporate', live ? 'reachable' : 'unreachable'],
      properties: { operator: '帝国データバンク', reachable: live },
    }],
    live,
    description: 'TDB monthly corporate failure listings',
  });
}
