/**
 * JCG e-Anshin Maritime Safety Information broadcasts.
 * https://www6.kaiho.mlit.go.jp/info/msi.html
 *
 * Companion to `jcg-navarea` — that source is the NAVAREA XI portal (the
 * Japanese Coast Guard's regional bulletin obligation under SOLAS); this
 * one is the broader e-Anshin (e-安心) MSI feed covering exercise notices,
 * missile-debris zones, sub-surface ops, channel closures.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'jcg-msi';
const PROBE_URL = 'https://www6.kaiho.mlit.go.jp/info/msi.html';

export default async function collectJcgMsi() {
  const live = await fetchHead(PROBE_URL);
  const items = [{
    uid: intelUid(SOURCE_ID, 'jcg-msi-portal'),
    title: 'JCG Maritime Safety Information (e-Anshin)',
    summary: 'Japan Coast Guard MSI broadcasts — exercises, missile-debris zones, sub-surface ops, channel closures',
    link: PROBE_URL,
    language: 'ja',
    published_at: new Date().toISOString(),
    tags: ['maritime', 'msi', 'e-anshin', 'kaiho', live ? 'reachable' : 'unreachable'],
    properties: { operator: 'Japan Coast Guard HQ', reachable: live },
  }];

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live,
    description: 'JCG Maritime Safety Information (e-Anshin) — exercise / missile-debris / sub-surface broadcasts',
  });
}
