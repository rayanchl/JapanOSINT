/**
 * Instagram location-page directory for Japan (instagram-locations).
 *
 * Needs INSTAGRAM_SESSION_COOKIE (via envFor). Resolves the metadata of a
 * fixed set of well-known JP Instagram location pages (name, address, post
 * count). Non-spatial intel (a location directory). Without the cookie or on
 * failure we return an honest empty result — never fabricated.
 *
 * Endpoint (unverified shape, requires session cookie; subject to change):
 *   https://www.instagram.com/api/v1/locations/web_info/?location_id=<id>
 * Credential var: INSTAGRAM_SESSION_COOKIE.
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

const SOURCE_ID = 'instagram-locations';

const LOCATION_IDS = [
  '212999109', '213385402', '214074699',
  '105466816170557', '249193905',
  '213820592',  // Sapporo
  '214058107',  // Nagoya
  '213038402',  // Fukuoka
];

function envelope(items, source) {
  return {
    kind: 'intel',
    items,
    meta: {
      fetchedAt: new Date().toISOString(),
      recordCount: items.length,
      source,
      description: 'Instagram location-page directory for major JP places (requires INSTAGRAM_SESSION_COOKIE)',
    },
  };
}

export default async function collectInstagramLocations() {
  const cookie = envFor('INSTAGRAM_SESSION_COOKIE');
  if (!cookie) return envelope([], 'instagram_locations_no_key');

  const headers = {
    Cookie: cookie,
    'X-IG-App-ID': '936619743392459',
    Accept: 'application/json',
    Referer: 'https://www.instagram.com/',
  };

  const items = [];
  let gotAny = false;

  for (const locId of LOCATION_IDS) {
    let data = null;
    try {
      const url = `https://www.instagram.com/api/v1/locations/web_info/?location_id=${encodeURIComponent(locId)}&show_nearby=false`;
      data = await fetchJson(url, { timeoutMs: 15000, headers });
    } catch { data = null; }
    if (!data) continue;
    gotAny = true;

    const loc = data?.native_location_data?.location_info
      || data?.location
      || data?.native_location_data;
    if (!loc) continue;
    const name = loc.name || `Location ${locId}`;
    items.push({
      uid: `${SOURCE_ID}|${locId}`,
      title: name,
      summary: loc.address || loc.city || null,
      body: [loc.address, loc.city, loc.country].filter(Boolean).join(', ') || null,
      link: `https://www.instagram.com/explore/locations/${locId}/`,
      author: null,
      language: 'ja',
      published_at: null,
      tags: ['instagram', 'location'],
      properties: {
        location_id: locId,
        lat: loc.lat ?? null,
        lng: loc.lng ?? null,
        media_count: loc.media_count ?? data?.media_count ?? null,
        address: loc.address || null,
        source: 'instagram_web_api',
      },
    });
  }

  if (!gotAny) return envelope([], 'instagram_locations_unavailable');
  if (!items.length) return envelope([], 'instagram_locations_unavailable');
  return envelope(items, 'instagram_web_api');
}
