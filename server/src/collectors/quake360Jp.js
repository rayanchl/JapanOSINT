/**
 * Quake (360 NetLab) — internet-asset search, complements FOFA for Asian
 * IoT/ICS coverage. Hits the public realtime/service endpoint filtered to
 * country: 日本 (or country_cn=日本), JP host fingerprints.
 *
 * Auth: QUAKE_API_KEY from https://quake.360.net/quake/#/personal.
 * Free tier returns up to 100 records per call with limited fields.
 *
 * Endpoint:
 *   POST https://quake.360.net/api/v3/search/quake_service
 *   headers: X-QuakeToken: <key>
 *   body: { query, start, size, include }
 *
 * Empty FeatureCollection when key missing — no fallback.
 */

const BASE = 'https://quake.360.net/api/v3/search/quake_service';
const TIMEOUT_MS = 20000;

const DEFAULT_QUERY = process.env.QUAKE_QUERY || 'country: "Japan"';

export default async function collectQuake360Jp() {
  const key = process.env.QUAKE_API_KEY;
  if (!key) {
    return {
      type: 'FeatureCollection',
      features: [],
      _meta: {
        source: 'quake_no_key',
        fetchedAt: new Date().toISOString(),
        recordCount: 0,
        env_hint: 'Set QUAKE_API_KEY (free signup: https://quake.360.net/)',
        description: 'Quake (360) Japan internet assets — requires API key',
      },
    };
  }

  const body = {
    query: DEFAULT_QUERY,
    start: 0,
    size: 100,
    include: [
      'ip', 'port', 'hostname', 'transport', 'asn', 'org', 'service',
      'location', 'time',
    ],
    latest: true,
  };

  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(BASE, {
      method: 'POST',
      signal: controller.signal,
      headers: {
        'X-QuakeToken': key,
        'content-type': 'application/json',
        accept: 'application/json',
      },
      body: JSON.stringify(body),
    });
    clearTimeout(timer);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const json = await res.json();
    if (json.code && json.code !== 0 && json.code !== '0') {
      throw new Error(json.message || `Quake error code ${json.code}`);
    }
    const rows = Array.isArray(json.data) ? json.data : [];

    const features = rows.map((r, i) => {
      const lat = r.location?.latitude;
      const lon = r.location?.longitude;
      return {
        type: 'Feature',
        geometry: Number.isFinite(lat) && Number.isFinite(lon)
          ? { type: 'Point', coordinates: [lon, lat] }
          : null,
        properties: {
          id: `QUAKE_${r.ip}_${r.port}_${i}`,
          ip: r.ip,
          port: r.port,
          hostname: r.hostname || null,
          transport: r.transport || null,
          asn: r.asn || null,
          org: r.org || null,
          service_name: r.service?.name || null,
          product: r.service?.product || null,
          banner: typeof r.service?.response === 'string'
            ? r.service.response.slice(0, 240)
            : null,
          city: r.location?.city_en || r.location?.city_cn || null,
          province: r.location?.province_en || r.location?.province_cn || null,
          country: r.location?.country_en || r.location?.country_cn || null,
          last_seen: r.time || null,
          source: 'quake360_v3',
        },
      };
    });

    return {
      type: 'FeatureCollection',
      features,
      _meta: {
        source: 'quake_live',
        fetchedAt: new Date().toISOString(),
        recordCount: features.length,
        query: DEFAULT_QUERY,
        description: 'Quake (360) internet asset search — Japan',
      },
    };
  } catch (err) {
    console.warn('[quake360Jp] fetch failed:', err?.message);
    return {
      type: 'FeatureCollection',
      features: [],
      _meta: {
        source: 'quake_error',
        fetchedAt: new Date().toISOString(),
        recordCount: 0,
        error: err?.message,
        description: 'Quake fetch failed — check QUAKE_API_KEY and rate limit',
      },
    };
  }
}
