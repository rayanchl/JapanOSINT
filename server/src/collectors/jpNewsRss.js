/**
 * Aggregated JP news / government RSS bundle (English + Japanese).
 *
 * Free, no auth. Pulls multiple feeds in parallel; each article becomes an
 * intel item tagged with its publisher.
 */

import { intelEnvelope, feedItemToIntel, intelHashKey, intelUid } from '../utils/intelHelpers.js';
import { fetchFeed } from '../utils/rssCollectorTemplate.js';

const SOURCE_ID = 'jp-news-rss';

const FEEDS = [
  { name: 'Nikkei Asia',        url: 'https://asia.nikkei.com/rss/feed/nar',        lang: 'en' },
  { name: 'Japan Times',        url: 'https://www.japantimes.co.jp/feed/',          lang: 'en' },
  { name: 'NHK World (top)',    url: 'https://www3.nhk.or.jp/rss/news/cat0.xml',    lang: 'ja' },
  { name: 'Reuters Japan',      url: 'https://jp.reuters.com/rss',                  lang: 'ja' },
  { name: 'Kantei (PMO)',       url: 'https://japan.kantei.go.jp/rss.xml',          lang: 'en' },
  { name: 'NDL Press Releases', url: 'https://www.ndl.go.jp/en/news/index.rss',     lang: 'en' },
];
const TIMEOUT_MS = 12000;
const PER_FEED_LIMIT = 20;

export default async function collectJpNewsRss() {
  const lists = await Promise.all(FEEDS.map((f) => fetchFeed(f.url, { timeoutMs: TIMEOUT_MS })));
  const items = [];
  lists.forEach((entries, fi) => {
    const feed = FEEDS[fi];
    entries.slice(0, PER_FEED_LIMIT).forEach((e) => {
      const it = feedItemToIntel(SOURCE_ID, e, {
        language: feed.lang,
        tags: ['news', `publisher:${feed.name}`],
      });
      // Make uid include publisher so the same headline reposted across feeds
      // doesn't collide.
      it.uid = intelUid(SOURCE_ID, e.guid, e.link, intelHashKey(feed.name, e.title, e.pubDate));
      it.properties = { ...(it.properties || {}), publisher: feed.name };
      items.push(it);
    });
  });

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    description: 'Aggregated JP news / government RSS (Nikkei, Japan Times, Reuters JP, NHK World, Kantei, NDL)',
    extraMeta: { publishers: FEEDS.map((f) => f.name) },
  });
}
