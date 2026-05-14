/**
 * CertStream — new .jp certificate issuances (Certificate Transparency log monitor).
 *
 * Non-spatial. Emits each cert event as an intel item (`kind:'intel'`).
 */

import { getRecentJpCerts, startCertstream } from '../utils/certstreamBuffer.js';
import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'certstream-jp';

export default async function collectCertstreamJp() {
  // Subscriber is lazy — started on first request so server boot doesn't open
  // a WebSocket to a flaky public relay nobody is looking at.
  startCertstream();
  const events = getRecentJpCerts({ limit: 500 });

  const items = events.map((ev, i) => {
    const seenIso = new Date(ev.ts).toISOString();
    return {
      uid: intelUid(SOURCE_ID, `${ev.seen}_${i}_${ev.cn}`),
      title: ev.cn,
      summary: `Cert for ${ev.jp_domains.length} .jp domain${ev.jp_domains.length !== 1 ? 's' : ''}`,
      body: `Issuer: ${ev.issuer}\nDomains: ${ev.jp_domains.join(', ')}`,
      link: ev.cert_link || null,
      language: 'en',
      published_at: seenIso,
      tags: ['ct-log', `issuer:${ev.issuer || 'unknown'}`],
      properties: {
        cn: ev.cn,
        issuer: ev.issuer,
        jp_domains: ev.jp_domains,
        primary_domain: ev.jp_domains[0] || ev.cn,
        all_domains_count: ev.all_domains.length,
        ct_source: ev.ct_source,
        not_before: ev.not_before,
        not_after: ev.not_after,
        seen_at: seenIso,
      },
    };
  });

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    description: 'Recent .jp-domain certificate issuance events from the CertStream public relay',
    extraMeta: { ws_url: 'wss://certstream.calidog.io/' },
  });
}
