/**
 * P2P Quake API - community JMA earthquake mirror
 * https://api.p2pquake.net/v2/history?codes=551
 * Free JSON, 60 req/min anonymous. Also exposes WebSocket at wss://api.p2pquake.net/v2/ws
 */

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

function getSeedData() {
  return [
    { lat: 35.6, lon: 139.7, mag: 3.5, depth: 30, name: '東京湾', scale: 20, time: '2026-04-12T10:00:00+09:00', id: 'seed_p2p_1' },
    { lat: 38.3, lon: 141.9, mag: 4.2, depth: 55, name: '宮城県沖', scale: 30, time: '2026-04-12T08:15:00+09:00', id: 'seed_p2p_2' },
    { lat: 33.2, lon: 131.6, mag: 3.0, depth: 10, name: '大分県中部', scale: 20, time: '2026-04-12T06:30:00+09:00', id: 'seed_p2p_3' },
  ].map(q => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [q.lon, q.lat] },
    properties: {
      earthquake_id: q.id,
      magnitude: q.mag,
      depth_km: q.depth,
      max_intensity: q.scale,
      timestamp: q.time,
      place: q.name,
      tsunami_warning: false,
      source: 'p2pquake_seed',
    },
  }));
}

export default async function collectP2pquakeJma() {
  let features = [];
  let source = 'live';
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(API_URL, { signal: controller.signal, headers: { 'User-Agent': 'JapanOSINT' } });
    clearTimeout(timer);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.json();
    const items = Array.isArray(data) ? data : [];
    features = items.map(buildFeature).filter(Boolean);
    if (features.length === 0) throw new Error('empty');
  } catch {
    features = getSeedData();
    source = 'seed';
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'P2P Quake JMA earthquake mirror',
    },
    metadata: {},
  };
}
