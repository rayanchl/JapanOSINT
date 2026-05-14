/**
 * IODA (Georgia Tech) — JP internet-outage signals.
 *
 * Free, no auth. Endpoint:
 *   GET https://api.ioda.inetintel.cc.gatech.edu/v2/signals/raw/country/JP?from=…&until=…&datasource=…
 *
 * We pull a 7-day window across IODA's three core datasources (BGP,
 * active-probing, telescope) and surface time-series summaries.
 */

import { createThreatIntelCollector } from '../utils/threatIntelCollectorFactory.js';
import { TOKYO } from './_satelliteSeeds.js';

const BASE = 'https://api.ioda.inetintel.cc.gatech.edu/v2';
const TIMEOUT_MS = 15000;

async function fetchSignals(from, until, datasource) {
  const url = `${BASE}/signals/raw/country/JP?from=${from}&until=${until}&datasource=${datasource}`;
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(url, { signal: ctrl.signal, headers: { accept: 'application/json' } });
    clearTimeout(t);
    if (!res.ok) return { datasource, err: `HTTP ${res.status}` };
    return await res.json();
  } catch (err) { return { datasource, err: err?.message || 'fetch_failed' }; }
}

async function fetchEvents(from, until) {
  const url = `${BASE}/events?from=${from}&until=${until}&overall=true&relatedTo=country%2FJP`;
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(url, { signal: ctrl.signal, headers: { accept: 'application/json' } });
    clearTimeout(t);
    if (!res.ok) return { err: `HTTP ${res.status}` };
    return await res.json();
  } catch (err) { return { err: err?.message || 'fetch_failed' }; }
}

export default createThreatIntelCollector({
  sourceId: 'ioda',
  description: 'IODA — JP internet-outage signals (BGP, active probing, telescope) + events',
  run: async () => {
    const until = Math.floor(Date.now() / 1000);
    const from = until - 7 * 24 * 3600;
    const [bgp, ap, telescope, events] = await Promise.all([
      fetchSignals(from, until, 'bgp'),
      fetchSignals(from, until, 'ping-slash24'),
      fetchSignals(from, until, 'merit-nt'),
      fetchEvents(from, until),
    ]);

    const features = [];
    const summarise = (label, payload) => {
      const series = Array.isArray(payload?.data) ? payload.data : [];
      const lastVals = series.flatMap((s) => Array.isArray(s?.values) ? s.values.slice(-3) : []);
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: TOKYO },
        properties: {
          kind: 'signal',
          datasource: label,
          series_count: series.length,
          sample_values: lastVals,
          err: payload?.err || null,
          source: 'ioda_signals',
        },
      });
    };
    summarise('bgp', bgp);
    summarise('ping-slash24', ap);
    summarise('merit-nt', telescope);

    const evArr = Array.isArray(events?.data) ? events.data : [];
    evArr.slice(0, 30).forEach((e, i) => {
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: TOKYO },
        properties: {
          idx: i,
          kind: 'event',
          from: e.from,
          until: e.until,
          score: e.score,
          relevant_signals: e.relevantSignals,
          location_code: e.locationCode,
          location_name: e.locationName,
          source: 'ioda_events',
        },
      });
    });

    return { features, extraMeta: { window_days: 7 } };
  },
});
