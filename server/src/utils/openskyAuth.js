/**
 * OpenSky Network OAuth2 token cache.
 * Exchanges OPENSKY_CLIENT_ID / OPENSKY_CLIENT_SECRET for a bearer token
 * using the client-credentials flow and caches it until 30 s before expiry.
 * Returns null when credentials are absent or the request fails, allowing
 * callers to fall back to anonymous (rate-limited) access.
 */

const TOKEN_URL = 'https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token';

let cachedToken = null;
let tokenExpiresAt = 0;

export async function getOAuthToken() {
  const clientId = process.env.OPENSKY_CLIENT_ID || '';
  const clientSecret = process.env.OPENSKY_CLIENT_SECRET || '';
  if (!clientId || !clientSecret) return null;
  if (cachedToken && Date.now() < tokenExpiresAt) return cachedToken;

  try {
    const res = await fetch(TOKEN_URL, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: `grant_type=client_credentials&client_id=${encodeURIComponent(clientId)}&client_secret=${encodeURIComponent(clientSecret)}`,
    });
    if (!res.ok) return null;
    const data = await res.json();
    cachedToken = data.access_token;
    const expiresIn = data.expires_in ?? 300;
    tokenExpiresAt = Date.now() + (expiresIn - 30) * 1000;
    return cachedToken;
  } catch {
    return null;
  }
}

// Module-level cache can't be reset between `node --test` runs without
// re-importing. This escape hatch lets tests clear it in place.
export function __resetForTests() {
  cachedToken = null;
  tokenExpiresAt = 0;
}
