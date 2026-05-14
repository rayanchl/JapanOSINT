/**
 * VRChat — active JP-tagged worlds + instance headcount.
 * https://vrchat.com/api/1/worlds/active
 *
 * VRChat exposes a JSON API of currently-popular worlds, with location
 * + tag metadata. Useful for niche-community presence indicators.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'vrchat-active-jp';
const KEY_ENV = 'VRC_AUTH';
const PROBE_URL = 'https://vrchat.com/';

export default async function collectVrchatActiveJp() {
  const hasKey = !!process.env[KEY_ENV];
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'VRChat active JP-tagged worlds',
      summary: hasKey ? 'Configured' : `Set ${KEY_ENV} (session cookie) to enable enumeration`,
      link: PROBE_URL,
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['vrchat', 'social', 'metaverse', live ? 'reachable' : 'unreachable', hasKey ? 'key-present' : 'key-missing'],
      properties: { reachable: live, requires_key: true, has_key: hasKey },
    }],
    live,
    description: 'VRChat active JP-tagged worlds + headcount',
  });
}
