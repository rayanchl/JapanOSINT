/**
 * NICT Atlas (NICTER darknet sensor) — emits a single portal-status intel item.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'nict-atlas';
const PROBE_URL = 'https://www.nicter.jp/atlas/';

export default async function collectNictAtlas() {
  const live = await fetchHead(PROBE_URL);
  const items = [{
    uid: intelUid(SOURCE_ID, 'nict-atlas-portal'),
    title: 'NICT NICTER Atlas — darknet sensor',
    summary: 'NICTER project darknet visualisation portal (Koganei, Tokyo)',
    link: PROBE_URL,
    language: 'ja',
    published_at: new Date().toISOString(),
    tags: ['cyber', 'darknet', 'nict', live ? 'reachable' : 'unreachable'],
    properties: { sensor_type: 'darknet', operator: 'NICT', reachable: live },
  }];

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live,
    description: 'NICT NICTER darknet sensor visualization',
  });
}
