/**
 * Tor Exit Nodes Collector
 * Tries Tor onionoo API for live exit relays in Japan, falls back to seed.
 */

const ONIONOO_URL = 'https://onionoo.torproject.org/details?country=jp&flag=Exit';

const SEED_RELAYS = [
  // Approximate seed of historic Tor exit relays in Japan; replaced by live data when API responds.
  { nickname: 'jpExit01', lat: 35.6900, lon: 139.7000, bandwidth_kbs: 5000 },
  { nickname: 'tokyoRelay', lat: 35.6700, lon: 139.7600, bandwidth_kbs: 12000 },
  { nickname: 'osakaTor', lat: 34.6913, lon: 135.5023, bandwidth_kbs: 8000 },
  { nickname: 'sapporoExit', lat: 43.0640, lon: 141.3469, bandwidth_kbs: 3000 },
  { nickname: 'fukuokaTor', lat: 33.5904, lon: 130.4017, bandwidth_kbs: 4000 },
  { nickname: 'nagoyaTor', lat: 35.1814, lon: 136.9067, bandwidth_kbs: 3500 },
  { nickname: 'jp_relay_1', lat: 35.7000, lon: 139.6900, bandwidth_kbs: 7500 },
  { nickname: 'jp_relay_2', lat: 35.6800, lon: 139.7900, bandwidth_kbs: 6200 },
  { nickname: 'jp_exit_kyoto', lat: 35.0114, lon: 135.7681, bandwidth_kbs: 4500 },
  { nickname: 'tor_jp_csu', lat: 35.6800, lon: 139.7700, bandwidth_kbs: 9000 },
];

async function tryOnionoo() {
  try {
    const ctrl = new AbortController();
    const timeout = setTimeout(() => ctrl.abort(), 10000);
    const res = await fetch(ONIONOO_URL, {
      signal: ctrl.signal,
      headers: { Accept: 'application/json' },
    });
    clearTimeout(timeout);
    if (!res.ok) return null;
    const data = await res.json();
    const relays = data?.relays || [];
    if (!relays.length) return null;
    return relays
      .filter((r) => r.latitude != null && r.longitude != null)
      .map((r) => ({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [r.longitude, r.latitude] },
        properties: {
          relay_id: r.fingerprint,
          nickname: r.nickname,
          country: r.country?.toUpperCase() || 'JP',
          city: r.city_name,
          as_name: r.as_name,
          bandwidth_kbs: Math.round((r.observed_bandwidth || 0) / 1000),
          running: !!r.running,
          flags: (r.flags || []).join(','),
          source: 'onionoo',
        },
      }));
  } catch {
    return null;
  }
}

function generateSeedData() {
  return SEED_RELAYS.map((r, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [r.lon, r.lat] },
    properties: {
      relay_id: `TOR_${String(i + 1).padStart(5, '0')}`,
      nickname: r.nickname,
      country: 'JP',
      bandwidth_kbs: r.bandwidth_kbs,
      running: true,
      flags: 'Exit,Running',
      source: 'tor_exit_seed',
    },
  }));
}

export default async function collectTorExitNodes() {
  let features = await tryOnionoo();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'tor_exit_nodes',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Tor exit relays in Japan via Tor Project onionoo API',
    },
    metadata: {},
  };
}
