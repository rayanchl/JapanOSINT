/**
 * JPCERT/CC security advisories RSS/RDF — emits each advisory as an intel item.
 * https://www.jpcert.or.jp/rss/jpcert.rdf
 */

import { createRssCollector } from '../utils/rssCollectorTemplate.js';

export default createRssCollector({
  sourceId: 'jpcert-alerts',
  description: 'JPCERT/CC security advisories RSS/RDF',
  url: 'https://www.jpcert.or.jp/rss/jpcert.rdf',
  language: 'ja',
  tags: ['advisory', 'jpcert', 'cyber'],
});
