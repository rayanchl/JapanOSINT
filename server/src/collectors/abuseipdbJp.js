/**
 * AbuseIPDB blacklist — public list filtered to JP.
 *
 * Auth: ABUSEIPDB_API_KEY (free, 100 block-checks/day on free plan).
 * Endpoint: GET https://api.abuseipdb.com/api/v2/blacklist?confidenceMinimum=…
 *           AbuseIPDB does not expose a country-only filter on the free tier;
 *           we pull the global blacklist and JP-filter using the per-row
 *           `countryCode` field.
 */

import { createThreatIntelCollector } from '../utils/threatIntelCollectorFactory.js';
import { TOKYO } from './_satelliteSeeds.js';

const URL_JSON = 'https://api.abuseipdb.com/api/v2/blacklist';
const TIMEOUT_MS = 15000;
const LIMIT = Number(process.env.ABUSEIPDB_LIMIT || 10000);

export default createThreatIntelCollector({
  sourceId: 'abuseipdb',
  description: 'AbuseIPDB blacklist — JP-IP entries (confidence ≥90)',
  envKey: 'ABUSEIPDB_API_KEY',
  envHint: 'Set ABUSEIPDB_API_KEY (free at https://www.abuseipdb.com/account/api)',
  run: async (key) => {
    const url = `${URL_JSON}?confidenceMinimum=90&limit=${LIMIT}`;
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(url, {
      signal: ctrl.signal,
      headers: { Key: key, accept: 'application/json' },
    });
    clearTimeout(t);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const json = await res.json();
    const rows = Array.isArray(json?.data) ? json.data : [];
    const jp = rows.filter((r) => String(r?.countryCode || '').toUpperCase() === 'JP');
    const features = jp.slice(0, 500).map((r, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: TOKYO },
      properties: {
        idx: i,
        ip: r.ipAddress,
        country: r.countryCode,
        abuse_score: r.abuseConfidenceScore,
        last_reported: r.lastReportedAt,
        source: 'abuseipdb',
      },
    }));
    return {
      features,
      extraMeta: {
        total_rows: rows.length,
        jp_filtered: jp.length,
        env_hint: 'Free tier 100 block-checks/day; ABUSEIPDB_LIMIT to cap',
      },
    };
  },
});
