/**
 * JMA UV index collector (jma-uv) — kind:'intel'.
 *
 * JMA publishes the UV index ("紫外線情報") only as rendered raster maps /
 * an interactive viewer; there is no key-free machine-readable JSON endpoint
 * (every documented/guessed data path — data.jma.go.jp/env/uvhp/data/*.json,
 * bosai/uv/data/* — returns 404 as of this writing). Rather than fabricate
 * point values we emit a single portal intel item that records the official
 * UV-index product URL and a best-effort reachability probe.
 *
 * NOTE: the JSON endpoint is UNVERIFIED / believed not to exist publicly; if
 * JMA later exposes one this collector should be upgraded to parse it.
 */

import { fetchHead } from './_liveHelpers.js';
import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'jma-uv';
const PORTAL = 'https://www.jma.go.jp/jp/uv/';

export default async function collectJmaUv() {
  const live = await fetchHead(PORTAL, { timeoutMs: 8000 }).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'JMA UV index (紫外線情報)',
      summary: 'Nationwide ultraviolet index forecast and analysis maps (JMA)',
      body: 'JMA publishes the UV index only as raster maps / an interactive viewer; no key-free machine-readable JSON endpoint is available, so no point features are emitted.',
      link: PORTAL,
      author: '気象庁 Japan Meteorological Agency',
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['uv', 'jma', 'health', live ? 'reachable' : 'unreachable'],
      properties: { operator: '気象庁', reachable: !!live, machine_readable: false },
    }],
    live: !!live,
    description: 'JMA UV index portal (no public JSON endpoint — portal item only)',
  });
}
