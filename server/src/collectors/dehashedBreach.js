/**
 * DeHashed — breach / credential lookup (paid).
 * https://api.dehashed.com/search
 *
 * Aggregates many breaches keyed by domain. Query e.g. domain=mufg.jp
 * for credential exposures by JP corp.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'dehashed-breach';
const PROBE_URL = 'https://api.dehashed.com/';

export default async function collectDehashedBreach() {
  const hasKey = !!(process.env.DEHASHED_USER && process.env.DEHASHED_KEY);
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'DeHashed — credential breach lookup',
      summary: hasKey ? 'Configured — domain-keyed credential lookups' : 'Set DEHASHED_USER + DEHASHED_KEY to enable',
      link: PROBE_URL,
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['breach', 'credential', 'dehashed', live ? 'reachable' : 'unreachable', hasKey ? 'key-present' : 'key-missing'],
      properties: { reachable: live, requires_key: true, has_key: hasKey },
    }],
    live,
    description: 'DeHashed credential breach lookup (paid)',
  });
}
