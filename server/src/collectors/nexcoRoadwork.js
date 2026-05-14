/**
 * NEXCO East / Central / West roadwork + planned closures.
 *   East    https://www.e-nexco.co.jp/traffic/
 *   Central https://www.c-nexco.co.jp/road_info/
 *   West    https://www.w-nexco.co.jp/traffic/
 *
 * Each NEXCO operator publishes upcoming construction and lane closures
 * by section. Complements jartic-traffic (live) with the 30-day forward
 * outlook for planned restrictions.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'nexco-roadwork';
const OPS = [
  ['NEXCO-East',    'https://www.e-nexco.co.jp/traffic/'],
  ['NEXCO-Central', 'https://www.c-nexco.co.jp/road_info/'],
  ['NEXCO-West',    'https://www.w-nexco.co.jp/traffic/'],
];

export default async function collectNexcoRoadwork() {
  const items = [];
  let anyLive = false;
  for (const [op, url] of OPS) {
    const live = await fetchHead(url).catch(() => false);
    if (live) anyLive = true;
    items.push({
      uid: intelUid(SOURCE_ID, op),
      title: `${op} roadwork / planned closures`,
      summary: 'Construction + lane-closure outlook by section',
      link: url,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['highway', 'roadwork', 'nexco', live ? 'reachable' : 'unreachable'],
      properties: { operator: op, reachable: live },
    });
  }
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: anyLive,
    description: 'NEXCO East/Central/West roadwork + planned closures',
  });
}
