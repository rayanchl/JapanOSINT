/**
 * Netlas.io — internet asset search, JP query.
 *
 * Auth: NETLAS_API_KEY (free signup, https://app.netlas.io/login/).
 * Endpoint: GET https://app.netlas.io/api/responses/?q=geo.country:%22Japan%22
 *           Returns JSON with `items[]`.
 */

import { createThreatIntelCollector } from '../utils/threatIntelCollectorFactory.js';
import { TOKYO } from './_satelliteSeeds.js';

const URL_BASE = 'https://app.netlas.io/api/responses/';
const TIMEOUT_MS = 15000;
const QUERY = process.env.NETLAS_QUERY || 'geo.country:"Japan"';
const SIZE = Number(process.env.NETLAS_LIMIT || 100);

export default createThreatIntelCollector({
  sourceId: 'netlas',
  description: 'Netlas.io — JP-located internet responses',
  envKey: 'NETLAS_API_KEY',
  envHint: 'Set NETLAS_API_KEY (free at https://app.netlas.io/login/)',
  run: async (key) => {
    const url = `${URL_BASE}?q=${encodeURIComponent(QUERY)}&size=${SIZE}`;
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(url, {
      signal: ctrl.signal,
      headers: { 'X-API-Key': key, accept: 'application/json' },
    });
    clearTimeout(t);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const json = await res.json();
    const items = Array.isArray(json?.items) ? json.items : [];
    const features = items.map((it, i) => {
      const d = it?.data || it;
      const lon = d?.geo?.location?.lon;
      const lat = d?.geo?.location?.lat;
      const geom = (Number.isFinite(lon) && Number.isFinite(lat))
        ? { type: 'Point', coordinates: [lon, lat] }
        : { type: 'Point', coordinates: TOKYO };
      return {
        type: 'Feature',
        geometry: geom,
        properties: {
          idx: i,
          ip: d?.ip,
          port: d?.port,
          protocol: d?.protocol,
          host: d?.host,
          country: d?.geo?.country,
          city: d?.geo?.city,
          asn: d?.asn?.number,
          as_name: d?.asn?.name,
          product: d?.product,
          title: d?.http?.title || d?.tls?.subject?.common_name || null,
          last_updated: d?.last_updated,
          source: 'netlas',
        },
      };
    });
    return {
      features,
      extraMeta: {
        query: QUERY,
        env_hint: 'NETLAS_QUERY to override; NETLAS_LIMIT to tune',
      },
    };
  },
});
