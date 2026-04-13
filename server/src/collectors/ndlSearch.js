/**
 * National Diet Library OpenSearch
 * https://iss.ndl.go.jp/api/opensearch
 */

const API_URL = 'https://iss.ndl.go.jp/api/opensearch?title=%E6%97%A5%E6%9C%AC&cnt=10';
const TIMEOUT_MS = 10000;

export default async function collectNdlSearch() {
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
    if (count === 0) throw new Error('empty');
  } catch {
    source = 'seed';
    count = 10;
  }
  const features = [{
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [139.744, 35.679] },
    properties: { name: '国立国会図書館', sample_hits: count, source: source === 'live' ? 'ndl' : 'ndl_seed' },
  }];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'National Diet Library bibliographic OpenSearch',
    },
    metadata: {},
  };
}
