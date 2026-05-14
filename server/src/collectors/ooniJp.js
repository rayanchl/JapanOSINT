/**
 * OONI Explorer — JP network-interference measurements.
 *
 * Free, no auth. Endpoint:
 *   GET https://api.ooni.io/api/v1/measurements?probe_cc=JP&limit=200&order_by=measurement_start_time&order=desc
 *
 * We surface the most recent N measurements with anomaly/blocking flags.
 */

import { createThreatIntelCollector } from '../utils/threatIntelCollectorFactory.js';
import { TOKYO } from './_satelliteSeeds.js';

const URL = 'https://api.ooni.io/api/v1/measurements?probe_cc=JP&limit=200&order_by=measurement_start_time&order=desc';
const TIMEOUT_MS = 20000;

export default createThreatIntelCollector({
  sourceId: 'ooni',
  description: 'OONI Explorer — JP network-interference measurements (recent 200)',
  run: async () => {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(URL, { signal: ctrl.signal, headers: { accept: 'application/json' } });
    clearTimeout(t);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const json = await res.json();
    const arr = Array.isArray(json?.results) ? json.results : [];
    const features = arr.map((m, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: TOKYO },
      properties: {
        idx: i,
        measurement_uid: m.measurement_uid,
        report_id: m.report_id,
        test_name: m.test_name,
        input: m.input,
        probe_cc: m.probe_cc,
        probe_asn: m.probe_asn,
        anomaly: m.anomaly,
        confirmed: m.confirmed,
        failure: m.failure,
        test_start_time: m.test_start_time,
        url: m.measurement_url,
        source: 'ooni',
      },
    }));
    return { features, extraMeta: { total_results: json?.metadata?.count ?? null } };
  },
});
