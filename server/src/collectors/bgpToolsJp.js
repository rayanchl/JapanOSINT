/**
 * BGP.tools — JP-registered ASN topology slice.
 *
 * Pulls the (auth-free) ASN listing CSV at https://bgp.tools/asns.csv
 * and the JP-country mapping from RIPEstat (free) to filter by country.
 *
 * Set BGPTOOLS_USER_AGENT to a contact email per BGP.tools' policy.
 */

const URL_ASNS = 'https://bgp.tools/asns.csv';
const URL_RIPESTAT_JP = 'https://stat.ripe.net/data/country-resource-list/data.json?resource=JP';
const TIMEOUT_MS = 30000;

const TOKYO = [139.6917, 35.6895];

async function fetchTextWithUA(url) {
  // BGP.tools requires a *real* contact email in the user agent or the
  // request is 403'd. We refuse to send placeholder/example.com defaults.
  const ua = process.env.BGPTOOLS_USER_AGENT;
  if (!ua || /example\.com|@example/i.test(ua) || !/@/.test(ua)) {
    return { ua_required: true };
  }
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const r = await fetch(url, { signal: ctrl.signal, headers: { 'user-agent': ua } });
    clearTimeout(t);
    if (!r.ok) return { err: `HTTP ${r.status}` };
    return { text: await r.text() };
  } catch (e) { return { err: e?.message || 'fetch_failed' }; }
}

async function fetchJpAsnSet() {
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const r = await fetch(URL_RIPESTAT_JP, { signal: ctrl.signal });
    clearTimeout(t);
    if (!r.ok) return new Set();
    const j = await r.json();
    const asns = j?.data?.resources?.asn || [];
    const set = new Set();
    for (const item of asns) {
      // RIPEstat returns either single ASNs ("12345") or ranges ("100-200")
      const s = String(item);
      const m = s.match(/^(\d+)(?:-(\d+))?$/);
      if (!m) continue;
      const start = Number(m[1]);
      const end = m[2] ? Number(m[2]) : start;
      for (let n = start; n <= end && n - start < 5000; n += 1) set.add(n);
    }
    return set;
  } catch { return new Set(); }
}

function parseCsvAsn(csv) {
  const out = [];
  for (const raw of csv.split(/\r?\n/)) {
    const line = raw.trim();
    if (!line || line.startsWith('asn')) continue;
    const i = line.indexOf(',');
    if (i < 0) continue;
    const asnStr = line.slice(0, i);
    const name = line.slice(i + 1).replace(/^"|"$/g, '');
    const asn = Number(asnStr);
    if (!Number.isFinite(asn)) continue;
    out.push({ asn, name });
  }
  return out;
}

export default async function collectBgpToolsJp() {
  const [asnRes, jpAsns] = await Promise.all([fetchTextWithUA(URL_ASNS), fetchJpAsnSet()]);
  if (asnRes?.ua_required) {
    return {
      type: 'FeatureCollection',
      features: [],
      _meta: {
        source: 'bgp_tools_no_ua',
        fetchedAt: new Date().toISOString(),
        recordCount: 0,
        env_hint: 'Set BGPTOOLS_USER_AGENT to a real contact email per BGP.tools policy (e.g. "japanosint contact@yourdomain.tld")',
        description: 'BGP.tools — no contact UA configured',
      },
    };
  }
  const all = parseCsvAsn(asnRes?.text || '');
  const jp = jpAsns.size > 0 ? all.filter((r) => jpAsns.has(r.asn)) : [];

  const features = jp.slice(0, 4000).map((r, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: TOKYO },
    properties: {
      idx: i,
      asn: r.asn,
      name: r.name,
      url: `https://bgp.tools/as/${r.asn}`,
      source: 'bgp_tools',
    },
  }));

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'bgp_tools',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      total_asns: all.length,
      jp_asn_count: jpAsns.size,
      env_hint: 'Set BGPTOOLS_USER_AGENT to a contact email per BGP.tools policy',
      description: 'BGP.tools ASN list intersected with RIPEstat JP-country ASN set',
    },
  };
}
