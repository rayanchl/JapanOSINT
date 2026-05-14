/**
 * NHK World English news feed — emits each article as an intel item.
 */

import { createRssCollector } from '../utils/rssCollectorTemplate.js';

export default createRssCollector({
  sourceId: 'nhk-world-rss',
  description: 'NHK World English news feed',
  url: 'https://www3.nhk.or.jp/nhkworld/en/news/feeds/',
  language: 'en',
  tags: ['news', 'nhk-world'],
});
