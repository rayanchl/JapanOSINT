/**
 * Blitzortung — citizen-network real-time lightning strikes.
 * https://www.blitzortung.org/en/live_lightning_maps.php
 *
 * Free WebSocket stream (`wss://ws*.blitzortung.org/`) of detected impacts.
 * Pair with `jma-weather` for severe-storm correlation. We register the
 * portal here; a follow-up WS consumer ingests strikes into a feature
 * stream.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'blitzortung-lightning';
const PROBE_URL = 'https://www.blitzortung.org/en/live_lightning_maps.php';

export default async function collectBlitzortungLightning() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'Blitzortung lightning network',
      summary: 'Citizen-detected real-time lightning strikes (WSS feed)',
      link: PROBE_URL,
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['lightning', 'weather', 'citizen-science', live ? 'reachable' : 'unreachable'],
      properties: { operator: 'Blitzortung.org', reachable: live },
    }],
    live,
    description: 'Blitzortung — real-time lightning detection network',
  });
}
