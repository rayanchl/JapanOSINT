/**
 * GSI address search (geocoder)
 * https://msearch.gsi.go.jp/address-search/AddressSearch?q=...
 */

const API_URL = 'https://msearch.gsi.go.jp/address-search/AddressSearch?q=' + encodeURIComponent('東京駅');
const TIMEOUT_MS = 8000;

export default async function collectGsiGeocode() {
  let features = [];
  let source = 'live';
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(API_URL, { signal: controller.signal });
    clearTimeout(timer);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.json();
    const list = Array.isArray(data) ? data : [];
    for (const r of list.slice(0, 20)) {
      const coords = r?.geometry?.coordinates;
      if (!Array.isArray(coords) || coords.length < 2) continue;
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [Number(coords[0]), Number(coords[1])] },
        properties: { title: r.properties?.title ?? null, source: 'gsi_geocode' },
      });
    }
    if (features.length === 0) throw new Error('empty');
  } catch {
    source = 'seed';
    features = [{
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [139.767, 35.681] },
      properties: { title: '東京駅 (seed)', source: 'gsi_seed' },
    }];
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'GSI address-search geocoder probe',
    },
    metadata: {},
  };
}
