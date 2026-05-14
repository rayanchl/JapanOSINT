/**
 * IPA (Information-technology Promotion Agency, Japan) — security advisories RSS.
 * Tries a small list of historical feed URLs and uses whichever responds.
 */

import { intelEnvelope, feedItemToIntel } from '../utils/intelHelpers.js';
import { fetchFeed } from '../utils/rssCollectorTemplate.js';

const SOURCE_ID = 'ipa-alerts';
const FEEDS = [
  'https://www.ipa.go.jp/security/alert-rss.rdf',
  'https://www.ipa.go.jp/security/announce/alert.rss',
  'https://www.ipa.go.jp/security/security-alert.rss',
];
const TIMEOUT_MS = 8000;

export default async function collectIpaAlertsRss() {
  let entries = [];
  let liveFeed = null;
  for (const url of FEEDS) {
    entries = await fetchFeed(url, { timeoutMs: TIMEOUT_MS });
    if (entries.length) { liveFeed = url; break; }
  }

  const items = entries.slice(0, 100).map((e) => feedItemToIntel(SOURCE_ID, e, {
    language: 'ja',
    tags: ['advisory', 'ipa', 'cyber'],
  }));

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: items.length > 0,
    description: 'IPA Japan security advisories RSS',
    extraMeta: { live_feed: liveFeed },
  });
}
