/**
 * Comiket + Comic City — doujin event calendar.
 * https://www.comiket.co.jp/info-a/
 *
 * Tokyo Big Sight / Makuhari Messe occupancy spikes during Comiket weeks.
 * Pair with crowd-density layers to detect anomalous traffic vs. baseline.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'comiket-events';
const PROBE_URL = 'https://www.comiket.co.jp/info-a/';

export default async function collectComiketEvents() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'Comiket — doujin event calendar',
      summary: 'Comiket + Comic City schedule — Tokyo Big Sight / Makuhari Messe occupancy spikes',
      link: PROBE_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['event', 'doujin', 'comiket', live ? 'reachable' : 'unreachable'],
      properties: { operator: 'Comiket Preparatory Committee', reachable: live },
    }],
    live,
    description: 'Comiket event calendar',
  });
}
