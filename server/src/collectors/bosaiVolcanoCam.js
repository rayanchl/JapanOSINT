/**
 * Volcano live-camera collector (bosai-volcano-cam) — kind:'intel'.
 *
 * JMA's volcano monitoring cameras (火山監視カメラ) are served only through an
 * interactive viewer; there is no key-free machine-readable camera listing or
 * geo-feed (the documented STOCK/camera/camera.html path 404s). NIED bosai
 * likewise gates camera/observation data behind authenticated portals. To
 * honour the no-fabrication rule this emits a single portal intel item with
 * the official JMA volcano-camera viewer URL plus a reachability probe — no
 * fabricated camera points.
 *
 * NOTE: a key-free volcano-camera listing endpoint is UNVERIFIED / believed
 * absent (interactive viewer only).
 */

import { fetchHead } from './_liveHelpers.js';
import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'bosai-volcano-cam';
// JMA volcano-camera interactive viewer (火山カメラ画像).
const VIEWER = 'https://www.data.jma.go.jp/vois/data/tokyo/volcano.html';

export default async function collectBosaiVolcanoCam() {
  const live = await fetchHead(VIEWER, { timeoutMs: 8000 }).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'JMA volcano monitoring cameras (火山監視カメラ)',
      summary: 'Live monitoring-camera imagery for active Japanese volcanoes (JMA)',
      body: 'JMA serves volcano monitoring-camera imagery only via an interactive viewer; no key-free machine-readable camera listing/geo-feed is available, so no camera point features are emitted.',
      link: VIEWER,
      author: '気象庁 Japan Meteorological Agency',
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['volcano', 'camera', 'jma', 'monitoring', live ? 'reachable' : 'unreachable'],
      properties: { operator: '気象庁', reachable: !!live, machine_readable: false },
    }],
    live: !!live,
    description: 'JMA volcano monitoring-camera viewer (no public camera listing)',
  });
}
