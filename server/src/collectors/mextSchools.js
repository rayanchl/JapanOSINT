/**
 * MEXT school registry (学校基本調査).
 * https://www.mext.go.jp/b_menu/toukei/chousa01/
 *
 * Every elementary / junior-high / high-school + university + technical
 * college in Japan, with address + student count + faculty count. Static
 * yearly CSV from MEXT.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'mext-schools';
const PROBE_URL = 'https://www.mext.go.jp/b_menu/toukei/chousa01/';

export default async function collectMextSchools() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'MEXT school registry (学校基本調査)',
      summary: 'Every elementary/junior/HS/college/university — yearly CSV',
      link: PROBE_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['education', 'mext', 'school-registry', live ? 'reachable' : 'unreachable'],
      properties: { operator: '文部科学省', reachable: live },
    }],
    live,
    description: 'MEXT school basic survey — registry of all schools + universities',
  });
}
