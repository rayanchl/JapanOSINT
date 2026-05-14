/**
 * IntelX — paste / leak intelligence search.
 * https://2.intelx.io/intelligent/search
 *
 * Free tier: ~50 search requests / IP / month. Searches across pastebins,
 * leaked DB archives, dark-web mentions. Filter by JP TLDs / ASN / kanji
 * corporate names for high-signal hits.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'intelx-leaks';
const KEY_ENV = 'INTELX_KEY';
const PROBE_URL = 'https://2.intelx.io/';

export default async function collectIntelxLeaks() {
  const hasKey = !!process.env[KEY_ENV];
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'IntelX — paste/leak search',
      summary: hasKey ? 'Configured — search across pastes/leaks/darknet for JP TLDs' : `Set ${KEY_ENV} to enable searches`,
      link: PROBE_URL,
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['breach', 'paste', 'intelx', live ? 'reachable' : 'unreachable', hasKey ? 'key-present' : 'key-missing'],
      properties: { reachable: live, requires_key: true, has_key: hasKey },
    }],
    live,
    description: 'IntelX paste / leak / darknet search (50 reqs/month free)',
  });
}
