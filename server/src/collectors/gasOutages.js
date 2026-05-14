/**
 * Gas operator outage / disaster info portals.
 *   Tokyo Gas  https://www.tokyo-gas.co.jp/saigai/
 *   Osaka Gas  https://home.osakagas.co.jp/disaster/
 *   Toho Gas   https://www.tohogas.co.jp/anshin/saigai/
 *   Saibu Gas  https://www.saibugas.co.jp/info/saigai/
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'gas-outages';
const OPS = [
  ['Tokyo-Gas',  'https://www.tokyo-gas.co.jp/saigai/'],
  ['Osaka-Gas',  'https://home.osakagas.co.jp/disaster/'],
  ['Toho-Gas',   'https://www.tohogas.co.jp/anshin/saigai/'],
  ['Saibu-Gas',  'https://www.saibugas.co.jp/info/saigai/'],
];

export default async function collectGasOutages() {
  const items = [];
  let anyLive = false;
  for (const [op, url] of OPS) {
    const live = await fetchHead(url).catch(() => false);
    if (live) anyLive = true;
    items.push({
      uid: intelUid(SOURCE_ID, op),
      title: `${op} disaster / outage portal`,
      summary: 'Gas-utility disaster / outage info page',
      link: url,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['gas', 'outage', op.toLowerCase().replace('-', '_'), live ? 'reachable' : 'unreachable'],
      properties: { operator: op, reachable: live },
    });
  }
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: anyLive,
    description: 'Gas-utility disaster / outage portals (4 major operators)',
  });
}
