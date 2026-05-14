/**
 * MSIL Umishiru (海しる) - JCG Maritime Domain Awareness public APIs
 * https://portal.msil.go.jp/apis
 * Requires subscription key (trial key available on portal); honour UMISHIRU_API_KEY env var.
 * Without a key we fall back to seed port positions.
 */

const TIMEOUT_MS = 10000;
const API_KEY = process.env.UMISHIRU_API_KEY;

const SEED_PORTS = [
  { name: '東京港', lat: 35.61, lon: 139.77 },
  { name: '横浜港', lat: 35.45, lon: 139.66 },
  { name: '名古屋港', lat: 35.08, lon: 136.88 },
  { name: '大阪港', lat: 34.65, lon: 135.43 },
  { name: '神戸港', lat: 34.68, lon: 135.18 },
  { name: '博多港', lat: 33.60, lon: 130.40 },
  { name: '那覇港', lat: 26.21, lon: 127.67 },
  { name: '苫小牧港', lat: 42.62, lon: 141.62 },
];

export default async function collectMsilUmishiru() {
  let features = [];
  let source = 'seed';
  if (API_KEY) {
    try {
      const controller = new AbortController();
      const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
      const url = 'https://portal.msil.go.jp/api/v1/ports?bbox=122,24,146,46&limit=1000';
      const res = await fetch(url, { signal: controller.signal, headers: { 'Ocp-Apim-Subscription-Key': API_KEY } });
      clearTimeout(timer);
      if (res.ok) {
        const data = await res.json();
        const list = Array.isArray(data) ? data : (data?.features ?? data?.items ?? []);
        for (const p of list) {
          const lat = p?.lat ?? p?.geometry?.coordinates?.[1];
          const lon = p?.lon ?? p?.geometry?.coordinates?.[0];
          if (lat == null || lon == null) continue;
          features.push({
            type: 'Feature',
            geometry: { type: 'Point', coordinates: [Number(lon), Number(lat)] },
            properties: { name: p.name ?? p.properties?.name ?? null, source: 'msil_umishiru' },
          });
        }
        if (features.length) source = 'live';
      }
    } catch { /* fall through to seed */ }
  }
  if (features.length === 0) {
    features = SEED_PORTS.map(p => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [p.lon, p.lat] },
      properties: { name: p.name, source: 'msil_seed' },
    }));
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'MSIL Umishiru ports (JCG MDA)',
      key_required: true,
    },
  };
}
