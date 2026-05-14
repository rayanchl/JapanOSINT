/**
 * Saibansyo — Japanese court rulings database.
 * https://www.courts.go.jp/
 *
 * Public ruling index — Supreme Court + High / District / Family / Summary
 * courts. Searchable; full text PDF per ruling.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'saibansyo-rulings';
const PROBE_URL = 'https://www.courts.go.jp/app/hanrei_jp/list1';

export default async function collectSaibansyoRulings() {
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'Saibansyo (Courts of Japan) ruling search',
      summary: 'Public ruling index — Supreme Court + High / District / Family / Summary courts',
      link: PROBE_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['legal', 'courts', 'jurisprudence', live ? 'reachable' : 'unreachable'],
      properties: { operator: '裁判所', reachable: live },
    }],
    live,
    description: 'Saibansyo court rulings database',
  });
}
