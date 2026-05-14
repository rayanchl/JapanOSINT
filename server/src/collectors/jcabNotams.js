/**
 * JCAB NOTAMs — Japan Civil Aviation Bureau notices to airmen.
 * https://aim-naviserv.mlit.go.jp/aimjp/web/notam.html
 *
 * NOTAMs surface VIP-state-visit TFRs, naval-exercise restricted zones,
 * disaster-response no-fly zones — often *before* news. The portal is an
 * HTML form that requires a POST to retrieve the actual NOTAM listing, so
 * we record portal status + one rolling intel item; the body can be
 * fleshed out with a Playwright pass later.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'jcab-notams';
const PROBE_URL = 'https://aim-naviserv.mlit.go.jp/aimjp/web/notam.html';

export default async function collectJcabNotams() {
  const live = await fetchHead(PROBE_URL);
  const items = [{
    uid: intelUid(SOURCE_ID, 'notam-portal'),
    title: 'JCAB NOTAMs (notices to airmen)',
    summary: 'Japan Civil Aviation Bureau NOTAM portal — temporary flight restrictions, exercise areas, VIP transit corridors',
    link: PROBE_URL,
    language: 'ja',
    published_at: new Date().toISOString(),
    tags: ['aviation', 'notam', 'tfr', 'mlit', live ? 'reachable' : 'unreachable'],
    properties: { operator: 'MLIT JCAB AIM Naviservice', reachable: live },
  }];

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live,
    description: 'JCAB NOTAMs — notices to airmen (TFRs, exercise zones, VIP transit)',
  });
}
