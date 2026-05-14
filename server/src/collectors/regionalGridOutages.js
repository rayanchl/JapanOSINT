/**
 * Regional grid outage maps (KEPCO + Chubu + Tohoku + Kyuden + HEPCO etc.).
 * Companion to `tepco-outage`. Each operator publishes its own portal:
 *   KEPCO   https://www.kansai-td.co.jp/teiden/area_jishin/
 *   Chubu   https://teiden.chuden.jp/p/index.html
 *   Tohoku  https://teiden.nw.tohoku-epco.co.jp/
 *   Kyuden  https://www.kyuden.co.jp/td_teiden_index.html
 *   HEPCO   https://www.hepco.co.jp/network/teiden_info/
 *   Rikuden https://teiden.rikuden.co.jp/
 *   Chugoku https://teiden.energia.co.jp/
 *   Yonden  https://www.yonden.co.jp/cnt_teiden/
 *   Okiden  https://www.okiden.co.jp/dispatch/
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'regional-grid-outages';
const OPS = [
  ['KEPCO',   'https://www.kansai-td.co.jp/teiden/area_jishin/'],
  ['Chubu',   'https://teiden.chuden.jp/p/index.html'],
  ['Tohoku',  'https://teiden.nw.tohoku-epco.co.jp/'],
  ['Kyuden',  'https://www.kyuden.co.jp/td_teiden_index.html'],
  ['HEPCO',   'https://www.hepco.co.jp/network/teiden_info/'],
  ['Rikuden', 'https://teiden.rikuden.co.jp/'],
  ['Chugoku', 'https://teiden.energia.co.jp/'],
  ['Yonden',  'https://www.yonden.co.jp/cnt_teiden/'],
  ['Okiden',  'https://www.okiden.co.jp/dispatch/'],
];

export default async function collectRegionalGridOutages() {
  const items = [];
  let anyLive = false;
  for (const [op, url] of OPS) {
    const live = await fetchHead(url).catch(() => false);
    if (live) anyLive = true;
    items.push({
      uid: intelUid(SOURCE_ID, op),
      title: `${op} outage portal`,
      summary: 'Per-municipality outage feed',
      link: url,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['power', 'outage', op.toLowerCase(), live ? 'reachable' : 'unreachable'],
      properties: { operator: op, reachable: live },
    });
  }
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: anyLive,
    description: 'Regional grid outage portals — 9 operators outside TEPCO',
  });
}
