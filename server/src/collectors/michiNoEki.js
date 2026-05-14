/**
 * Michi-no-Eki (道の駅) — MLIT-designated roadside-station registry.
 * https://www.michi-no-eki.jp/
 *
 * Nationwide rest stops on secondary roads. Useful for the camping-car /
 * caravan / secondary-route layer + as cross-ref for tourism analytics.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'michi-no-eki';
const PROBE_URL = 'https://www.michi-no-eki.jp/';

export default async function collectMichiNoEki() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'Michi-no-Eki — roadside station registry',
      summary: 'MLIT-designated 道の駅 — camping-car / secondary-route POIs',
      link: PROBE_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['poi', 'tourism', 'roadside', live ? 'reachable' : 'unreachable'],
      properties: { operator: '全国「道の駅」連絡会', reachable: live },
    }],
    live,
    description: 'Michi-no-Eki — MLIT roadside-station registry',
  });
}
