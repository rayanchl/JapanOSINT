/**
 * RIPEstat — JP country-level routing resource totals.
 *
 * Free, no auth. Endpoint:
 *   /data/country-resource-list/data.json?resource=JP
 *
 * Emits one Intel item with ASN / IPv4 / IPv6 prefix counts for JP. No
 * geometry — BGP routing-table aggregates aren't spatial.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'ripestat-jp';
const BASE = 'https://stat.ripe.net/data';
const TIMEOUT_MS = 15000;

async function ripe(path) {
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const r = await fetch(`${BASE}${path}`, { signal: ctrl.signal, headers: { accept: 'application/json' } });
    clearTimeout(t);
    if (!r.ok) return { err: `HTTP ${r.status}` };
    return await r.json();
  } catch (err) { return { err: err?.message || 'fetch_failed' }; }
}

export default async function collectRipestatJp() {
  const country = await ripe('/country-resource-list/data.json?resource=JP');
  const resources = country?.data?.resources || {};
  const asnCount  = Array.isArray(resources.asn)  ? resources.asn.length  : 0;
  const ipv4Count = Array.isArray(resources.ipv4) ? resources.ipv4.length : 0;
  const ipv6Count = Array.isArray(resources.ipv6) ? resources.ipv6.length : 0;
  const live = !country?.err && asnCount > 0;

  const items = [{
    uid: intelUid(SOURCE_ID, 'jp-country-resources'),
    title: 'Japan country routing resources (RIPEstat)',
    summary: `${asnCount.toLocaleString()} ASNs · ${ipv4Count.toLocaleString()} IPv4 prefixes · ${ipv6Count.toLocaleString()} IPv6 prefixes`,
    body: [
      `ASNs registered to JP: ${asnCount}`,
      `IPv4 prefixes: ${ipv4Count}`,
      `IPv6 prefixes: ${ipv6Count}`,
    ].join('\n'),
    link: 'https://stat.ripe.net/JP',
    language: 'en',
    published_at: new Date().toISOString(),
    tags: ['bgp', 'routing', 'country-stats', live ? 'reachable' : 'unreachable'],
    properties: {
      country: 'JP',
      asn_count: asnCount,
      ipv4_count: ipv4Count,
      ipv6_count: ipv6Count,
      err: country?.err || null,
      source: 'ripestat_country_resource_list',
    },
  }];

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live,
    description: 'RIPEstat — JP country-level routing resource totals',
  });
}
