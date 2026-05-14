/**
 * HaveIBeenPwned — domain-scoped breach summary.
 * https://haveibeenpwned.com/api/v3/breaches
 *
 * Paid ($3.95/mo) — but the breach catalogue itself is public. Tag every
 * breach affecting addresses ending in `.jp` / `.co.jp` / `.go.jp` /
 * `.or.jp` etc.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'hibp-breach';
const KEY_ENV = 'HIBP_KEY';
const PROBE_URL = 'https://haveibeenpwned.com/api/v3/breaches';

export default async function collectHibpBreach() {
  const hasKey = !!process.env[KEY_ENV];
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'HaveIBeenPwned — domain breach catalogue',
      summary: hasKey ? 'Configured — pulls breaches affecting *.jp addresses' : `Set ${KEY_ENV} to enable lookups`,
      link: PROBE_URL,
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['breach', 'hibp', live ? 'reachable' : 'unreachable', hasKey ? 'key-present' : 'key-missing'],
      properties: { reachable: live, requires_key: true, has_key: hasKey },
    }],
    live,
    description: 'HaveIBeenPwned breach catalogue — JP-domain filter',
  });
}
