/**
 * MOFA 海外安全ホームページ — travel advisories.
 * https://www.anzen.mofa.go.jp/
 *
 * JP government's view of foreign risk by country, with 4 alert levels
 * (1=注意, 2=不要不急, 3=渡航中止, 4=退避勧告). Inverted, this is the
 * JP intelligence community's read on world stability. The site exposes
 * an RSS endpoint that fans out per-region.
 */

import { intelEnvelope, feedItemToIntel } from '../utils/intelHelpers.js';
import { fetchFeed } from '../utils/rssCollectorTemplate.js';

const SOURCE_ID = 'mofa-travel-advisory';
const FEEDS = [
  'https://www.anzen.mofa.go.jp/rss/spotinfo.xml',
  'https://www.anzen.mofa.go.jp/rss/dangerinfo.xml',
  'https://www.anzen.mofa.go.jp/rss/info.xml',
];

export default async function collectMofaTravelAdvisory() {
  const all = [];
  let liveFeed = null;
  for (const url of FEEDS) {
    const entries = await fetchFeed(url, { timeoutMs: 10000 });
    if (entries.length > 0) {
      if (!liveFeed) liveFeed = url;
      for (const e of entries) all.push({ feed: url, entry: e });
    }
  }

  const items = all.slice(0, 200).map(({ feed, entry }) => {
    const it = feedItemToIntel(SOURCE_ID, entry, {
      language: 'ja',
      tags: ['advisory', 'mofa', 'travel', feed.endsWith('dangerinfo.xml') ? 'danger' : 'spot'],
    });
    it.properties = { ...(it.properties || {}), feed_url: feed };
    return it;
  });

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: items.length > 0,
    description: 'MOFA travel advisories (海外安全ホームページ RSS)',
    extraMeta: { live_feed: liveFeed },
  });
}
