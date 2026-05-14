/**
 * VirusTotal v3 — domain reputation + passive-DNS pivots for JP-megacorp domains.
 *
 * Free tier: 4 req/min, 500/day. We poll the canonical JP corp/gov domains
 * and surface community votes, last-analysis stats, and recent resolutions.
 *
 * Endpoint: GET https://www.virustotal.com/api/v3/domains/<domain>
 */

import { createThreatIntelCollector } from '../utils/threatIntelCollectorFactory.js';
import { TOKYO } from './_satelliteSeeds.js';

const BASE = 'https://www.virustotal.com/api/v3/domains/';
const TIMEOUT_MS = 15000;

const DEFAULT_DOMAINS = (process.env.VIRUSTOTAL_DOMAINS || [
  'mufg.jp', 'rakuten.co.jp', 'japanpost.jp', 'jal.co.jp', 'ana.co.jp',
  'sony.co.jp', 'toyota.co.jp', 'meti.go.jp', 'mod.go.jp', 'kantei.go.jp',
  'docomo.ne.jp', 'softbank.jp', 'kddi.com',
].join(',')).split(',').map((s) => s.trim()).filter(Boolean);

async function vtFetch(path, key) {
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(`${BASE}${path}`, {
      signal: ctrl.signal,
      headers: { 'x-apikey': key, accept: 'application/json' },
    });
    clearTimeout(t);
    if (res.status === 429) return { rate_limited: true };
    if (!res.ok) return { err: `HTTP ${res.status}` };
    return await res.json();
  } catch (err) { return { err: err?.message || 'fetch_failed' }; }
}

export default createThreatIntelCollector({
  sourceId: 'virustotal',
  description: 'VirusTotal v3 — JP-megacorp domain reputation snapshot',
  envKey: 'VIRUSTOTAL_API_KEY',
  envHint: 'Set VIRUSTOTAL_API_KEY (free at https://www.virustotal.com/gui/sign-in)',
  run: async (key) => {
    // Throttle to 4 req/min: stagger sequential requests with 16s delay.
    const features = [];
    for (let i = 0; i < DEFAULT_DOMAINS.length; i += 1) {
      const domain = DEFAULT_DOMAINS[i];
      const dom = await vtFetch(encodeURIComponent(domain), key);
      if (dom?.rate_limited) {
        features.push({
          type: 'Feature',
          geometry: { type: 'Point', coordinates: TOKYO },
          properties: { domain, rate_limited: true, source: 'virustotal' },
        });
        break;
      }
      const a = dom?.data?.attributes || {};
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: TOKYO },
        properties: {
          idx: i,
          domain,
          last_analysis_stats: a.last_analysis_stats,
          last_dns_records: Array.isArray(a.last_dns_records) ? a.last_dns_records.slice(0, 5) : [],
          registrar: a.registrar,
          creation_date: a.creation_date,
          last_modification_date: a.last_modification_date,
          reputation: a.reputation,
          total_votes: a.total_votes,
          categories: a.categories,
          err: dom?.err || null,
          source: 'virustotal',
        },
      });
      if (i < DEFAULT_DOMAINS.length - 1) {
        // 16s delay to fit 4 req/min budget (single-key path)
        await new Promise((r) => setTimeout(r, 16000));
      }
    }
    return {
      features,
      extraMeta: { env_hint: 'Free tier 4 req/min — collector throttles to 16s spacing' },
    };
  },
});
