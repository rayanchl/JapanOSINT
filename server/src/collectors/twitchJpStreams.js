/**
 * Twitch JP — live streams by language=ja.
 * https://api.twitch.tv/helix/streams
 *
 * Twitch helix API needs an app token. We probe the portal here; the
 * scheduler pulls live streams with `?language=ja` and surfaces game +
 * viewer count.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

const SOURCE_ID = 'twitch-jp-streams';
const KEY_ENV = 'TWITCH_CLIENT_ID';
const PROBE_URL = 'https://www.twitch.tv/directory/all/ja';

export default async function collectTwitchJpStreams() {
  const hasKey = !!(envFor(KEY_ENV) && envFor('TWITCH_CLIENT_SECRET'));
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'Twitch JP live streams (language=ja)',
      summary: hasKey ? 'Configured' : `Set ${KEY_ENV} + TWITCH_CLIENT_SECRET to enable streams pull`,
      link: PROBE_URL,
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['twitch', 'live-stream', 'social', live ? 'reachable' : 'unreachable', hasKey ? 'key-present' : 'key-missing'],
      properties: { reachable: live, requires_key: true, has_key: hasKey },
    }],
    live: false, // probe-only: reachability ping emits no real data records
    extraMeta: { probe: true, reachable: live },
    description: 'Twitch JP live streams (language=ja) via helix',
  });
}
