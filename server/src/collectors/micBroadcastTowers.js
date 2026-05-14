/**
 * MIC broadcast / radio transmitter registry (放送局・無線局検索).
 * https://www.tele.soumu.go.jp/giga-search/
 *
 * Searchable database of every licensed AM/FM/TV/community-FM transmitter,
 * amateur repeater, and other regulated radio installation in Japan.
 * Frequency, callsign, output power, antenna pattern.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'mic-broadcast-towers';
const PROBE_URL = 'https://www.tele.soumu.go.jp/giga-search/';

export default async function collectMicBroadcastTowers() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'MIC radio station registry (giga-search)',
      summary: 'Every licensed AM/FM/TV/community-FM transmitter + amateur repeater',
      link: PROBE_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['broadcast', 'radio', 'soumu', live ? 'reachable' : 'unreachable'],
      properties: { operator: '総務省 電波利用ホームページ', reachable: live },
    }],
    live,
    description: 'MIC giga-search broadcast / radio transmitter registry',
  });
}
