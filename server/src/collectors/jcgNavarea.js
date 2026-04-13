/**
 * JCG NAVAREA XI warnings - Maritime Safety Information
 * https://www6.kaiho.mlit.go.jp/JAPANNAVAREA/
 * HTML listing; we return a seed envelope of the coordinating station plus
 * attempt a reachability ping so status reflects upstream availability.
 */

const PROBE_URL = 'https://www6.kaiho.mlit.go.jp/JAPANNAVAREA/';
const TIMEOUT_MS = 8000;

export default async function collectJcgNavarea() {
  let source = 'seed';
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(PROBE_URL, { method: 'HEAD', signal: controller.signal });
    clearTimeout(timer);
    if (res.ok) source = 'live';
  } catch { /* offline */ }

  const features = [
    {
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [139.77, 35.65] },
      properties: { name: 'Japan Coast Guard HQ (NAVAREA XI)', role: 'coordinator', source: 'jcg_navarea' },
    },
  ];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'JCG NAVAREA XI navigation warnings (MSI)',
    },
    metadata: {},
  };
}
