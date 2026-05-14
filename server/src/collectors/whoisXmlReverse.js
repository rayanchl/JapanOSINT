/**
 * WhoisXMLAPI — bulk + reverse WHOIS.
 * https://whois.whoisxmlapi.com/
 *
 * Reverse-WHOIS by registrant string lets you enumerate every domain
 * owned by e.g. "Ministry of Defense Japan" / a megacorp. Output is
 * historical so domain churn is visible.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchHead } from './_liveHelpers.js';

const SOURCE_ID = 'whoisxml-reverse';
const KEY_ENV = 'WHOISXML_KEY';
const PROBE_URL = 'https://whois.whoisxmlapi.com/';

export default async function collectWhoisXmlReverse() {
  const hasKey = !!process.env[KEY_ENV];
  const live = await fetchHead(PROBE_URL).catch(() => false);
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [{
      uid: intelUid(SOURCE_ID, 'portal'),
      title: 'WhoisXMLAPI — bulk / reverse WHOIS',
      summary: hasKey ? 'Configured' : `Set ${KEY_ENV} to enable reverse-WHOIS queries`,
      link: PROBE_URL,
      language: 'en',
      published_at: new Date().toISOString(),
      tags: ['whois', 'reverse', 'whoisxml', live ? 'reachable' : 'unreachable', hasKey ? 'key-present' : 'key-missing'],
      properties: { reachable: live, requires_key: true, has_key: hasKey },
    }],
    live,
    description: 'WhoisXMLAPI bulk + reverse-WHOIS — registrant pivots for JP corps / ministries',
  });
}
