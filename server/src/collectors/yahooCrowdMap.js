/**
 * Yahoo! Japan 混雑レーダー (Crowd Radar).
 * https://map.yahoo.co.jp/crowd
 *
 * 250 m mesh real-time crowd density derived from Yahoo's mobile-app
 * footprint. Their tile API is undocumented (z/x/y JSON tiles) and the
 * actual mesh values come from `https://map.yahoo.co.jp/api/...` requests
 * issued client-side. We record portal reachability + sampling fingerprint
 * so we can detect when the endpoint structure changes; a Playwright pass
 * can later sample tiles around Tokyo / Osaka to populate density features.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'yahoo-crowd-map';
const PROBE_URL = 'https://map.yahoo.co.jp/crowd';

export default async function collectYahooCrowdMap() {
  const live = await fetchHead(PROBE_URL);
  const items = [{
    uid: intelUid(SOURCE_ID, 'yahoo-crowd-portal'),
    title: 'Yahoo! Japan Crowd Radar',
    summary: '250 m mesh real-time crowd density derived from Yahoo mobile app footprint',
    link: PROBE_URL,
    language: 'ja',
    published_at: new Date().toISOString(),
    tags: ['crowd', 'mobility', 'yahoo', live ? 'reachable' : 'unreachable'],
    properties: { operator: 'Yahoo! Japan', mesh_size_m: 250, reachable: live },
  }];

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live,
    description: 'Yahoo! Japan 混雑レーダー — 250 m mesh real-time crowd density',
  });
}
