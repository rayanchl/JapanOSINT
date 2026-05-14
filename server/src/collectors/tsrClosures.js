/**
 * Tokyo Shoko Research — corporate closure releases.
 * https://www.tsr-net.co.jp/news/tsr_release/
 *
 * TSR is the second canonical credit-research outlet (alongside TDB).
 * Same domain as `teikoku-failures` but distinct enumeration cadence and
 * stricter "officially closed" filter — pair both for high recall.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'tsr-closures';
const PROBE_URL = 'https://www.tsr-net.co.jp/news/tsr_release/';

export default async function collectTsrClosures() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'Tokyo Shoko Research — corporate closures',
      summary: 'TSR release feed — closures, liquidations, restructurings',
      link: PROBE_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['bankruptcy', 'tsr', 'corporate', live ? 'reachable' : 'unreachable'],
      properties: { operator: '東京商工リサーチ', reachable: live },
    }],
    live,
    description: 'Tokyo Shoko Research — corporate closures (releases)',
  });
}
