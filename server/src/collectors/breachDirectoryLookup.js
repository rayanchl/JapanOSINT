/**
 * BreachDirectory.org — RapidAPI-backed breach lookup.
 * https://breachdirectory.org/
 *
 * Query email / username for breach hits across many indexed corpora.
 * Routed through RapidAPI's marketplace.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'breach-directory';
const KEY_ENV = 'RAPIDAPI_KEY';
const PROBE_URL = 'https://breachdirectory.org/';

export default async function collectBreachDirectory() {
  const hasKey = !!process.env[KEY_ENV];
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'BreachDirectory.org lookup',
      summary: hasKey ? 'Configured (RapidAPI)' : `Set ${KEY_ENV} (RapidAPI) to enable lookups`,
      link: PROBE_URL,
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['breach', 'rapidapi', live ? 'reachable' : 'unreachable', hasKey ? 'key-present' : 'key-missing'],
      properties: { reachable: live, requires_key: true, has_key: hasKey },
    }],
    live,
    description: 'BreachDirectory.org email/username breach lookup',
  });
}
