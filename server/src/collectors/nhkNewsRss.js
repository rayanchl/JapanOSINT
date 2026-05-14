/**
 * NHK news top RSS — emits each headline as an intel item.
 * https://www3.nhk.or.jp/rss/news/cat0.xml
 */

import { createRssCollector } from '../utils/rssCollectorTemplate.js';

export default createRssCollector({
  sourceId: 'nhk-news-rss',
  description: 'NHK news RSS (cat0 main feed)',
  url: 'https://www3.nhk.or.jp/rss/news/cat0.xml',
  language: 'ja',
  tags: ['news', 'nhk'],
});
