/**
 * Strava segments — JP-bbox enumeration.
 * https://www.strava.com/api/v3/segments/explore
 *
 * Companion to the heatmap-based `stravaHeatmapBases`. Segments expose
 * named routes (with top-N leaderboards = athletes / pace). Useful for
 * routine-inference around bases + sensitive sites at human resolution.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'strava-segments-jp';
const KEY_ENV = 'STRAVA_TOKEN';
const PROBE_URL = 'https://www.strava.com/';

export default async function collectStravaSegmentsJp() {
  const hasKey = !!process.env[KEY_ENV];
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'Strava segments — JP-bbox explorer',
      summary: hasKey ? 'Configured' : `Set ${KEY_ENV} (OAuth user token) to enable segment exploration`,
      link: PROBE_URL,
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['strava', 'segment', 'routine-inference', live ? 'reachable' : 'unreachable', hasKey ? 'key-present' : 'key-missing'],
      properties: { reachable: live, requires_key: true, has_key: hasKey },
    }],
    live,
    description: 'Strava segments JP-bbox enumeration — routine-inference',
  });
}
