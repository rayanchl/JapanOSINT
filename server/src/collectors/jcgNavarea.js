/**
 * JCG NAVAREA XI warnings — Maritime Safety Information.
 * https://www6.kaiho.mlit.go.jp/JAPANNAVAREA/
 *
 * The portal is HTML-only — we ship a single intel item recording portal
 * status; can grow to per-warning entries once we parse the listing.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'jcg-navarea';
const PROBE_URL = 'https://www6.kaiho.mlit.go.jp/JAPANNAVAREA/';

export default async function collectJcgNavarea() {
  const live = await fetchHead(PROBE_URL);
  const items = [{
    uid: intelUid(SOURCE_ID, 'navarea-xi-portal'),
    title: 'JCG NAVAREA XI navigation warnings',
    summary: 'Japan Coast Guard maritime safety information for the West Pacific NAVAREA XI region',
    link: PROBE_URL,
    language: 'ja',
    published_at: new Date().toISOString(),
    tags: ['maritime', 'msi', 'navarea-xi', live ? 'reachable' : 'unreachable'],
    properties: { coordinator: 'Japan Coast Guard HQ', role: 'NAVAREA XI coordinator', reachable: live },
  }];

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live,
    description: 'JCG NAVAREA XI navigation warnings (MSI)',
  });
}
