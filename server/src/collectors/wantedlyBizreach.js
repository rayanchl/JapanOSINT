/**
 * Wantedly + Bizreach — JP professional profiles.
 * https://www.wantedly.com/  +  https://www.bizreach.jp/
 *
 * Public profile pages expose employer history + skills. Useful for
 * corporate-org-chart inference + employee mobility tracking
 * (esp. departures from gov / JSDF contractors).
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'wantedly-bizreach';
const PROBES = [
  ['Wantedly', 'https://www.wantedly.com/'],
  ['Bizreach', 'https://www.bizreach.jp/'],
];

export default async function collectWantedlyBizreach() {
  const items = [];
  let anyLive = false;
  for (const [op, url] of PROBES) {
    const live = await fetchHead(url).catch(() => false);
    if (live) anyLive = true;
    items.push({
      uid: intelUid(SOURCE_ID, op),
      title: `${op} — public profiles`,
      summary: 'Employer history + skills (ToS-rate-limited scraping)',
      link: url,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['profile', 'corporate', op.toLowerCase(), live ? 'reachable' : 'unreachable', 'tos-caveat'],
      properties: { operator: op, reachable: live, tos_caveat: true },
    });
  }
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: anyLive,
    description: 'Wantedly + Bizreach public-profile probe',
  });
}
