/**
 * LeakCheck.io — multi-breach lookup.
 * https://leakcheck.io/api
 *
 * Paid; query by email / username / domain. JP-relevant queries: feed it
 * a list of high-value @co.jp / @go.jp domains and surface every breach
 * hit for the cyber-intel layer.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'leakcheck-breach';
const KEY_ENV = 'LEAKCHECK_KEY';
const PROBE_URL = 'https://leakcheck.io/';

export default async function collectLeakcheckBreach() {
  const hasKey = !!process.env[KEY_ENV];
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'LeakCheck.io — multi-breach lookup',
      summary: hasKey ? 'Configured' : `Set ${KEY_ENV} to enable lookups`,
      link: PROBE_URL,
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['breach', 'leakcheck', live ? 'reachable' : 'unreachable', hasKey ? 'key-present' : 'key-missing'],
      properties: { reachable: live, requires_key: true, has_key: hasKey },
    }],
    live,
    description: 'LeakCheck.io multi-breach lookup',
  });
}
