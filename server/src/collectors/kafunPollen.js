/**
 * Ministry of the Environment pollen forecast (花粉観測システム "はなこさん").
 * https://kafun.env.go.jp/
 *
 * Per-prefecture pollen concentration (cedar / cypress / grasses), updated
 * hourly during the season. Useful health/density seasonal signal.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'kafun-pollen';
const PROBE_URL = 'https://kafun.env.go.jp/';

export default async function collectKafunPollen() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'MOE pollen monitoring (はなこさん)',
      summary: 'Per-prefecture pollen concentration — cedar / cypress / grasses',
      link: PROBE_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['pollen', 'environment', 'moe', live ? 'reachable' : 'unreachable'],
      properties: { operator: '環境省', reachable: live },
    }],
    live,
    description: 'MOE pollen monitoring (はなこさん) — per-prefecture concentration',
  });
}
