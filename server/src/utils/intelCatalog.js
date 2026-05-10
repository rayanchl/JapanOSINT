/**
 * Catalog of source ids that emit `kind:'intel'` (non-spatial) collector
 * output. Single source of truth shared between:
 *   - /api/intel/sources (lists every catalogued source whether or not
 *                         intel_items has rows for it yet)
 *   - /api/intel/sources/:id/run (manual trigger; gated to this set)
 *   - /api/layers STRIP_LAYER_IDS (these never appear as togglable map
 *                                   layers since they have no geometry)
 *
 * Adding a new intel collector? Add the source id here in addition to
 * registering the function in collectors/index.js.
 */
export const INTEL_SOURCE_IDS = [
  // Pattern-1 (formerly geometry:null)
  'certstream-jp', 'bird-makeup-jp', 'chan-5ch', 'fofa-jp',
  'github-leaks-jp', 'grayhat-buckets', 'greynoise-jp', 'hatena-bookmark',
  'houjin-bangou', 'mercari-trending', 'misskey-timeline', 'note-com-trending',
  'urlscan-jp', 'wayback-jp',
  // Pattern-2 (formerly placeholder Tokyo coords)
  'edinet-filings', 'boj-stats', 'data-go-jp-ckan', 'egov-laws',
  'geospatial-jp-ckan', 'kyodo-rss', 'nhk-world-rss', 'jcg-navarea', 'nict-atlas',
  'ripestat-jp',
  'wifi-hotspots-jcfw', 'wifi-hotspots-freespot',
  // Pattern-3 (per-item RSS feeds with placeholder coords)
  'jp-news-rss', 'nhk-news-rss', 'yahoo-news-jp-rss', 'ipa-alerts',
  'jpcert-alerts', 'phishing-feeds-jp', 'sans-isc-feeds', 'my-jvn',
];

export const INTEL_SOURCE_SET = new Set(INTEL_SOURCE_IDS);
