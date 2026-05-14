/**
 * Yahoo! Chiebukuro — JP-locale Q&A.
 * https://chiebukuro.yahoo.co.jp/
 *
 * High-volume JP-language Q&A; useful for geographical / institutional /
 * incident OSINT (people ask very specific questions with locations and
 * proper names).
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'yahoo-chiebukuro';
const PROBE_URL = 'https://chiebukuro.yahoo.co.jp/';

export default async function collectYahooChiebukuro() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'Yahoo! Chiebukuro — JP Q&A',
      summary: 'High-volume JP-locale Q&A — frequent mentions of locations + proper names',
      link: PROBE_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['social', 'qa', 'yahoo', live ? 'reachable' : 'unreachable'],
      properties: { operator: 'Yahoo! Japan', reachable: live },
    }],
    live,
    description: 'Yahoo! Chiebukuro Q&A',
  });
}
