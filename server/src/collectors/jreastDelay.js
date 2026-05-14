/**
 * JR East delay certificates (遅延証明書).
 * https://traininfo.jreast.co.jp/delay_certificate/
 *
 * Every JR East line publishes a daily delay certificate whenever a delay
 * exceeds the threshold passengers can claim against work attendance. The
 * certificates include cause text — "human accident", "signal failure",
 * "earthquake stop" — which is far higher-resolution incident data than the
 * train operator's status dashboard. Pattern over time = chokepoint map.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchText, fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'jr-east-delay';
const PORTAL_URL = 'https://traininfo.jreast.co.jp/delay_certificate/';

export default async function collectJreastDelay() {
  // The portal is a JS SPA; surface reachability + a today-dated rolling
  // pointer so timelines / alerts can hang off the source even before we
  // parse certificates. Per-line certificates are PDFs linked off the
  // SPA; a Playwright pass can later enumerate them.
  let live = false;
  try { live = await fetchHead(PORTAL_URL); } catch { /* keep false */ }
  let bodySnippet = null;
  try {
    const html = await fetchText(PORTAL_URL, { timeoutMs: 10000 });
    bodySnippet = (html || '').replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim().slice(0, 240);
  } catch { /* ignore */ }

  const items = [{
    uid: intelUid(SOURCE_ID, 'jr-east-portal'),
    title: 'JR East delay certificates (遅延証明書)',
    summary: bodySnippet || 'Daily line-level delay certificates with cause text',
    link: PORTAL_URL,
    language: 'ja',
    published_at: new Date().toISOString(),
    tags: ['transit', 'rail', 'delay', 'jr-east', live ? 'reachable' : 'unreachable'],
    properties: { operator: 'JR East', reachable: live },
  }];

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live,
    description: 'JR East delay certificates portal (per-line daily delay cause text)',
  });
}
