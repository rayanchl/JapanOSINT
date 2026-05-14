/**
 * Helpers for the seven RSS/Atom feed collectors. They all share three steps:
 *
 *   1. fetch the feed XML with a timeout
 *   2. parse via parseFeed → array of entries
 *   3. wrap into the canonical intel envelope
 *
 * `fetchFeed` covers step 1 (and never throws). `createRssCollector` is the
 * one-liner factory for the simple "single feed → intel envelope" case.
 *
 * Multi-feed collectors that need per-feed metadata (Yahoo News, jpNewsRss)
 * can call `fetchFeed` directly without going through the factory.
 */

import { intelEnvelope, parseFeed, feedItemToIntel } from './intelHelpers.js';

/**
 * Fetch + parse one RSS/Atom feed. Returns an array of feed entries (possibly
 * empty). Never throws — transient errors yield an empty array so the caller
 * can fall back to last-known-good intel rows downstream.
 */
export async function fetchFeed(url, { timeoutMs = 8000, headers = {} } = {}) {
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), timeoutMs);
    const res = await fetch(url, {
      signal: ctrl.signal,
      headers: {
        accept: 'application/rss+xml,*/*',
        'user-agent': 'japanosint-collector',
        ...headers,
      },
    });
    clearTimeout(timer);
    if (!res.ok) return [];
    return parseFeed(await res.text());
  } catch { return []; }
}

/**
 * Build a collector for the single-feed case. The collector is a plain async
 * function returning the canonical intel envelope, no further wiring needed.
 *
 * @param {object} opts
 * @param {string} opts.sourceId
 * @param {string} opts.description
 * @param {string} opts.url
 * @param {string} [opts.language='ja']
 * @param {string[]} [opts.tags=[]]
 * @param {number} [opts.timeoutMs=8000]
 * @param {number} [opts.limit=null] - optional cap on items per fetch
 * @returns {() => Promise<object>}
 */
export function createRssCollector({
  sourceId,
  description,
  url,
  language = 'ja',
  tags = [],
  timeoutMs = 8000,
  limit = null,
}) {
  if (!sourceId) throw new Error('createRssCollector: sourceId required');
  if (!url) throw new Error('createRssCollector: url required');

  return async function collect() {
    const entries = await fetchFeed(url, { timeoutMs });
    const sliced = limit != null ? entries.slice(0, limit) : entries;
    const items = sliced.map((e) => feedItemToIntel(sourceId, e, { language, tags }));
    return intelEnvelope({
      sourceId,
      items,
      live: items.length > 0,
      description,
    });
  };
}
