/**
 * JMA active typhoon track JSON endpoints (discovered via https://www.jma.go.jp/bosai/typhoon/data/)
 * When no active typhoons exist the index is empty - we return seed sample.
 */

const INDEX_URL = 'https://www.jma.go.jp/bosai/typhoon/data/targetTc.json';
const TIMEOUT_MS = 8000;

export default async function collectJmaTyphoonJson() {
  let features = [];
  let source = 'live';
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(INDEX_URL, { signal: controller.signal });
    clearTimeout(timer);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.json();
    const list = Array.isArray(data) ? data : [];
    for (const tc of list) {
      if (tc?.lat != null && tc?.lon != null) {
        features.push({
          type: 'Feature',
          geometry: { type: 'Point', coordinates: [Number(tc.lon), Number(tc.lat)] },
          properties: { name: tc.name ?? null, id: tc.id ?? null, source: 'jma_typhoon' },
        });
      }
    }
  } catch {
    source = 'seed';
  }
  if (features.length === 0 && source !== 'live') {
    features = [
      { lat: 20.0, lon: 130.0, name: 'SEED_TYPHOON_1', id: 'TC26S1' },
    ].map(t => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [t.lon, t.lat] },
      properties: { name: t.name, id: t.id, source: 'jma_typhoon_seed' },
    }));
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'JMA active typhoon tracks',
    },
    metadata: {},
  };
}
