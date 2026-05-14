/**
 * Earthquake Network — citizen smartphone-accelerometer seismograph.
 * https://www.earthquakenetwork.it/realtime/
 *
 * Crowdsourced quake detections from a global network of phones running
 * the EQN app. Complements `wolfx-eew` / `p2pquake-jma` with grass-roots
 * detections that surface micro-events JMA may not publish (or publish later).
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'earthquake-network-citizen';
const PROBE_URL = 'https://www.earthquakenetwork.it/realtime/';

export default async function collectEarthquakeNetworkCitizen() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'Earthquake Network (citizen smartphones)',
      summary: 'Crowdsourced quake detections from global smartphone accelerometers',
      link: PROBE_URL,
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['earthquake', 'citizen-science', 'eqn', live ? 'reachable' : 'unreachable'],
      properties: { operator: 'Earthquake Network', reachable: live },
    }],
    live,
    description: 'EQN — citizen smartphone-accelerometer seismograph',
  });
}
