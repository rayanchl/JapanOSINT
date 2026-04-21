/**
 * CertStream — new .jp certificate issuances (Certificate Transparency log monitor).
 *
 * Data shape: non-geospatial. Each feature is a certificate event for a
 * `.jp` domain. The background subscriber (utils/certstreamBuffer.js)
 * maintains a rolling window; this collector snapshots it.
 *
 * Useful signal for first-seen Japanese infrastructure and phishing
 * lookalike detection.
 */

import { getRecentJpCerts, startCertstream } from '../utils/certstreamBuffer.js';

export default async function collectCertstreamJp() {
  // Subscriber is lazy — started on first request so server boot doesn't
  // open a WebSocket to a flaky public relay nobody is looking at.
  startCertstream();
  const events = getRecentJpCerts({ limit: 500 });

  const features = events.map((ev, i) => ({
    type: 'Feature',
    geometry: null,
    properties: {
      id: `CT_${ev.seen}_${i}`,
      cn: ev.cn,
      issuer: ev.issuer,
      jp_domains: ev.jp_domains,
      primary_domain: ev.jp_domains[0] || ev.cn,
      all_domains_count: ev.all_domains.length,
      ct_source: ev.ct_source,
      not_before: ev.not_before,
      not_after: ev.not_after,
      seen_at: new Date(ev.ts).toISOString(),
      cert_link: ev.cert_link,
      source: 'certstream',
    },
  }));

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'certstream',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Recent .jp-domain certificate issuance events from the CertStream public relay',
      ws_url: 'wss://certstream.calidog.io/',
    },
    metadata: {},
  };
}
