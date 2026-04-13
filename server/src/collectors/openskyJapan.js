/**
 * OpenSky Network ADS-B, Japan bounding box
 * https://opensky-network.org/api/states/all?lamin=24&lomin=122&lamax=46&lomax=146
 * Anonymous but heavily rate-limited; keep interval >= 30s.
 */

const API_URL = 'https://opensky-network.org/api/states/all?lamin=24&lomin=122&lamax=46&lomax=146';
const TIMEOUT_MS = 12000;

export default async function collectOpenskyJapan() {
  let features = [];
  let source = 'live';
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(API_URL, { signal: controller.signal });
    clearTimeout(timer);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.json();
    const states = data?.states ?? [];
    for (const s of states) {
      // [icao24, callsign, origin_country, ..., longitude(5), latitude(6), baro_alt(7), on_ground(8), velocity(9), heading(10)]
      const lon = s[5];
      const lat = s[6];
      if (lon == null || lat == null) continue;
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [lon, lat] },
        properties: {
          icao24: s[0],
          callsign: (s[1] ?? '').trim(),
          origin_country: s[2],
          altitude_m: s[7],
          on_ground: s[8],
          velocity_mps: s[9],
          heading_deg: s[10],
          source: 'opensky',
        },
      });
    }
    if (features.length === 0) throw new Error('empty');
  } catch {
    source = 'seed';
    features = [
      { callsign: 'JAL001', lat: 35.55, lon: 139.78, alt: 10500 },
      { callsign: 'ANA006', lat: 34.43, lon: 135.24, alt: 11200 },
    ].map(p => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [p.lon, p.lat] },
      properties: { callsign: p.callsign, altitude_m: p.alt, source: 'opensky_seed' },
    }));
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'OpenSky ADS-B live aircraft states, Japan bbox',
    },
    metadata: {},
  };
}
