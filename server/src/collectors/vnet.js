/**
 * NIED V-net (fundamental volcano observation network) collector (vnet) —
 * kind:'intel'.
 *
 * NIED's V-net does not expose a key-free public station listing or waveform
 * JSON; access to the underlying data is via the authenticated Hi-net/V-net
 * portal (hinetwww11.bosai.go.jp, requires registration). To honour the
 * no-fabrication rule we emit a single portal intel item recording the
 * official V-net portal URL plus a best-effort reachability probe rather
 * than inventing station points.
 *
 * NOTE: a key-free V-net station/data endpoint is UNVERIFIED / believed to
 * require NIED authentication.
 */

import { fetchHead } from './_liveHelpers.js';
import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'vnet';
const PORTAL = 'https://www.vnet.bosai.go.jp/';

export default async function collectVnet() {
  const live = await fetchHead(PORTAL, { timeoutMs: 8000 }).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'NIED V-net (基盤的火山観測網)',
      summary: 'National Research Institute for Earth Science and Disaster Resilience fundamental volcano observation network',
      body: 'V-net broadband seismic / tilt / GNSS volcano observation network operated by NIED. No key-free public station or data endpoint exists (data access requires NIED registration); portal reachability only.',
      link: PORTAL,
      author: 'NIED 防災科学技術研究所',
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['volcano', 'nied', 'vnet', 'seismic', live ? 'reachable' : 'unreachable'],
      properties: { operator: 'NIED', reachable: !!live, machine_readable: false },
    }],
    live: !!live,
    description: 'NIED V-net volcano observation network portal (no public data endpoint)',
  });
}
