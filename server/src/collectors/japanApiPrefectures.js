/**
 * japan-api prefectures REST API
 * https://japanapi.curtisbarnard.com/api/v1/prefectures
 */

import { fetchJson } from './_liveHelpers.js';

const API_URL = 'https://japanapi.curtisbarnard.com/api/v1/prefectures';
const TIMEOUT_MS = 10000;

const SEED_PREFS = [
  { name: 'Tokyo', lat: 35.69, lon: 139.69 },
  { name: 'Osaka', lat: 34.69, lon: 135.50 },
  { name: 'Hokkaido', lat: 43.06, lon: 141.35 },
];

export default async function collectJapanApiPrefectures() {
  let features = [];
  let source = 'live';
  const data = await fetchJson(API_URL, { timeoutMs: TIMEOUT_MS });
  const list = Array.isArray(data) ? data : (data?.data ?? data?.prefectures ?? []);
  for (const p of list) {
    const lat = p?.latitude ?? p?.lat;
    const lon = p?.longitude ?? p?.lon;
    if (lat == null || lon == null) continue;
    features.push({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [Number(lon), Number(lat)] },
      properties: {
        name: p.name ?? p.english ?? null,
        name_ja: p.japanese ?? p.nameJa ?? null,
        region: p.region ?? null,
        population: p.population ?? null,
        source: 'japan_api',
      },
    });
  }
  if (features.length === 0) {
    source = 'seed';
    features = SEED_PREFS.map(p => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [p.lon, p.lat] },
      properties: { name: p.name, source: 'japan_api_seed' },
    }));
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Community REST API for Japan prefectures',
    },
  };
}
