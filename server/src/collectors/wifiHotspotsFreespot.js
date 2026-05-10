/**
 * FREESPOT venue directory.
 * The upstream listing is HTML rows of (prefecture, area, venue) text —
 * no coordinates — so this collector emits intel items rather than map
 * features. Each row lands in intel_items via the kind:'intel' branch in
 * respondWithData.
 */

import { fetchText } from './_liveHelpers.js';
import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'wifi-hotspots-freespot';
const SOURCE_URL = 'https://www.freespot.com/users/map_e.html';

async function scrape() {
  const html = await fetchText(SOURCE_URL);
  if (!html) return [];
  const items = [];
  const rowRegex = /<tr[^>]*>[\s\S]*?<td[^>]*>(.*?)<\/td>[\s\S]*?<td[^>]*>(.*?)<\/td>[\s\S]*?<td[^>]*>(.*?)<\/td>/gi;
  let match;
  while ((match = rowRegex.exec(html)) !== null) {
    const prefecture = match[1].replace(/<[^>]+>/g, '').trim();
    const area = match[2].replace(/<[^>]+>/g, '').trim();
    const venue = match[3].replace(/<[^>]+>/g, '').trim();
    if (!area && !venue) continue;
    const title = venue || area || prefecture;
    items.push({
      uid: intelUid(SOURCE_ID, `${prefecture}|${area}|${venue}`),
      title,
      summary: `FREESPOT directory entry${prefecture ? ` (${prefecture})` : ''}`,
      body: `Prefecture: ${prefecture || 'unknown'}\nArea: ${area || 'unknown'}\nVenue: ${venue || 'unknown'}`,
      link: SOURCE_URL,
      author: 'FREESPOT',
      language: 'ja',
      published_at: null,
      tags: ['free-wifi', 'freespot', `pref:${prefecture || 'unknown'}`],
      properties: { prefecture, area, venue },
    });
  }
  return items;
}

export default async function collectWifiHotspotsFreespot() {
  let items = [];
  try {
    items = await scrape();
  } catch {
    items = [];
  }
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    description: 'FREESPOT WiFi venue directory',
  });
}
