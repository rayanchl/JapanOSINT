/**
 * FSA registered crypto-asset exchange service providers.
 * https://www.fsa.go.jp/policy/virtual_currency02/
 *
 * Static HTML list maintained by FSA — name, registration number, HQ
 * address, services offered. Diff over time = entrants / suspensions.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'fsa-crypto-exchanges';
const PROBE_URL = 'https://www.fsa.go.jp/policy/virtual_currency02/';

export default async function collectFsaCryptoExchanges() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'FSA registered crypto exchange list',
      summary: 'Financial Services Agency — registered crypto-asset exchange service providers',
      link: PROBE_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['crypto', 'fsa', 'exchange', live ? 'reachable' : 'unreachable'],
      properties: { operator: '金融庁', reachable: live },
    }],
    live,
    description: 'FSA registered crypto exchanges list',
  });
}
