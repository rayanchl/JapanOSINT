/**
 * Wolfx JMA Earthquake list (last 50)
 * https://api.wolfx.jp/jma_eqlist.json
 */

import { fetchJson } from './_liveHelpers.js';

const API_URL = 'https://api.wolfx.jp/jma_eqlist.json';
const TIMEOUT_MS = 8000;

function toFeature(key, eq) {
  const lat = Number(eq?.Latitude);
  const lon = Number(eq?.Longitude);
  const geocoded = Number.isFinite(lat) && Number.isFinite(lon);
  return {
    type: 'Feature',
    geometry: geocoded ? { type: 'Point', coordinates: [lon, lat] } : null,
    properties: {
      event_id: eq.EventID ?? key,
      magnitude: eq.Magunitude ?? eq.Magnitude ?? null,
      depth_km: eq.Depth ?? null,
      max_intensity: eq.MaxIntensity ?? null,
      place: eq.Hypocenter ?? null,
      time: eq.time ?? eq.Time ?? null,
      source: 'wolfx_eqlist',
    },
  };
}

export default async function collectWolfxEqlist() {
  let features = [];
  let source = 'live';
  const data = await fetchJson(API_URL, { timeoutMs: TIMEOUT_MS });
  if (data) {
    for (const [k, v] of Object.entries(data)) {
      if (typeof v !== 'object' || !v) continue;
      const f = toFeature(k, v);
      if (f) features.push(f);
    }
  }
  if (features.length === 0) source = 'seed';
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Wolfx JMA earthquake list (last 50)',
    },
  };
}
