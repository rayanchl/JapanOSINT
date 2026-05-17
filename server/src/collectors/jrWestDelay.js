/**
 * JR West train status / delay info (jr-west-delay).
 *
 * JR West publishes line status on its 列車運行情報 portal (a JS SPA). We
 * best-effort fetch the public status page and emit an honest intel item
 * carrying reachability + a stripped text snippet. No fabricated delays.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchText, fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'jr-west-delay';
const PORTAL_URL = 'https://trafficinfo.westjr.co.jp/';

export default async function collectJrWestDelay() {
  let live = false;
  try { live = await fetchHead(PORTAL_URL); } catch { live = false; }

  let snippet = null;
  try {
    const html = await fetchText(PORTAL_URL, { timeoutMs: 10000 });
    snippet = (html || '').replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim().slice(0, 240);
  } catch { snippet = null; }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'jr-west-portal'),
      title: 'JR West 列車運行情報 (train status)',
      summary: snippet || 'JR West line-by-line operating status and delay information',
      body: snippet || null,
      link: PORTAL_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['transit', 'rail', 'delay', 'jr-west', live ? 'reachable' : 'unreachable'],
      properties: { operator: 'JR West', reachable: live },
    }],
    live,
    description: 'JR West train operating-status portal (SPA; reachability + snippet)',
  });
}
