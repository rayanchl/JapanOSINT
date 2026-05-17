// server/src/osint/services/net.js
//
// JS ports of the network-OSINT C services that need no API key. Each returns
// the dispatcher's uniform shape: { success, data(JSON string), confidence }.
// More C services port into this folder incrementally ("as needed").

import { promises as dns } from 'node:dns';

const ok = (obj, confidence = 85) => ({ success: true, confidence, data: JSON.stringify(obj) });
const fail = (error) => ({ success: false, confidence: 0, error: String(error) });

// IP_GEOLOCATION + ASN_LOOKUP — ip-api.com (free, no key, returns AS too).
export async function ipGeolocation(entity) {
  try {
    const r = await fetch(`http://ip-api.com/json/${encodeURIComponent(entity)}?fields=66846719`);
    if (!r.ok) return fail(`ip-api ${r.status}`);
    const j = await r.json();
    if (j.status !== 'success') return fail(j.message || 'lookup_failed');
    return ok(j, 95);
  } catch (e) { return fail(e?.message || e); }
}

export async function asnLookup(entity) {
  try {
    const r = await fetch(`http://ip-api.com/json/${encodeURIComponent(entity)}?fields=as,asname,isp,org,query,status,message`);
    if (!r.ok) return fail(`ip-api ${r.status}`);
    const j = await r.json();
    if (j.status !== 'success') return fail(j.message || 'lookup_failed');
    return ok({ ip: j.query, as: j.as, asname: j.asname, isp: j.isp, org: j.org }, 90);
  } catch (e) { return fail(e?.message || e); }
}

// REVERSE_DNS — PTR for an IP.
export async function reverseDns(entity) {
  try {
    const names = await dns.reverse(entity);
    return ok({ ip: entity, ptr: names }, names.length ? 90 : 40);
  } catch (e) { return fail(e?.message || e); }
}

// DNS_RECORDS — A/AAAA/MX/NS/TXT for a domain.
export async function dnsRecords(entity) {
  const out = { domain: entity };
  await Promise.all(
    [['A', 'resolve4'], ['AAAA', 'resolve6'], ['MX', 'resolveMx'],
     ['NS', 'resolveNs'], ['TXT', 'resolveTxt']].map(async ([k, fn]) => {
      try { out[k] = await dns[fn](entity); } catch { out[k] = []; }
    }),
  );
  const any = ['A', 'AAAA', 'MX', 'NS', 'TXT'].some((k) => (out[k] || []).length);
  return ok(out, any ? 85 : 30);
}
