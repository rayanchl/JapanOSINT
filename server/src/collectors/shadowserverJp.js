/**
 * Shadowserver Foundation — public dashboard time-series API.
 *
 * Polls daily counts of Japan-located compromised hosts across the
 * highest-signal datasets: SSL vulnerabilities, ICS scanning sources,
 * RDP brute-force sources, generic compromised IPs, and CVE-tagged
 * vulnerable hosts.
 *
 * Endpoint:
 *   GET https://dashboard.shadowserver.org/api/?endpoint=stats/time-series&dataset=…&geo=JP&days=30&format=json
 *
 * Free, no auth needed for the public dashboard endpoints.
 */

import { createThreatIntelCollector } from '../utils/threatIntelCollectorFactory.js';
import { TOKYO } from './_satelliteSeeds.js';

const BASE = 'https://dashboard.shadowserver.org/api/';
const TIMEOUT_MS = 15000;

const DATASETS = (process.env.SHADOWSERVER_DATASETS || [
  'compromised_website',
  'ics',
  'sinkhole_http',
  'scan_rdp',
  'scan_smb',
  'ssl_freak_vulnerable',
  'ssl_poodle_vulnerable',
  'open_elasticsearch',
  'open_redis',
  'open_mongodb',
].join(',')).split(',').map((s) => s.trim()).filter(Boolean);

const DAYS = Number(process.env.SHADOWSERVER_DAYS || 14);

async function fetchOne(dataset) {
  const url = `${BASE}?endpoint=stats/time-series&dataset=${encodeURIComponent(dataset)}&geo=JP&days=${DAYS}&format=json`;
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(url, { signal: ctrl.signal, headers: { accept: 'application/json' } });
    clearTimeout(t);
    if (!res.ok) return { dataset, err: `HTTP ${res.status}`, points: [] };
    const json = await res.json();
    const arr = Array.isArray(json) ? json : (json?.results || []);
    return { dataset, points: arr };
  } catch (err) {
    return { dataset, err: err?.message || 'fetch_failed', points: [] };
  }
}

export default createThreatIntelCollector({
  sourceId: 'shadowserver_dashboard',
  description: 'Shadowserver public time-series — JP compromised-host counts per dataset',
  run: async () => {
    const results = await Promise.all(DATASETS.map(fetchOne));
    const features = [];
    for (const r of results) {
      const last = Array.isArray(r.points) && r.points.length
        ? r.points[r.points.length - 1] : null;
      if (last) {
        features.push({
          type: 'Feature',
          geometry: { type: 'Point', coordinates: TOKYO },
          properties: {
            dataset: r.dataset,
            date: last.date || last.timestamp || null,
            count: last.count ?? last.value ?? null,
            unique_ips: last.unique_ips ?? null,
            series_length: r.points.length,
            source: 'shadowserver_dashboard',
          },
        });
      } else {
        features.push({
          type: 'Feature',
          geometry: { type: 'Point', coordinates: TOKYO },
          properties: {
            dataset: r.dataset,
            err: r.err || 'empty',
            source: 'shadowserver_dashboard_empty',
          },
        });
      }
    }
    return {
      features,
      extraMeta: {
        datasets_polled: DATASETS.length,
        env_hint: 'SHADOWSERVER_DATASETS to override; SHADOWSERVER_DAYS for window',
      },
    };
  },
});
