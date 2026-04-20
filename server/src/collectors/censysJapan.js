/**
 * Censys — internet-exposed hosts with location.country: Japan.
 *
 * Auth: CENSYS_API_ID + CENSYS_API_SECRET (free account signup at
 * https://search.censys.io/). Free tier is capped at 500 results per
 * query and limited host-history window; for larger corpora upgrade to
 * a paid plan.
 *
 * We use Censys Search v2 REST endpoints:
 *   POST https://search.censys.io/api/v2/hosts/search
 * with query = "location.country_code: JP" and fields.
 *
 * Empty FeatureCollection when keys are missing — no scraping fallback.
 */

const BASE = 'https://search.censys.io/api/v2';
const TIMEOUT_MS = 20000;

function authHeader() {
  const id = process.env.CENSYS_API_ID;
  const secret = process.env.CENSYS_API_SECRET;
  if (!id || !secret) return null;
  return `Basic ${Buffer.from(`${id}:${secret}`).toString('base64')}`;
}

export default async function collectCensysJapan() {
  const auth = authHeader();
  if (!auth) {
    return {
      type: 'FeatureCollection',
      features: [],
      _meta: {
        source: 'censys_no_key',
        fetchedAt: new Date().toISOString(),
        recordCount: 0,
        env_hint: 'Set CENSYS_API_ID and CENSYS_API_SECRET (free signup: https://search.censys.io/)',
        description: 'Censys hosts located in Japan',
      },
      metadata: {},
    };
  }

  const url = `${BASE}/hosts/search`;
  const body = {
    q: 'location.country_code: JP',
    per_page: 100,
  };

  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(url, {
      method: 'POST',
      signal: controller.signal,
      headers: {
        'authorization': auth,
        'content-type': 'application/json',
        'accept': 'application/json',
      },
      body: JSON.stringify(body),
    });
    clearTimeout(timer);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.json();
    const hits = data?.result?.hits || [];

    const features = hits.map((h) => {
      const lat = h.location?.coordinates?.latitude;
      const lon = h.location?.coordinates?.longitude;
      return {
        type: 'Feature',
        geometry: Number.isFinite(lat) && Number.isFinite(lon)
          ? { type: 'Point', coordinates: [lon, lat] }
          : null,
        properties: {
          id: `CENSYS_${h.ip}`,
          ip: h.ip,
          asn: h.autonomous_system?.asn || null,
          as_name: h.autonomous_system?.name || null,
          as_country: h.autonomous_system?.country_code || null,
          city: h.location?.city || null,
          province: h.location?.province || null,
          services: Array.isArray(h.services)
            ? h.services.map((s) => `${s.port}/${s.service_name || s.transport_protocol}`)
            : [],
          last_updated: h.last_updated_at || null,
          source: 'censys_hosts_search_v2',
        },
      };
    });

    return {
      type: 'FeatureCollection',
      features,
      _meta: {
        source: 'censys_live',
        fetchedAt: new Date().toISOString(),
        recordCount: features.length,
        query: body.q,
        description: 'Censys internet-exposed hosts, country_code: JP',
      },
      metadata: {},
    };
  } catch (err) {
    console.warn('[censysJapan] fetch failed:', err?.message);
    return {
      type: 'FeatureCollection',
      features: [],
      _meta: {
        source: 'censys_error',
        fetchedAt: new Date().toISOString(),
        recordCount: 0,
        error: err?.message,
        description: 'Censys fetch failed — check credentials and rate limit',
      },
      metadata: {},
    };
  }
}
