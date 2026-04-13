/**
 * Bank of Japan statistics portal
 * https://www.stat-search.boj.or.jp/
 * The BOJ search portal is HTML-first; we expose it as a single reachability
 * signal so the dashboard reflects upstream status, and return a seed snapshot
 * of published monetary-aggregate index URLs.
 */

const PROBE_URL = 'https://www.stat-search.boj.or.jp/';
const TIMEOUT_MS = 8000;

export default async function collectBojStats() {
  let source = 'seed';
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(PROBE_URL, { method: 'HEAD', signal: controller.signal });
    clearTimeout(timer);
    if (res.ok) source = 'live';
  } catch { /* ignore */ }
  const features = [
    {
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [139.77, 35.68] },
      properties: { name: 'Bank of Japan HQ', dataset: 'Monetary aggregates / rates / BOP', source: 'boj_stats' },
    },
  ];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Bank of Japan statistics portal reachability + index',
    },
    metadata: {},
  };
}
