/**
 * Snusbase — combolist lookup (paid).
 * https://snusbase.com/v3/search
 *
 * Aggregator of many breach corpora keyed by email / domain. Same usage
 * pattern as DeHashed / LeakCheck — query by JP corp domain list.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'snusbase-breach';
const KEY_ENV = 'SNUSBASE_KEY';
const PROBE_URL = 'https://snusbase.com/';

export default async function collectSnusbaseBreach() {
  const hasKey = !!process.env[KEY_ENV];
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'Snusbase — combolist lookup',
      summary: hasKey ? 'Configured' : `Set ${KEY_ENV} to enable lookups`,
      link: PROBE_URL,
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['breach', 'combolist', 'snusbase', live ? 'reachable' : 'unreachable', hasKey ? 'key-present' : 'key-missing'],
      properties: { reachable: live, requires_key: true, has_key: hasKey },
    }],
    live,
    description: 'Snusbase — combolist / breach lookup',
  });
}
