/**
 * MIC — election results portal.
 * https://www.soumu.go.jp/senkyo/
 *
 * Ministry of Internal Affairs and Communications publishes per-scrutin
 * CSVs (national, prefecture, lower house, upper house, local), keyed by
 * constituency. Long-tail intel for political-geography overlays.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'mic-elections';
const PROBE_URL = 'https://www.soumu.go.jp/senkyo/';

export default async function collectMicElections() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'MIC election results portal',
      summary: 'Per-scrutin CSVs — national, prefecture, lower / upper house, local',
      link: PROBE_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['election', 'soumu', 'politics', live ? 'reachable' : 'unreachable'],
      properties: { operator: '総務省 自治行政局', reachable: live },
    }],
    live,
    description: 'MIC election results portal',
  });
}
