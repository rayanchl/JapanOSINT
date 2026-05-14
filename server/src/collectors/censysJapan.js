/**
 * Censys — internet-exposed hosts with location.country: Japan.
 *
 * Auth: CENSYS_API_ID + CENSYS_API_SECRET (free account signup at
 * https://search.censys.io/). Free tier is capped at 500 results per
 * query and limited host-history window; for larger corpora upgrade to
 * a paid plan.
 *
 * Censys Search v2: POST https://search.censys.io/api/v2/hosts/search
 * with query = "location.country_code: JP".
 */

import { createThreatIntelCollector } from '../utils/threatIntelCollectorFactory.js';

const BASE = 'https://search.censys.io/api/v2';
const TIMEOUT_MS = 20000;

function authHeader() {
  const id = process.env.CENSYS_API_ID;
  const secret = process.env.CENSYS_API_SECRET;
  if (!id || !secret) return null;
  return `Basic ${Buffer.from(`${id}:${secret}`).toString('base64')}`;
}

export default createThreatIntelCollector({
  sourceId: 'censys',
  description: 'Censys internet-exposed hosts, country_code: JP',
  // Censys uses two env vars (id+secret) so we manage the key check ourselves
  // by pointing envKey at the id and verifying both inside run().
  envKey: 'CENSYS_API_ID',
  envHint: 'Set CENSYS_API_ID and CENSYS_API_SECRET (free signup: https://search.censys.io/)',
  run: async () => {
    const auth = authHeader();
    if (!auth) {
      // Secret missing even though id is set — surface as a key error.
      throw new Error('CENSYS_API_SECRET not set');
    }
    const url = `${BASE}/hosts/search`;
    const body = { q: 'location.country_code: JP', per_page: 100 };
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(url, {
      method: 'POST',
      signal: ctrl.signal,
      headers: {
        authorization: auth,
        'content-type': 'application/json',
        accept: 'application/json',
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
    return { features, source: 'censys_live', extraMeta: { query: body.q } };
  },
});
