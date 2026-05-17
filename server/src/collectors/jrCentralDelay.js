/**
 * JR Central (JR Tokai) train status / delay info (jr-central-delay).
 *
 * JR Central publishes conventional-line and Tokaido Shinkansen status on
 * its 運行情報 portal. We best-effort fetch the public status page and emit
 * an honest intel item with reachability + a stripped text snippet. No
 * fabricated delays.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchText, fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'jr-central-delay';
const PORTAL_URL = 'https://traininfo.jr-central.co.jp/zairaisen/index.html';

export default async function collectJrCentralDelay() {
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
      uid: intelUid(SOURCE_ID, 'jr-central-portal'),
      title: 'JR Central 運行情報 (train status)',
      summary: snippet || 'JR Central conventional-line operating status and delay information',
      body: snippet || null,
      link: PORTAL_URL,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['transit', 'rail', 'delay', 'jr-central', live ? 'reachable' : 'unreachable'],
      properties: { operator: 'JR Central', reachable: live },
    }],
    live,
    description: 'JR Central train operating-status portal (reachability + snippet)',
  });
}
