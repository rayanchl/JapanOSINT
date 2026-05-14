/**
 * TikTok JP — discover (hashtag + sound + location).
 * https://www.tiktok.com/discover
 *
 * Public discover surfaces JP-locale trending hashtags + sounds. Geo-tagged
 * posts come via the discovery layer. The legitimate TikTok Research API
 * is geofenced and requires application; this stub probes the public
 * portal and flags it for a Playwright fan-out.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'tiktok-jp-discover';
const PROBE_URL = 'https://www.tiktok.com/discover';

export default async function collectTiktokJpDiscover() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'TikTok JP discover',
      summary: 'JP-locale trending hashtags + sounds + geo-tagged posts',
      link: PROBE_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['tiktok', 'social', 'discover', live ? 'reachable' : 'unreachable'],
      properties: { reachable: live, tos_caveat: true },
    }],
    live,
    description: 'TikTok JP discover (trending hashtags / sounds / geo posts)',
  });
}
