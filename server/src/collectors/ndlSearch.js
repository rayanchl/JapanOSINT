/**
 * NDL National Diet Library — OpenSearch.
 * https://iss.ndl.go.jp/api/opensearch
 *
 * Bibliographic OpenSearch over the entire NDL holdings (books, journals,
 * news photos, archives). Useful for historic context + retro-OSINT.
 * Note: `egov-laws` is registered with a related search but this is the
 * dedicated NDL catalogue probe.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'ndl-search';
const PROBE_URL = 'https://iss.ndl.go.jp/api/opensearch?title=%E6%97%A5%E6%9C%AC';

export default async function collectNdlSearch() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'NDL OpenSearch catalogue',
      summary: 'National Diet Library OpenSearch — bibliographic API',
      link: PROBE_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['library', 'ndl', 'opensearch', live ? 'reachable' : 'unreachable'],
      properties: { operator: '国立国会図書館', reachable: live },
    }],
    live,
    description: 'NDL OpenSearch — books, journals, photos, archives',
  });
}
