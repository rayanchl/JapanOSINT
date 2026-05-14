/**
 * DOCOMO Mobile Spatial Statistics (モバ空, Mobaku) — public sample.
 * https://mobaku.jp/sample/
 *
 * Hourly mesh-level population proxy derived from DOCOMO's carrier
 * positioning network. Full tier is paid; the `/sample/` directory
 * exposes a Tokyo-only sample dataset that's free to download as CSV
 * (mesh-id, hour, population). We probe the portal here; a downstream
 * pipeline can pull the CSV and write per-mesh features.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'docomo-mobaku';
const PROBE_URL = 'https://mobaku.jp/sample/';

export default async function collectDocomoMobaku() {
  const live = await fetchHead(PROBE_URL);
  const items = [{
    uid: intelUid(SOURCE_ID, 'mobaku-sample-portal'),
    title: 'DOCOMO Mobile Spatial Statistics (Mobaku sample)',
    summary: 'Hourly mesh population for Tokyo, sampled from DOCOMO Mobile Spatial Statistics (公開サンプル)',
    link: PROBE_URL,
    language: 'ja',
    published_at: new Date().toISOString(),
    tags: ['crowd', 'mobility', 'docomo', 'mobaku', live ? 'reachable' : 'unreachable'],
    properties: { operator: 'NTT DOCOMO Insight Marketing', cadence: 'hourly', reachable: live },
  }];

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live,
    description: 'DOCOMO Mobaku public sample — hourly Tokyo mesh population',
  });
}
