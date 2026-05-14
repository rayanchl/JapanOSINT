/**
 * HudsonRock Cavalier — infostealer-infection summary per JP domain.
 *
 * The /osint-tools/ endpoints are free, no key needed. We poll a curated
 * list of high-value JP domains (megabank / megacorp / ministries / ISPs)
 * and surface counts of: stealers, employees-with-leaked-creds, users,
 * third-party domains, etc.
 *
 * Endpoint: GET https://cavalier.hudsonrock.com/api/json/v2/osint-tools/search-by-domain?domain=<d>
 */

import { createThreatIntelCollector } from '../utils/threatIntelCollectorFactory.js';
import { TOKYO } from './_satelliteSeeds.js';

const BASE = 'https://cavalier.hudsonrock.com/api/json/v2/osint-tools/search-by-domain';
const TIMEOUT_MS = 15000;

const DEFAULT_DOMAINS = (process.env.HUDSONROCK_DOMAINS || [
  // Megabanks
  'mufg.jp', 'smbc.co.jp', 'mizuho-fg.co.jp', 'rakuten-bank.co.jp', 'japanpost.jp',
  // Telecoms / ISPs
  'ntt.co.jp', 'docomo.ne.jp', 'kddi.com', 'softbank.jp', 'iij.ad.jp', 'rakuten.co.jp',
  // Megacorps / IT
  'sony.co.jp', 'panasonic.com', 'fujitsu.com', 'nec.com', 'hitachi.co.jp',
  'mitsubishielectric.co.jp', 'toshiba.co.jp', 'canon.jp', 'ricoh.co.jp',
  // Auto
  'toyota.co.jp', 'honda.co.jp', 'nissan.co.jp', 'subaru.co.jp', 'mazda.co.jp',
  // Air / rail
  'jal.co.jp', 'ana.co.jp', 'jr-east.co.jp', 'jr-central.co.jp',
  // Government
  'meti.go.jp', 'mod.go.jp', 'kantei.go.jp', 'mofa.go.jp', 'soumu.go.jp',
].join(',')).split(',').map((s) => s.trim()).filter(Boolean);

async function lookupOne(domain) {
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(`${BASE}?domain=${encodeURIComponent(domain)}`, {
      signal: ctrl.signal,
      headers: { accept: 'application/json' },
    });
    clearTimeout(t);
    if (!res.ok) return { domain, err: `HTTP ${res.status}` };
    return { domain, data: await res.json() };
  } catch (err) {
    return { domain, err: err?.message || 'fetch_failed' };
  }
}

export default createThreatIntelCollector({
  sourceId: 'hudsonrock_osint',
  description: 'HudsonRock Cavalier — infostealer-infection summary per JP domain',
  run: async () => {
    const concurrency = 4;
    const out = [];
    for (let i = 0; i < DEFAULT_DOMAINS.length; i += concurrency) {
      const slice = DEFAULT_DOMAINS.slice(i, i + concurrency);
      out.push(...(await Promise.all(slice.map(lookupOne))));
    }
    const features = out.map((r, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: TOKYO },
      properties: {
        idx: i,
        domain: r.domain,
        total_employees: r?.data?.total_employees ?? null,
        total_users: r?.data?.total_users ?? null,
        employees: r?.data?.employees ?? null,
        users: r?.data?.users ?? null,
        third_parties: r?.data?.third_parties ?? null,
        stealers: r?.data?.stealers ?? null,
        total_external_domains: r?.data?.total_external_domains ?? null,
        external_domains: r?.data?.external_domains ?? null,
        message: r?.data?.message ?? null,
        err: r.err || null,
        source: 'hudsonrock_osint',
      },
    }));
    return {
      features,
      extraMeta: {
        domains_polled: DEFAULT_DOMAINS.length,
        env_hint: 'HUDSONROCK_DOMAINS=comma,sep,domains to override',
      },
    };
  },
});
