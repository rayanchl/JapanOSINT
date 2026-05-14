/**
 * JPO J-PlatPat — Japan Patent Office search portal.
 * https://www.j-platpat.inpit.go.jp/
 *
 * Patents + utility models + designs + trademarks. REST exists via INPIT's
 * official API gateway, with a free anonymous tier (per-IP throttled).
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'jpo-jplatpat';
const PROBE_URL = 'https://www.j-platpat.inpit.go.jp/';

export default async function collectJpoPatents() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'JPO J-PlatPat (patents / utility / designs / trademarks)',
      summary: 'Japan Patent Office — search portal, filings by IPC class + applicant',
      link: PROBE_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['patent', 'jpo', 'inpit', live ? 'reachable' : 'unreachable'],
      properties: { operator: 'INPIT', reachable: live },
    }],
    live,
    description: 'JPO J-PlatPat — patents / utility models / designs / trademarks',
  });
}
