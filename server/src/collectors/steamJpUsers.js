/**
 * Steam Community — JP-country profile enumeration.
 * https://steamcommunity.com/search/users
 *
 * Profiles tagged Country:Japan with recent activity are searchable.
 * Combined with profile name + avatar fingerprints, useful for OSINT
 * pivots on JP gamers + dox-targeting research.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'steam-jp-users';
const PROBE_URL = 'https://steamcommunity.com/search/users/?country=JP';

export default async function collectSteamJpUsers() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'Steam Community — JP profile search',
      summary: 'Country=JP profile enumeration via Steam Community search',
      link: PROBE_URL,
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['gaming', 'steam', 'profile-search', live ? 'reachable' : 'unreachable'],
      properties: { reachable: live },
    }],
    live,
    description: 'Steam Community JP profile search',
  });
}
