/**
 * TEPCO Kanto outage map.
 * https://teideninfo.tepco.co.jp/
 *
 * TEPCO publishes a per-municipality outage feed (rough lat/lon + customer
 * count). Useful for incident correlation with storms / earthquakes.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'tepco-outage';
const PROBE_URL = 'https://teideninfo.tepco.co.jp/';

export default async function collectTepcoOutage() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'TEPCO Kanto outage map',
      summary: 'Per-municipality power outage feed, customer-count granular',
      link: PROBE_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['power', 'outage', 'tepco', live ? 'reachable' : 'unreachable'],
      properties: { operator: '東京電力パワーグリッド', reachable: live },
    }],
    live,
    description: 'TEPCO Kanto power outage feed',
  });
}
