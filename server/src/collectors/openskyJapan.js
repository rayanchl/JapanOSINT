/**
 * OpenSky Network ADS-B, Japan bounding box
 * https://opensky-network.org/api/states/all?lamin=24&lomin=122&lamax=46&lomax=146
 *
 * Supports OAuth2 client credentials (OPENSKY_CLIENT_ID / OPENSKY_CLIENT_SECRET)
 * and falls back to anonymous (heavily rate-limited).
 */

const API_URL = 'https://opensky-network.org/api/states/all?lamin=24&lomin=122&lamax=46&lomax=146';
const TOKEN_URL = 'https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token';
const TIMEOUT_MS = 12000;

const CLIENT_ID = process.env.OPENSKY_CLIENT_ID || '';
const CLIENT_SECRET = process.env.OPENSKY_CLIENT_SECRET || '';

const POSITION_SOURCE = ['ADS-B', 'ASTERIX', 'MLAT', 'FLARM'];
const CATEGORY_LABELS = [
  'No info', 'No ADS-B category', 'Light (<15500 lbs)', 'Small (15500-75000 lbs)',
  'Large (75000-300000 lbs)', 'High Vortex Large', 'Heavy (>300000 lbs)',
  'High Performance', 'Rotorcraft', 'Glider/Sailplane', 'Lighter-than-air',
  'Parachutist/Skydiver', 'Ultralight/Paraglider', 'Reserved', 'UAV',
  'Space/Trans-atmospheric', 'Emergency Vehicle', 'Service Vehicle',
  'Point Obstacle', 'Cluster Obstacle', 'Line Obstacle',
];

// Simple in-memory token cache
let cachedToken = null;
let tokenExpiresAt = 0;

async function getOAuthToken() {
  if (!CLIENT_ID || !CLIENT_SECRET) return null;
  if (cachedToken && Date.now() < tokenExpiresAt) return cachedToken;

  try {
    const res = await fetch(TOKEN_URL, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: `grant_type=client_credentials&client_id=${encodeURIComponent(CLIENT_ID)}&client_secret=${encodeURIComponent(CLIENT_SECRET)}`,
    });
    if (!res.ok) return null;
    const data = await res.json();
    cachedToken = data.access_token;
    tokenExpiresAt = Date.now() + (data.expires_in - 30) * 1000;
    return cachedToken;
  } catch {
    return null;
  }
}

export default async function collectOpenskyJapan() {
  let features = [];
  let source = 'live';
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);

    const headers = {};
    const token = await getOAuthToken();
    if (token) headers.Authorization = `Bearer ${token}`;

    const res = await fetch(API_URL, { headers, signal: controller.signal });
    clearTimeout(timer);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.json();
    const states = data?.states ?? [];
    for (const s of states) {
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
          time_position: s[3],
          last_contact: s[4],
          baro_altitude_m: s[7],
          altitude_ft: s[7] != null ? Math.round(s[7] * 3.28084) : null,
          on_ground: s[8],
          ground_speed_knots: s[9] != null ? Math.round(s[9] * 1.94384) : null,
          velocity_mps: s[9],
          true_track: s[10] != null ? Math.round(s[10]) : null,
          heading: s[10] != null ? Math.round(s[10]) : null,
          vertical_rate_fpm: s[11] != null ? Math.round(s[11] * 196.85) : null,
          geo_altitude_m: s[13],
          geo_altitude_ft: s[13] != null ? Math.round(s[13] * 3.28084) : null,
          squawk: s[14],
          spi: s[15],
          position_source: POSITION_SOURCE[s[16]] || s[16],
          category: s[17] != null ? (CATEGORY_LABELS[s[17]] || s[17]) : null,
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
      properties: { callsign: p.callsign, altitude_ft: Math.round(p.alt * 3.28084), source: 'opensky_seed' },
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
