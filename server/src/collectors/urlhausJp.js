/**
 * abuse.ch URLhaus — recent malware-distribution URLs filtered to JP.
 *
 * Auth: ABUSE_CH_AUTH_KEY (free signup at https://auth.abuse.ch/).
 * Endpoint: POST https://urlhaus-api.abuse.ch/v1/urls/recent/
 *           Returns up to 1000 recent entries; we filter to those whose
 *           host ends in .jp or whose payload metadata mentions Japan.
 */

import { createThreatIntelCollector } from '../utils/threatIntelCollectorFactory.js';
import { TOKYO } from './_satelliteSeeds.js';

const URL = 'https://urlhaus-api.abuse.ch/v1/urls/recent/';
const TIMEOUT_MS = 15000;

function isJp(entry) {
  const host = String(entry.host || entry.url || '').toLowerCase();
  return /\.jp(\b|\/|:|$)/.test(host) ||
    /\.jp\b/.test(String(entry.urlhaus_reference || '').toLowerCase());
}

export default createThreatIntelCollector({
  sourceId: 'urlhaus',
  description: 'abuse.ch URLhaus — recent JP-host malware URLs',
  envKey: 'ABUSE_CH_AUTH_KEY',
  envFallbackKeys: ['URLHAUS_AUTH_KEY'],
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
      body: JSON.stringify({}),
    });
    clearTimeout(t);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const json = await res.json();
    const all = Array.isArray(json?.urls) ? json.urls : [];
    const jp = all.filter(isJp);
    const features = jp.slice(0, 200).map((u, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: TOKYO },
      properties: {
        idx: i,
        url: u.url,
        host: u.host,
        threat: u.threat,
        tags: u.tags,
        url_status: u.url_status,
        date_added: u.date_added,
        reporter: u.reporter,
        reference: u.urlhaus_reference,
        source: 'urlhaus',
      },
    }));
    return {
      features,
      extraMeta: {
        total_recent: all.length,
        jp_filtered: jp.length,
      },
    };
  },
});
