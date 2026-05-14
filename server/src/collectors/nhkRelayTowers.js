/**
 * NHK broadcast facility / relay tower registry.
 * https://www.nhk.or.jp/corporateinfo/english/operations/
 *
 * NHK studios + relay-transmitter network — useful for broadcast topology
 * and reception-coverage maps. Companion to `mic-broadcast-towers` (the
 * regulator-side registry).
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'nhk-relay-towers';
const PROBE_URL = 'https://www.nhk.or.jp/corporateinfo/english/operations/';

export default async function collectNhkRelayTowers() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'NHK broadcast / relay facility registry',
      summary: 'NHK studios + relay-transmitter network',
      link: PROBE_URL,
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['broadcast', 'nhk', 'relay', live ? 'reachable' : 'unreachable'],
      properties: { operator: 'NHK', reachable: live },
    }],
    live,
    description: 'NHK broadcast facility / relay tower registry',
  });
}
