/**
 * MHLW Health Statistics collector (mhlw-health).
 *
 * MHLW's health-statistics portal (https://www.mhlw.go.jp/toukei/) publishes
 * surveys as downloadable Excel/PDF, not a structured JSON/CSV API. There is
 * no key-free machine endpoint to parse reliably, so this collector performs
 * a best-effort reachability probe and emits a single intel index item
 * recording portal status. No fabricated statistics are ever emitted; if the
 * portal is unreachable the item is tagged `unreachable` and `live:false`.
 *
 * UNVERIFIED: no stable parseable dataset URL — probe-only by design.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'mhlw-health';
const PROBE_URL = 'https://www.mhlw.go.jp/toukei/';

export default async function collectMhlwHealth() {
  const reachable = await fetchHead(PROBE_URL);
  const items = [{
    uid: intelUid(SOURCE_ID, 'portal'),
    title: 'MHLW Health Statistics portal',
    summary: '人口動態統計・患者調査・国民健康・栄養調査ほか',
    body: 'Ministry of Health, Labour and Welfare statistics portal. Datasets are published as Excel/PDF; no stable machine API to parse.',
    link: PROBE_URL,
    language: 'ja',
    published_at: new Date().toISOString(),
    tags: ['health', 'statistics', reachable ? 'reachable' : 'unreachable'],
    properties: {
      org: 'MHLW',
      datasets: ['Vital statistics', 'Patient survey', 'National Health & Nutrition Survey'],
      reachable,
    },
  }];

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: false,
    extraMeta: { source: reachable ? 'mhlw_health_probe' : 'mhlw_health_unavailable', probe: true, reachable },
    description: 'MHLW health-statistics portal reachability + index (probe-only)',
  });
}
