/**
 * NDB Open Data collector (ndb-open).
 *
 * MHLW's National Database (NDB) open data
 * (https://www.mhlw.go.jp/stf/seisakunitsuite/bunya/0000177182.html) is
 * published as bulk Excel workbooks per edition — there is no key-free
 * structured API. This collector performs a best-effort reachability probe
 * of the index page and emits a single intel index item. No fabricated
 * claims data is ever emitted; unreachable → `unreachable` tag + live:false.
 *
 * UNVERIFIED: Excel-only dataset, no machine API — probe-only by design.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'ndb-open';
const PROBE_URL = 'https://www.mhlw.go.jp/stf/seisakunitsuite/bunya/0000177182.html';

export default async function collectNdbOpen() {
  const reachable = await fetchHead(PROBE_URL);
  const items = [{
    uid: intelUid(SOURCE_ID, 'portal'),
    title: 'NDB Open Data (health-insurance claims)',
    summary: 'レセプト情報・特定健診等オープンデータ',
    body: 'MHLW National Database open data — aggregated health-insurance claims and specific health checkups, published as Excel workbooks per edition.',
    link: PROBE_URL,
    language: 'ja',
    published_at: new Date().toISOString(),
    tags: ['health', 'statistics', reachable ? 'reachable' : 'unreachable'],
    properties: {
      org: 'MHLW',
      dataset: 'NDB Open Data',
      format: 'xlsx',
      reachable,
    },
  }];

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: false,
    extraMeta: { source: reachable ? 'ndb_open_probe' : 'ndb_open_unavailable', probe: true, reachable },
    description: 'MHLW NDB open data index reachability (probe-only)',
  });
}
