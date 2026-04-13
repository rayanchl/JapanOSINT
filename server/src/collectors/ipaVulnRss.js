/**
 * IPA critical security alerts RSS
 * https://www.ipa.go.jp/security/announce/alert.rss
 */

const API_URL = 'https://www.ipa.go.jp/security/announce/alert.rss';
const TIMEOUT_MS = 8000;

export default async function collectIpaVulnRss() {
  let count = 0;
  let source = 'live';
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(API_URL, { signal: controller.signal });
    clearTimeout(timer);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const text = await res.text();
    count = (text.match(/<item>|<entry>/g) ?? []).length;
  } catch {
    source = 'seed';
    count = 5;
  }
  const features = [{
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [139.7476, 35.6664] }, // IPA Bunkyo
    properties: { issuer: 'IPA', item_count: count, source: source === 'live' ? 'ipa' : 'ipa_seed' },
  }];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'IPA critical security alerts RSS',
    },
    metadata: {},
  };
}
