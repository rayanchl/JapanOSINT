/**
 * PSBDMP — Pastebin / paste-tier mirror search.
 * https://psbdmp.ws/api/search/
 *
 * Free anonymous search across pastebin mirrors + archive.org snapshots
 * for JP TLDs / ASN refs / kanji corp names. No key required.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'psbdmp-pastes';
const PROBE_URL = 'https://psbdmp.ws/';

export default async function collectPsbdmpPastes() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'PSBDMP — pastebin mirror search',
      summary: 'Pastebin / mirror search — JP TLD + kanji corp name queries',
      link: PROBE_URL,
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['paste', 'leak', 'psbdmp', live ? 'reachable' : 'unreachable'],
      properties: { reachable: live, requires_key: false },
    }],
    live,
    description: 'PSBDMP pastebin mirror search',
  });
}
