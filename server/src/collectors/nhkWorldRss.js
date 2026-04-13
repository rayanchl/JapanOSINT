/**
 * NHK World English news Atom feed
 */

const API_URL = 'https://www3.nhk.or.jp/nhkworld/en/news/feeds/';
const TIMEOUT_MS = 8000;

export default async function collectNhkWorldRss() {
  let source = 'live';
  let count = 0;
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(API_URL, { signal: controller.signal });
    clearTimeout(timer);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const text = await res.text();
    count = (text.match(/<entry>|<item>/g) ?? []).length;
  } catch {
    source = 'seed';
    count = 5;
  }
  const features = [{
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [139.75, 35.67] },
    properties: { outlet: 'NHK World EN', item_count: count, source: source === 'live' ? 'nhk_world' : 'nhk_world_seed' },
  }];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'NHK World English news feed',
    },
    metadata: {},
  };
}
