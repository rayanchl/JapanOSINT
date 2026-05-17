/**
 * MLIT XRAIN high-resolution rainfall radar (xrain-radar).
 *
 * XRAIN (国土交通省 XバンドMPレーダ雨量情報) is distributed via the MLIT
 * "国土交通省 防災情報提供センター" portal. There is no documented public
 * JSON/GeoJSON nowcast endpoint — the live product is served as map tiles
 * behind the interactive viewer (www.river.go.jp / x-rain). We probe the
 * official portal for reachability and emit a single honest intel pointer;
 * no fabricated rainfall cells are ever produced.
 *
 * Endpoint UNVERIFIED for bulk JSON — best-effort reachability probe only.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'xrain-radar';
const PORTAL_URL = 'https://www.river.go.jp/x/krademapcommon/xrain/index.html';

export default async function collectXrainRadar() {
  let live = false;
  try { live = await fetchHead(PORTAL_URL); } catch { live = false; }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'MLIT XRAIN X-band rainfall radar',
      summary: 'High-resolution (250 m / 1 min) MP-radar rainfall product, tile-served via the MLIT disaster-information portal',
      link: PORTAL_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['weather', 'radar', 'rainfall', 'mlit', live ? 'reachable' : 'unreachable'],
      properties: { operator: '国土交通省', reachable: live },
    }],
    live,
    description: 'MLIT XRAIN X-band MP rainfall radar portal (no public JSON nowcast endpoint)',
  });
}
