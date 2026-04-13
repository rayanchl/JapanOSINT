/**
 * e-Gov national law search API
 * https://laws.e-gov.go.jp/api/1/lawlists/1
 * Anonymous. Returns XML - we parse minimally to count laws.
 */

const API_URL = 'https://laws.e-gov.go.jp/api/1/lawlists/1';
const TIMEOUT_MS = 10000;

export default async function collectEgovLaws() {
  let source = 'live';
  let count = 0;
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(API_URL, { signal: controller.signal });
    clearTimeout(timer);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const text = await res.text();
    // crude count of <LawName> tags
    const matches = text.match(/<LawName\b/g) ?? [];
    count = matches.length;
    if (count === 0) throw new Error('empty');
  } catch {
    source = 'seed';
    count = 10000;
  }
  const features = [{
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [139.75, 35.67] },
    properties: { name: 'e-Gov 法令検索', law_count: count, source: source === 'live' ? 'egov_laws' : 'egov_seed' },
  }];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'e-Gov national law & ordinance catalog',
    },
    metadata: {},
  };
}
