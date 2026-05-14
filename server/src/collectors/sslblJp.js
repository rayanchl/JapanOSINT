/**
 * abuse.ch SSL Blacklist (SSLBL) — malicious SSL certificates.
 *
 * Public CSV is auth-free; we pull and surface entries whose listing host /
 * IP geolocates to JP-AS via the included AS info (when available).
 *
 * Endpoint:
 *   https://sslbl.abuse.ch/blacklist/sslipblacklist.csv    (IP:port + dst country)
 */

import { createThreatIntelCollector } from '../utils/threatIntelCollectorFactory.js';
import { TOKYO } from './_satelliteSeeds.js';

const URL_IP_CSV = 'https://sslbl.abuse.ch/blacklist/sslipblacklist.csv';
const FEODO_JSON = 'https://feodotracker.abuse.ch/downloads/ipblocklist.json';
const TIMEOUT_MS = 12000;

function parseCsv(text) {
  const out = [];
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trim();
    if (!line || line.startsWith('#')) continue;
    const cols = line.split(',').map((c) => c.replace(/^"|"$/g, ''));
    if (cols.length < 4) continue;
    out.push({
      first_seen: cols[0],
      ip: cols[1],
      port: cols[2],
      malware: cols[3],
    });
  }
  return out;
}

// SSLBL csv has no country; intersect against Feodo's broader JSON list
// to flag JP-located rows.
async function fetchJpIps() {
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const r = await fetch(FEODO_JSON, { signal: ctrl.signal });
    clearTimeout(t);
    if (!r.ok) return new Set();
    const arr = await r.json();
    const set = new Set();
    if (Array.isArray(arr)) {
      for (const row of arr) {
        if (String(row?.country || '').toUpperCase() === 'JP') set.add(String(row.ip_address));
      }
    }
    return set;
  } catch { return new Set(); }
}

export default createThreatIntelCollector({
  sourceId: 'sslbl',
  description: 'abuse.ch SSLBL — malicious-cert IPs intersected with JP-host set',
  run: async () => {
    let csv = '';
    let live = false;
    try {
      const ctrl = new AbortController();
      const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
      const res = await fetch(URL_IP_CSV, { signal: ctrl.signal });
      clearTimeout(t);
      if (res.ok) {
        csv = await res.text();
        live = csv.length > 0;
      }
    } catch { /* fall through to seed mode */ }

    const rows = parseCsv(csv);
    const jpIps = await fetchJpIps();
    const jp = jpIps.size > 0 ? rows.filter((r) => jpIps.has(r.ip)) : [];
    const features = jp.map((r, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: TOKYO },
      properties: {
        idx: i,
        first_seen: r.first_seen,
        ip: r.ip,
        port: r.port,
        malware: r.malware,
        source: live ? 'sslbl' : 'sslbl_seed',
      },
    }));
    return {
      features,
      source: live ? 'sslbl' : 'sslbl_seed',
      extraMeta: { total_rows: rows.length, jp_intersect: jpIps.size },
    };
  },
});
