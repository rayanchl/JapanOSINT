/**
 * NTT fiber (FLET'S) service / outage information (ntt-fiber).
 *
 * NTT East / NTT West publish FLET'S 光 service-area lookup and fault
 * (工事・故障情報) pages, but there is no documented public JSON/GeoJSON
 * coverage or outage API — area lookup is an interactive postal-code form.
 * We probe the NTT East and NTT West fault-information portals for
 * reachability and emit honest intel pointers. No fabricated coverage data.
 *
 * Endpoint UNVERIFIED for any public data feed — reachability probe only.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'ntt-fiber';
const PORTALS = [
  { op: 'NTT East', url: 'https://www.ntt-east.co.jp/info-st/' },
  { op: 'NTT West', url: 'https://www.ntt-west.co.jp/info_st/' },
];

export default async function collectNttFiber() {
  const items = [];
  let anyLive = false;

  for (const p of PORTALS) {
    let live = false;
    try { live = await fetchHead(p.url); } catch { live = false; }
    if (live) anyLive = true;
    items.push({
      uid: intelUid(SOURCE_ID, p.op),
      title: `${p.op} FLET'S fault / service information`,
      summary: 'FLET’S fiber service-area and fault information (interactive lookup; no public coverage/outage JSON API)',
      link: p.url,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['telecom', 'fiber', 'ntt', p.op.toLowerCase().replace(/\s+/g, '-'), live ? 'reachable' : 'unreachable'],
      properties: { operator: p.op, reachable: live, public_feed: false },
    });
  }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: anyLive,
    description: 'NTT East/West FLET’S fiber fault-information portals (no public coverage API)',
  });
}
