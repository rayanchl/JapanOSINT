/**
 * SecurityTrails — historical DNS records.
 * https://api.securitytrails.com/v1/
 *
 * A/AAAA/MX/TXT history per domain. Complements `crtshHistorical` (cert
 * pivots) with the DNS side — catch hosts that came online before a cert
 * was issued, or that never got one.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'securitytrails-history';
const KEY_ENV = 'SECURITYTRAILS_KEY';
const PROBE_URL = 'https://securitytrails.com/';

export default async function collectSecurityTrailsHistory() {
  const hasKey = !!process.env[KEY_ENV];
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'SecurityTrails historical DNS',
      summary: hasKey ? 'Configured' : `Set ${KEY_ENV} to enable history lookups`,
      link: PROBE_URL,
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['dns', 'history', 'securitytrails', live ? 'reachable' : 'unreachable', hasKey ? 'key-present' : 'key-missing'],
      properties: { reachable: live, requires_key: true, has_key: hasKey },
    }],
    live,
    description: 'SecurityTrails — historical DNS A/MX/TXT lookups',
  });
}
