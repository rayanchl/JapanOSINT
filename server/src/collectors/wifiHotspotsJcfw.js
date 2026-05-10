/**
 * NTT-BP "Japan Connected Free Wi-Fi" venue directory.
 * The upstream listing is HTML rows of (area, venue) text — no coordinates
 * — so this collector emits intel items rather than map features. Each row
 * lands in intel_items via the kind:'intel' branch in respondWithData.
 */

import { fetchText } from './_liveHelpers.js';
import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'wifi-hotspots-jcfw';
const SOURCE_URL = 'https://www.ntt-bp.net/jcfw/area.html';

async function scrape() {
  const html = await fetchText(SOURCE_URL);
  if (!html) return [];
  const items = [];
  const rowRegex = /<tr[^>]*>[\s\S]*?<td[^>]*>(.*?)<\/td>[\s\S]*?<td[^>]*>(.*?)<\/td>/gi;
  let match;
  while ((match = rowRegex.exec(html)) !== null) {
    const area = match[1].replace(/<[^>]+>/g, '').trim();
    const venue = match[2].replace(/<[^>]+>/g, '').trim();
    if (!area || !venue) continue;
    items.push({
      uid: intelUid(SOURCE_ID, `${area}|${venue}`),
      title: `${venue} (${area})`,
      summary: 'Free WiFi venue listed by NTT-BP Japan Connected Free Wi-Fi',
      body: `Area: ${area}\nVenue: ${venue}\nOperator: NTT-BP`,
      link: SOURCE_URL,
      author: 'NTT-BP',
      language: 'ja',
      published_at: null,
      tags: ['free-wifi', 'jcfw', `area:${area}`],
      properties: { area, venue, operator: 'NTT-BP' },
    });
  }
  return items;
}

export default async function collectWifiHotspotsJcfw() {
  let items = [];
  try {
    items = await scrape();
  } catch {
    items = [];
  }
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    description: 'NTT-BP Japan Connected Free Wi-Fi venue directory',
  });
}
