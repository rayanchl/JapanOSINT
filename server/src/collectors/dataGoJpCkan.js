/**
 * data.go.jp CKAN catalog
 * https://www.data.go.jp/data/api/action/package_search
 */

const API_URL = 'https://www.data.go.jp/data/api/action/package_search?rows=10';
const TIMEOUT_MS = 10000;

export default async function collectDataGoJpCkan() {
  let source = 'live';
  let count = 0;
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(API_URL, { signal: controller.signal });
    clearTimeout(timer);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.json();
    count = data?.result?.count ?? 0;
    if (count === 0) throw new Error('empty');
  } catch {
    source = 'seed';
    count = 30000;
  }
  const features = [{
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [139.75, 35.67] },
    properties: { name: 'data.go.jp CKAN', total_packages: count, source: source === 'live' ? 'data_go_jp' : 'data_go_jp_seed' },
  }];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Federated Japan government open-data CKAN catalog',
    },
    metadata: {},
  };
}
