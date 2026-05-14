/**
 * JPO Trademark search (商標検索).
 * https://www.j-platpat.inpit.go.jp/p1101
 *
 * Same INPIT portal as J-PlatPat but the trademark face — class, applicant,
 * status, image. Different cadence + index from the patent feed so registered
 * separately.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'jpo-trademarks';
const PROBE_URL = 'https://www.j-platpat.inpit.go.jp/p1101';

export default async function collectJpoTrademarks() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'JPO Trademark search (商標検索)',
      summary: 'INPIT trademark search — class, applicant, status',
      link: PROBE_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['trademark', 'jpo', 'inpit', live ? 'reachable' : 'unreachable'],
      properties: { operator: 'INPIT', reachable: live },
    }],
    live,
    description: 'JPO trademark search',
  });
}
