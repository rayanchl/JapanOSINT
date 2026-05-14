/**
 * Yahoo! JAPAN News topics RSS — multi-category aggregation. Each headline
 * becomes one intel item tagged with its category.
 */

import { intelEnvelope, feedItemToIntel, intelHashKey, intelUid } from '../utils/intelHelpers.js';
import { fetchFeed } from '../utils/rssCollectorTemplate.js';

const SOURCE_ID = 'yahoo-news-jp-rss';

const FEEDS = [
  { cat: 'top-picks', url: 'https://news.yahoo.co.jp/rss/topics/top-picks.xml' },
  { cat: 'domestic',  url: 'https://news.yahoo.co.jp/rss/topics/domestic.xml' },
  { cat: 'world',     url: 'https://news.yahoo.co.jp/rss/topics/world.xml' },
  { cat: 'business',  url: 'https://news.yahoo.co.jp/rss/topics/business.xml' },
  { cat: 'it',        url: 'https://news.yahoo.co.jp/rss/topics/it.xml' },
];
const TIMEOUT_MS = 12000;
const PER_FEED_LIMIT = 25;

export default async function collectYahooNewsJpRss() {
  const lists = await Promise.all(FEEDS.map((f) => fetchFeed(f.url, { timeoutMs: TIMEOUT_MS })));
  const items = [];
  lists.forEach((entries, fi) => {
    const feed = FEEDS[fi];
    entries.slice(0, PER_FEED_LIMIT).forEach((e) => {
      const it = feedItemToIntel(SOURCE_ID, e, {
        language: 'ja',
        tags: ['news', 'yahoo', `category:${feed.cat}`],
      });
      it.uid = intelUid(SOURCE_ID, e.guid, e.link, intelHashKey(feed.cat, e.title, e.pubDate));
      it.properties = { ...(it.properties || {}), category: feed.cat, publisher: 'Yahoo! JAPAN News' };
      items.push(it);
    });
  });

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    description: 'Yahoo! JAPAN News — topics RSS feeds (top, domestic, world, business, IT)',
    extraMeta: { categories: FEEDS.map((f) => f.cat) },
  });
}
