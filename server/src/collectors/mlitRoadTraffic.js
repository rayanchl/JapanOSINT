/**
 * MLIT road traffic census / regulation (mlit-road-traffic).
 *
 * MLIT's live road-traffic regulation feed (道路交通規制情報, 道路情報便覧)
 * is distributed via the regional 道路情報 portals and the national
 * "道路規制情報提供システム" — no documented nationwide public JSON API.
 * We probe the MLIT road-information portal for reachability and emit a
 * single honest intel pointer. No fabricated traffic points.
 *
 * Endpoint UNVERIFIED for bulk JSON — best-effort reachability probe only.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'mlit-road-traffic';
const PORTAL_URL = 'https://www.mlit.go.jp/road/road_e/index.html';

export default async function collectMlitRoadTraffic() {
  let live = false;
  try { live = await fetchHead(PORTAL_URL); } catch { live = false; }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'MLIT road traffic / regulation information',
      summary: 'National road-regulation and traffic-census information distributed via MLIT regional road portals (no public nationwide JSON API)',
      link: PORTAL_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['transport', 'road', 'traffic', 'mlit', live ? 'reachable' : 'unreachable'],
      properties: { operator: '国土交通省 道路局', reachable: live },
    }],
    live,
    description: 'MLIT road traffic / regulation portal (no public nationwide JSON feed)',
  });
}
