/**
 * JNTO arrivals statistics — foreign visitors to Japan.
 * https://statistics.jnto.go.jp/
 *
 * Monthly visitor counts per source country, mode of entry, prefecture
 * lodging stats. Demand-side baseline for tourism + outbound risk
 * inversion.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'jnto-arrivals';
const PROBE_URL = 'https://statistics.jnto.go.jp/';

export default async function collectJntoArrivals() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'JNTO arrivals statistics',
      summary: 'Monthly inbound visitors by nationality / mode of entry / lodging prefecture',
      link: PROBE_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['tourism', 'jnto', 'arrivals', live ? 'reachable' : 'unreachable'],
      properties: { operator: '日本政府観光局 (JNTO)', reachable: live },
    }],
    live,
    description: 'JNTO inbound arrivals stats — monthly + per-prefecture',
  });
}
