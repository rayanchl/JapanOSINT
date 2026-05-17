/**
 * Food poisoning reports collector (food-poisoning).
 *
 * MHLW publishes 食中毒統計 (food-poisoning statistics) at
 * https://www.mhlw.go.jp/stf/seisakunitsuite/bunya/kenkou_iryou/shokuhin/
 * syokuchu/ as annual Excel/PDF summaries plus an incident database that is
 * a server-rendered search form, not a key-free JSON API. This collector
 * performs a best-effort reachability probe and emits a single intel index
 * item. No fabricated incident records are ever emitted; unreachable →
 * live:false.
 *
 * UNVERIFIED: no stable machine endpoint for incident rows — probe-only.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'food-poisoning';
const PROBE_URL = 'https://www.mhlw.go.jp/stf/seisakunitsuite/bunya/kenkou_iryou/shokuhin/syokuchu/index.html';

export default async function collectFoodPoisoning() {
  const reachable = await fetchHead(PROBE_URL);
  const items = [{
    uid: intelUid(SOURCE_ID, 'portal'),
    title: 'MHLW food-poisoning statistics',
    summary: '食中毒統計調査・食中毒事件一覧',
    body: 'MHLW food-poisoning statistics (食中毒統計). Annual summaries and an incident database are published as Excel/PDF / a server-side search form; no stable JSON API.',
    link: PROBE_URL,
    language: 'ja',
    published_at: new Date().toISOString(),
    tags: ['health', 'food-safety', reachable ? 'reachable' : 'unreachable'],
    properties: {
      org: 'MHLW',
      dataset: 'Food-poisoning statistics (食中毒統計)',
      reachable,
    },
  }];

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: false,
    extraMeta: { source: reachable ? 'food_poisoning_probe' : 'food_poisoning_unavailable', probe: true, reachable },
    description: 'MHLW food-poisoning statistics index reachability (probe-only)',
  });
}
