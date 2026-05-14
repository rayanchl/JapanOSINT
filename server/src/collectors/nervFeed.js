/**
 * NERV Disaster Prevention unified alert feed
 * Community/third-party alert aggregator replicating JMA multi-hazard pushes.
 */

import { fetchJson } from './_liveHelpers.js';

const API_URL = 'https://unii-api.nerv.app/v1/lib/alerts.json';
const TIMEOUT_MS = 8000;

export default async function collectNervFeed() {
  let features = [];
  let source = 'live';
  const data = await fetchJson(API_URL, { timeoutMs: TIMEOUT_MS, headers: { 'User-Agent': 'JapanOSINT' } });
  if (data) {
    const arr = Array.isArray(data) ? data : (data?.alerts ?? []);
    for (const a of arr) {
      const lat = a?.latitude ?? a?.lat ?? a?.location?.lat ?? null;
      const lon = a?.longitude ?? a?.lon ?? a?.location?.lon ?? null;
      if (lat == null || lon == null) continue;
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [Number(lon), Number(lat)] },
        properties: {
          kind: a.kind ?? a.type ?? null,
          headline: a.headline ?? a.title ?? null,
          severity: a.severity ?? null,
          issued_at: a.issued ?? a.time ?? null,
          source: 'nerv',
        },
      });
    }
  }
  if (features.length === 0) {
    source = 'seed';
    features = [
      { lat: 35.69, lon: 139.69, kind: 'seed', headline: 'NERV seed alert', severity: 'info' },
    ].map(a => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [a.lon, a.lat] },
      properties: { kind: a.kind, headline: a.headline, severity: a.severity, source: 'nerv_seed' },
    }));
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'NERV Disaster Prevention multi-hazard alerts',
    },
  };
}
