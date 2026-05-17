/**
 * VICS road traffic information (vics-traffic).
 *
 * VICS (道路交通情報通信システム) data is licensed and delivered over FM
 * multiplex / radio beacons / ITS spot — there is no free public HTTP/JSON
 * feed. The VICS Center publishes only a status/service portal. We probe
 * that portal for reachability and emit a single honest intel pointer. No
 * fabricated traffic data is ever produced.
 *
 * Endpoint UNVERIFIED for any public data feed — reachability probe only.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'vics-traffic';
const PORTAL_URL = 'https://www.vics.or.jp/';

export default async function collectVicsTraffic() {
  let live = false;
  try { live = await fetchHead(PORTAL_URL); } catch { live = false; }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'VICS road traffic information center',
      summary: 'VICS congestion/travel-time data is licensed (FM multiplex / ITS spot) with no free public JSON feed; portal status only',
      link: PORTAL_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['transport', 'road', 'traffic', 'vics', live ? 'reachable' : 'unreachable'],
      properties: { operator: 'VICSセンター', reachable: live, public_feed: false },
    }],
    live,
    description: 'VICS traffic-information center portal (no free public data feed)',
  });
}
