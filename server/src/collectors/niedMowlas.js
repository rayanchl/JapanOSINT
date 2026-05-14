/**
 * NIED MOWLAS — Monitoring of Waves on Land and Seafloor.
 * https://www.mowlas.bosai.go.jp/
 *
 * Unifies the K-NET / KiK-net / Hi-net / F-net / S-net / DONET seismograph
 * networks operated by NIED. Distinct from `k-net` and `hi-net` registered
 * separately — this is the top-level merged portal.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'nied-mowlas';
const PROBE_URL = 'https://www.mowlas.bosai.go.jp/';

export default async function collectNiedMowlas() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'NIED MOWLAS unified seismograph portal',
      summary: 'K-NET + KiK-net + Hi-net + F-net + S-net + DONET combined',
      link: PROBE_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['seismic', 'nied', 'mowlas', live ? 'reachable' : 'unreachable'],
      properties: { operator: 'NIED', reachable: live },
    }],
    live,
    description: 'NIED MOWLAS — unified land + seafloor seismograph portal',
  });
}
