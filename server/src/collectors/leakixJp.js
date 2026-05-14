/**
 * LeakIX — internet exposure search engine, JP query.
 *
 * Auth: LEAKIX_API_KEY (free at https://leakix.net/auth/register).
 * Endpoint: GET https://leakix.net/search?scope=service&q=country%3A%22JP%22
 *           Response is JSON (max 100 items per page).
 */

import { createThreatIntelCollector } from '../utils/threatIntelCollectorFactory.js';
import { TOKYO } from './_satelliteSeeds.js';

const URL = 'https://leakix.net/search?scope=service&q=' + encodeURIComponent('country:"JP"');
const TIMEOUT_MS = 15000;

export default createThreatIntelCollector({
  sourceId: 'leakix',
  description: 'LeakIX — exposed services geolocated in Japan',
  envKey: 'LEAKIX_API_KEY',
  envHint: 'Set LEAKIX_API_KEY (free at https://leakix.net/auth/register)',
  run: async (key) => {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(URL, {
      signal: ctrl.signal,
      headers: { 'api-key': key, accept: 'application/json' },
    });
    clearTimeout(t);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const arr = await res.json();
    const features = (Array.isArray(arr) ? arr : []).slice(0, 200).map((s, i) => {
      const lon = s?.geoip?.longitude;
      const lat = s?.geoip?.latitude;
      const geom = (Number.isFinite(lon) && Number.isFinite(lat))
        ? { type: 'Point', coordinates: [lon, lat] }
        : { type: 'Point', coordinates: TOKYO };
      return {
        type: 'Feature',
        geometry: geom,
        properties: {
          idx: i,
          ip: s.ip,
          host: s.host,
          port: s.port,
          protocol: s.protocol,
          service_name: s?.service?.software?.name || null,
          service_version: s?.service?.software?.version || null,
          tags: s.tags,
          events: s.events,
          leak_severity: s?.leak?.severity || null,
          leak_type: s?.leak?.type || null,
          time: s.time,
          country: s?.geoip?.country_name || null,
          city: s?.geoip?.city_name || null,
          asn: s?.network?.asn || null,
          as_name: s?.network?.organization_name || null,
          source: 'leakix',
        },
      };
    });
    return { features };
  },
});
