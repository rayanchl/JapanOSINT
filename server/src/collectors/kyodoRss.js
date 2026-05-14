/**
 * Kyodo News English RSS — emits each article as an intel item (kind:'intel').
 */

import { createRssCollector } from '../utils/rssCollectorTemplate.js';

export default createRssCollector({
  sourceId: 'kyodo-rss',
  description: 'Kyodo News English RSS — domestic and international news wire',
  url: 'https://english.kyodonews.net/rss/news.xml',
  language: 'en',
  tags: ['news', 'kyodo'],
});
