/**
 * Cycling ports / docked bike-share stations (cycling-ports).
 *
 * Uses the real DOCOMO Bike Share GBFS station_information feed (served via
 * the public ODPT GBFS gateway). GBFS station_information.json gives the
 * physical "ports" (lat/lon, capacity) for the docomo-cycle networks.
 * Honest empty on failure — no fabricated ports.
 */

import { fetchJson } from './_liveHelpers.js';

// Real public ODPT GBFS discovery endpoints for DOCOMO Bike Share networks.
const NETWORKS = [
  { net: 'docomo-cycle-tokyo',    discovery: 'https://api-public.odpt.org/api/v4/gbfs/docomo-cycle-tokyo/gbfs.json' },
  { net: 'docomo-cycle-yokohama', discovery: 'https://api-public.odpt.org/api/v4/gbfs/docomo-cycle-yokohama/gbfs.json' },
  { net: 'docomo-cycle-osaka',    discovery: 'https://api-public.odpt.org/api/v4/gbfs/docomo-cycle-osaka/gbfs.json' },
];

async function loadStations(net) {
  try {
    const root = await fetchJson(net.discovery, { timeoutMs: 8000 });
    const feeds = root?.data?.ja?.feeds || root?.data?.en?.feeds || [];
    const byName = Object.fromEntries(feeds.map((f) => [f.name, f.url]));
    if (!byName.station_information) return [];
    const info = await fetchJson(byName.station_information, { timeoutMs: 8000 });
    return info?.data?.stations || [];
  } catch {
    return [];
  }
}

export default async function collectCyclingPorts() {
  const features = [];

  for (const net of NETWORKS) {
    const stations = await loadStations(net);
    for (const s of stations) {
      if (s.lat == null || s.lon == null) continue;
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [s.lon, s.lat] },
        properties: {
          network: net.net,
          station_id: s.station_id ?? null,
          name: s.name || null,
          capacity: s.capacity ?? null,
          address: s.address || null,
          country: 'JP',
          source: 'docomo_bikeshare_gbfs',
        },
      });
    }
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: features.length ? 'cycling_ports_gbfs' : 'cycling_ports_unavailable',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'DOCOMO Bike Share cycling ports via public GBFS station_information',
    },
  };
}
