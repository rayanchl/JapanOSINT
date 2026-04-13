/**
 * NICT Atlas (NICTER darknet sensor)
 * https://www.nicter.jp/atlas/
 * HTML-first; we use a reachability probe to feed status.
 */

const PROBE_URL = 'https://www.nicter.jp/atlas/';
const TIMEOUT_MS = 8000;

export default async function collectNictAtlas() {
  let source = 'seed';
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(PROBE_URL, { method: 'HEAD', signal: controller.signal });
    clearTimeout(timer);
    if (res.ok) source = 'live';
  } catch { /* ignore */ }
  const features = [{
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [139.486, 35.707] }, // NICT Koganei HQ
    properties: { name: 'NICT NICTER Atlas', sensor_type: 'darknet', source: source === 'live' ? 'nict' : 'nict_seed' },
  }];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'NICT NICTER darknet sensor visualization',
    },
    metadata: {},
  };
}
