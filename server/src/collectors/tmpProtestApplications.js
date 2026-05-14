/**
 * Tokyo Metropolitan Police (警視庁) traffic-restriction / protest
 * application listings — 道路使用許可・集団行進等.
 * https://www.keishicho.metro.tokyo.lg.jp/
 *
 * TMP publishes upcoming protest routes, march plans, and parade
 * applications (集団行進等の届出) as PDF lists. Each entry has date,
 * time-window, start/end coordinates by way of address. Predictive
 * layer: where riot police will deploy in the next 48 h.
 *
 * The TMP page is an HTML portal that links per-incident PDFs; we record
 * portal status and let a Playwright pass enumerate the per-day PDFs.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'tmp-protests';
const PORTAL_URL = 'https://www.keishicho.metro.tokyo.lg.jp/kotsu/jiko/koutsu_kisei/index.html';

export default async function collectTmpProtestApplications() {
  const live = await fetchHead(PORTAL_URL);
  const items = [{
    uid: intelUid(SOURCE_ID, 'tmp-protest-portal'),
    title: 'Tokyo Metropolitan Police — protest / parade applications',
    summary: '道路使用許可・集団行進等 published in advance (predicts riot-police mobilizations)',
    link: PORTAL_URL,
    language: 'ja',
    published_at: new Date().toISOString(),
    tags: ['protest', 'police', 'tokyo', 'tmp', live ? 'reachable' : 'unreachable'],
    properties: { operator: '警視庁 交通部', reachable: live },
  }];

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live,
    description: 'TMP protest / parade applications — 道路使用許可・集団行進等',
  });
}
