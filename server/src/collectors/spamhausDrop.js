/**
 * Spamhaus DROP — Don't Route Or Peer list (hijacked / cybercrime ASNs).
 *
 * Free, public; max 1 fetch/hour. We pull DROP+ASN-DROP and tag entries
 * known to belong to JP-registered ASNs (looked up later via APNIC; we
 * don't enrich here to avoid extra calls — just surface the raw list and
 * let the client filter or join with peeringdb-jp).
 *
 * Endpoints:
 *   https://www.spamhaus.org/drop/drop_v4.json
 *   https://www.spamhaus.org/drop/asndrop.json
 */

import { createThreatIntelCollector } from '../utils/threatIntelCollectorFactory.js';
import { TOKYO } from './_satelliteSeeds.js';

const URL_DROP = 'https://www.spamhaus.org/drop/drop_v4.json';
const URL_ASN = 'https://www.spamhaus.org/drop/asndrop.json';
const TIMEOUT_MS = 15000;

async function fetchJsonl(url) {
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const r = await fetch(url, { signal: ctrl.signal });
    clearTimeout(t);
    if (!r.ok) return [];
    const text = await r.text();
    return text.split(/\r?\n/).map((l) => l.trim()).filter(Boolean)
      .map((l) => { try { return JSON.parse(l); } catch { return null; } })
      .filter(Boolean);
  } catch { return []; }
}

export default createThreatIntelCollector({
  sourceId: 'spamhaus_drop',
  description: 'Spamhaus DROP / ASN-DROP — hijacked & cybercrime networks',
  run: async () => {
    const [drop, asn] = await Promise.all([fetchJsonl(URL_DROP), fetchJsonl(URL_ASN)]);
    const features = [];
    drop.forEach((row, i) => {
      if (!row || !row.cidr) return;
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: TOKYO },
        properties: {
          idx: i,
          kind: 'cidr',
          cidr: row.cidr,
          sblid: row.sblid || null,
          category: row.category || null,
          source: 'spamhaus_drop',
        },
      });
    });
    asn.forEach((row, i) => {
      if (!row || row.asn == null) return;
      const cc = String(row.country_code || '').toUpperCase();
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: TOKYO },
        properties: {
          idx: drop.length + i,
          kind: 'asn',
          asn: row.asn,
          as_name: row.asname || null,
          country: cc || null,
          is_jp: cc === 'JP',
          source: 'spamhaus_asndrop',
        },
      });
    });
    return {
      features,
      extraMeta: { cidr_rows: drop.length, asn_rows: asn.length },
    };
  },
});
