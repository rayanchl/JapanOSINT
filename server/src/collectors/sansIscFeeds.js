/**
 * SANS Internet Storm Center — handler diary RSS + global infocon.
 * Combines two free feeds.
 */

import { intelEnvelope, parseFeed, feedItemToIntel, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'sans-isc-feeds';
const URL_RSS = 'https://isc.sans.edu/rssfeed.xml';
const URL_INFOCON = 'https://isc.sans.edu/api/infocon?json';
const TIMEOUT_MS = 12000;

async function fetchText(url) {
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const r = await fetch(url, { signal: ctrl.signal });
    clearTimeout(t);
    if (!r.ok) return '';
    return await r.text();
  } catch { return ''; }
}

export default async function collectSansIscFeeds() {
  const [rssXml, infoconText] = await Promise.all([fetchText(URL_RSS), fetchText(URL_INFOCON)]);
  const entries = rssXml ? parseFeed(rssXml).slice(0, 20) : [];

  const items = entries.map((e) => feedItemToIntel(SOURCE_ID, e, {
    language: 'en',
    tags: ['sans-isc', 'cyber', 'diary'],
  }));

  let infocon = null;
  try { infocon = infoconText ? JSON.parse(infoconText) : null; } catch { /* ignore */ }
  if (infocon) {
    const status = infocon?.[0]?.status || infocon?.status || 'unknown';
    items.push({
      uid: intelUid(SOURCE_ID, 'infocon-current'),
      title: `Infocon level: ${status}`,
      summary: 'SANS ISC global threat-level indicator',
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['sans-isc', 'infocon', `status:${status}`],
      properties: { kind: 'infocon', infocon },
    });
  }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: rssXml.length > 0,
    description: 'SANS Internet Storm Center — handler diary + global infocon',
  });
}
