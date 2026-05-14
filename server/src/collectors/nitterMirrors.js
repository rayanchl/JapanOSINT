/**
 * Nitter — public X/Twitter mirror network.
 * https://github.com/zedeus/nitter/wiki/Instances
 *
 * Nitter instances expose RSS for any X account + search; we probe a
 * known-public mirror and surface reachability + sample RSS URL pattern.
 * Used as a no-key alternative when `twitter-geo` paid API is unset.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'nitter-mirrors';
const INSTANCES = [
  'https://nitter.net/',
  'https://nitter.tiekoetter.com/',
  'https://nitter.privacydev.net/',
];

export default async function collectNitterMirrors() {
  const items = [];
  let anyLive = false;
  for (const url of INSTANCES) {
    const live = await fetchHead(url).catch(() => false);
    if (live) anyLive = true;
    items.push({
      uid: intelUid(SOURCE_ID, url),
      title: `Nitter mirror ${new URL(url).hostname}`,
      summary: 'X/Twitter RSS proxy — no API key required',
      link: url,
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['twitter', 'nitter', 'mirror', live ? 'reachable' : 'unreachable'],
      properties: { reachable: live, host: new URL(url).hostname },
    });
  }
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: anyLive,
    description: 'Nitter mirrors — keyless X/Twitter RSS proxy',
  });
}
