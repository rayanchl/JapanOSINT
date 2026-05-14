/**
 * Pokemon GO raid / gym density (community trackers).
 * https://pokegenie.com/ + https://gomap.eu/ + https://thesilphroad.com/
 *
 * Community trackers expose raid + gym density (proxy for "publicly-walked
 * outdoor space" — gyms cluster around landmarks, train stations,
 * convenience stores). Surprisingly clean signal for JP urban POI density.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'pogo-raids-jp';
const PROBE_URL = 'https://gomap.eu/';

export default async function collectPogoRaidsJp() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'Pokemon GO raid / gym density',
      summary: 'Community trackers — proxy for public outdoor POI density',
      link: PROBE_URL,
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['pogo', 'crowdsource', 'poi-density', live ? 'reachable' : 'unreachable'],
      properties: { reachable: live, tos_caveat: true },
    }],
    live,
    description: 'Pokemon GO raid / gym density — community-tracker scrape',
  });
}
