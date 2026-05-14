/**
 * abuse.ch Feodo Tracker — botnet C2 server feed, JP-only filter.
 *
 * The recent CSV is published openly and lists active C2 IPs with country
 * code. Free, no auth (auth-key is optional for the JSON API).
 *
 * Endpoint: https://feodotracker.abuse.ch/downloads/ipblocklist.json
 */

import { createThreatIntelCollector } from '../utils/threatIntelCollectorFactory.js';
import { TOKYO } from './_satelliteSeeds.js';

const URL_JSON = 'https://feodotracker.abuse.ch/downloads/ipblocklist.json';
const TIMEOUT_MS = 12000;

export default createThreatIntelCollector({
  sourceId: 'feodo_tracker',
  description: 'abuse.ch Feodo Tracker — active botnet C2 IPs hosted in Japan',
  run: async () => {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(URL_JSON, {
      signal: ctrl.signal,
      headers: { accept: 'application/json' },
    });
    clearTimeout(t);
    let arr = [];
    let live = false;
    if (res.ok) {
      arr = await res.json();
      live = Array.isArray(arr);
    }
    const jp = (Array.isArray(arr) ? arr : []).filter((r) =>
      String(r?.country || '').toUpperCase() === 'JP'
    );
    const features = jp.map((r, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: TOKYO },
      properties: {
        idx: i,
        ip: r.ip_address,
        port: r.port,
        malware: r.malware,
        country: r.country,
        asn: r.as_number,
        asn_name: r.as_name,
        hostname: r.hostname,
        first_seen: r.first_seen,
        last_online: r.last_online,
        status: r.status,
        source: live ? 'feodo_tracker' : 'feodo_tracker_seed',
      },
    }));
    return {
      features,
      source: live ? 'feodo_tracker' : 'feodo_tracker_seed',
      extraMeta: { total: Array.isArray(arr) ? arr.length : 0 },
    };
  },
});
