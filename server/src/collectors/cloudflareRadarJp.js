/**
 * Cloudflare Radar — JP attack/DDoS/BGP/traffic snapshot.
 *
 * Auth: CLOUDFLARE_API_TOKEN (free; Radar.read scope). Endpoints used:
 *   /radar/attacks/layer3/summary?location=JP
 *   /radar/attacks/layer7/top/origin?location=JP
 *   /radar/bgp/leaks/events?involved_country=JP
 *   /radar/quality/iqi/summary?location=JP
 *   /radar/dns/top/locations?location=JP
 */

import { createThreatIntelCollector } from '../utils/threatIntelCollectorFactory.js';
import { TOKYO } from './_satelliteSeeds.js';

const BASE = 'https://api.cloudflare.com/client/v4/radar';
const TIMEOUT_MS = 15000;

async function radar(path, token) {
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(`${BASE}${path}`, {
      signal: ctrl.signal,
      headers: { authorization: `Bearer ${token}`, accept: 'application/json' },
    });
    clearTimeout(t);
    if (!res.ok) return { err: `HTTP ${res.status}` };
    return await res.json();
  } catch (err) { return { err: err?.message || 'fetch_failed' }; }
}

export default createThreatIntelCollector({
  sourceId: 'cf_radar',
  description: 'Cloudflare Radar — JP DDoS / BGP leaks / DNS / IQI summary',
  envKey: 'CLOUDFLARE_API_TOKEN',
  envHint: 'Set CLOUDFLARE_API_TOKEN with the Radar Read scope (free)',
  run: async (token) => {
    const [l3, l7, bgp, iqi, dns] = await Promise.all([
      radar('/attacks/layer3/summary?location=JP&dateRange=7d', token),
      radar('/attacks/layer7/top/origins?location=JP&dateRange=7d&limit=10', token),
      radar('/bgp/leaks/events?involved_country=JP&per_page=20', token),
      radar('/quality/iqi/summary?location=JP&dateRange=7d', token),
      radar('/dns/top/locations?location=JP&dateRange=7d&limit=10', token),
    ]);

    const features = [];
    const push = (kind, data) => features.push({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: TOKYO },
      properties: { kind, ...data, source: 'cf_radar' },
    });

    push('attacks_layer3', { result: l3?.result, err: l3?.err || null });
    push('attacks_layer7_top_origins', { result: l7?.result, err: l7?.err || null });
    push('bgp_leaks', { result: bgp?.result, err: bgp?.err || null });
    push('quality_iqi', { result: iqi?.result, err: iqi?.err || null });
    push('dns_top_locations', { result: dns?.result, err: dns?.err || null });

    return { features };
  },
});
