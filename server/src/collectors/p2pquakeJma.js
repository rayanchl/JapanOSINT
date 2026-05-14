/**
 * P2P Quake API - community JMA earthquake mirror
 * https://api.p2pquake.net/v2/history?codes=551
 * Free JSON, 60 req/min anonymous. Also exposes WebSocket at wss://api.p2pquake.net/v2/ws
 */

import { fetchJson } from './_liveHelpers.js';

const API_URL = 'https://api.p2pquake.net/v2/history?codes=551&limit=50';
const TIMEOUT_MS = 8000;

function buildFeature(item) {
  const h = item?.earthquake?.hypocenter;
  if (!h || h.latitude == null || h.longitude == null) return null;
  return {
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [h.longitude, h.latitude] },
    properties: {
      earthquake_id: item.id ?? null,
      magnitude: h.magnitude ?? null,
      depth_km: h.depth ?? null,
      max_intensity: item?.earthquake?.maxScale ?? null,
      timestamp: item?.earthquake?.time ?? item.time ?? null,
      place: h.name ?? null,
      tsunami_warning: item?.earthquake?.domesticTsunami && item.earthquake.domesticTsunami !== 'None',
      source: 'p2pquake',
    },
  };
}

export default async function collectP2pquakeJma() {
  let features = [];
  let source = 'live';
  const data = await fetchJson(API_URL, { timeoutMs: TIMEOUT_MS, headers: { 'User-Agent': 'JapanOSINT' } });
  const items = Array.isArray(data) ? data : [];
  features = items.map(buildFeature).filter(Boolean);
  if (features.length === 0) source = 'seed';
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'P2P Quake JMA earthquake mirror',
    },
  };
}
