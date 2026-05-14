/**
 * abuse.ch ThreatFox — recent IOCs filtered to Japan-related entries.
 *
 * Auth: ABUSE_CH_AUTH_KEY (free at https://auth.abuse.ch/).
 * Endpoint: POST https://threatfox-api.abuse.ch/api/v1/  body { query:'get_iocs', days:7 }
 *
 * IOCs include domains, IPs, URLs and file hashes. We filter to entries
 * whose ioc_value ends in .jp, mentions japan in tags, or whose
 * confidence_level >= a threshold (configurable).
 */

import { createThreatIntelCollector } from '../utils/threatIntelCollectorFactory.js';
import { TOKYO } from './_satelliteSeeds.js';

const URL = 'https://threatfox-api.abuse.ch/api/v1/';
const TIMEOUT_MS = 15000;
const DAYS = Number(process.env.THREATFOX_DAYS || 3);
const MIN_CONF = Number(process.env.THREATFOX_MIN_CONFIDENCE || 75);

function jpRelevant(ioc) {
  const v = String(ioc.ioc_value || '').toLowerCase();
  if (/\.jp(\b|\/|:|$)/.test(v)) return true;
  const tags = Array.isArray(ioc.tags) ? ioc.tags.join(',').toLowerCase() : '';
  if (/\bjapan\b|\bjp\b/.test(tags)) return true;
  return false;
}

export default createThreatIntelCollector({
  sourceId: 'threatfox',
  description: 'abuse.ch ThreatFox — JP-relevant recent IOCs',
  envKey: 'ABUSE_CH_AUTH_KEY',
  envFallbackKeys: ['THREATFOX_AUTH_KEY'],
  envHint: 'Set ABUSE_CH_AUTH_KEY (free at https://auth.abuse.ch/)',
  run: async (auth) => {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(URL, {
      signal: ctrl.signal,
      method: 'POST',
      headers: {
        'auth-key': auth,
        accept: 'application/json',
        'content-type': 'application/json',
      },
      body: JSON.stringify({ query: 'get_iocs', days: DAYS }),
    });
    clearTimeout(t);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const json = await res.json();
    const data = Array.isArray(json?.data) ? json.data : [];
    const jp = data.filter((d) => jpRelevant(d) && (d.confidence_level ?? 0) >= MIN_CONF);
    const features = jp.slice(0, 300).map((d, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: TOKYO },
      properties: {
        idx: i,
        ioc_id: d.id,
        ioc_value: d.ioc_value,
        ioc_type: d.ioc_type,
        threat_type: d.threat_type,
        malware: d.malware,
        malware_alias: d.malware_alias,
        first_seen: d.first_seen,
        last_seen: d.last_seen,
        confidence_level: d.confidence_level,
        reporter: d.reporter,
        reference: d.reference,
        tags: d.tags,
        source: 'threatfox',
      },
    }));
    return {
      features,
      extraMeta: {
        total_iocs: data.length,
        jp_filtered: jp.length,
        env_hint: `THREATFOX_DAYS (default ${DAYS}); THREATFOX_MIN_CONFIDENCE (default ${MIN_CONF})`,
      },
    };
  },
});
