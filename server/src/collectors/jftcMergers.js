/**
 * JFTC merger filings (届出・公表).
 * https://www.jftc.go.jp/dk/kiseido/todokeide/index.html
 *
 * Japan Fair Trade Commission publishes M&A notifications and concentration
 * reviews. Leading indicator for major corporate restructurings, M&A flow.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'jftc-mergers';
const PROBE_URL = 'https://www.jftc.go.jp/dk/kiseido/todokeide/index.html';

export default async function collectJftcMergers() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'JFTC merger / concentration filings',
      summary: 'Japan Fair Trade Commission — M&A notifications and review outcomes',
      link: PROBE_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['antitrust', 'm&a', 'jftc', live ? 'reachable' : 'unreachable'],
      properties: { operator: '公正取引委員会', reachable: live },
    }],
    live,
    description: 'JFTC merger / concentration filings',
  });
}
