/**
 * NIID/JIHS Influenza surveillance collector (influenza-surveillance).
 *
 * Japan's infectious-disease sentinel surveillance (IDWR / 感染症発生動向
 * 調査) is run by NIID, reorganised into JIHS in 2025. The flu-map data at
 * https://www.niid.go.jp/niid/ja/flu-map.html is rendered from per-week
 * Excel/CSV bulletins rather than a stable JSON API. We best-effort probe
 * the flu-map index and emit a single intel index item recording status.
 * No fabricated case counts are ever emitted; unreachable → live:false.
 *
 * UNVERIFIED: no stable machine endpoint for weekly sentinel CSV — the
 * collector degrades to a reachability probe rather than guessing a URL.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'influenza-surveillance';
const PROBE_URL = 'https://www.niid.go.jp/niid/ja/flu-map.html';

export default async function collectInfluenzaSurveillance() {
  const reachable = await fetchHead(PROBE_URL);
  const items = [{
    uid: intelUid(SOURCE_ID, 'flu-map'),
    title: 'NIID/JIHS influenza sentinel surveillance',
    summary: 'インフルエンザ定点当たり報告数 (感染症発生動向調査)',
    body: 'NIID/JIHS influenza sentinel surveillance (IDWR). Weekly per-prefecture sentinel reports are published as Excel/CSV bulletins; no stable JSON API.',
    link: PROBE_URL,
    language: 'ja',
    published_at: new Date().toISOString(),
    tags: ['health', 'influenza', 'surveillance', reachable ? 'reachable' : 'unreachable'],
    properties: {
      org: 'NIID / JIHS',
      dataset: 'Influenza sentinel surveillance (IDWR)',
      reachable,
    },
  }];

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: false,
    extraMeta: { source: reachable ? 'influenza_surveillance_probe' : 'influenza_surveillance_unavailable', probe: true, reachable },
    description: 'NIID/JIHS influenza sentinel surveillance index reachability (probe-only)',
  });
}
