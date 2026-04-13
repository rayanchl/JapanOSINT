/**
 * Kyodo News English RSS
 */

const API_URL = 'https://english.kyodonews.net/rss/news.xml';
const TIMEOUT_MS = 8000;

export default async function collectKyodoRss() {
  let source = 'live';
  let count = 0;
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(API_URL, { signal: controller.signal });
    clearTimeout(timer);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const text = await res.text();
    count = (text.match(/<item>/g) ?? []).length;
  } catch {
    source = 'seed';
    count = 5;
  }
  const features = [{
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [139.764, 35.678] }, // Kyodo HQ Tokyo
    properties: { outlet: 'Kyodo News (EN)', item_count: count, source: source === 'live' ? 'kyodo' : 'kyodo_seed' },
  }];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Kyodo News English RSS feed',
    },
    metadata: {},
  };
}
