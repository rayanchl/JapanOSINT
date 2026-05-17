/**
 * Instagram geotagged posts across Japan (instagram-geo).
 *
 * Needs INSTAGRAM_SESSION_COOKIE (resolved via envFor) — Instagram's location
 * feed endpoints require an authenticated session. Posts at a location carry
 * the location's lat/lng, so this is emitted as a FeatureCollection. Without
 * the cookie (or on failure) we return an honest empty result — never
 * fabricated points.
 *
 * Endpoint (unverified shape, requires session cookie; subject to change):
 *   https://www.instagram.com/api/v1/locations/web_info/?location_id=<id>
 * We iterate a fixed list of well-known JP location IDs (major cities/landmarks)
 * and emit recent media for each. Credential var: INSTAGRAM_SESSION_COOKIE.
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

const SOURCE_ID = 'instagram-geo';

// Well-known public Instagram location IDs for major JP places.
const LOCATION_IDS = [
  '212999109',  // Tokyo, Japan
  '213385402',  // Osaka, Japan
  '214074699',  // Kyoto, Japan
  '105466816170557', // Shibuya Crossing
  '249193905', // Mount Fuji
];

function result(features, source) {
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Geotagged Instagram posts at major JP locations (requires INSTAGRAM_SESSION_COOKIE)',
    },
  };
}

export default async function collectInstagramGeo() {
  const cookie = envFor('INSTAGRAM_SESSION_COOKIE');
  if (!cookie) return result([], 'instagram_geo_no_key');

  const headers = {
    Cookie: cookie,
    'X-IG-App-ID': '936619743392459',
    Accept: 'application/json',
    Referer: 'https://www.instagram.com/',
  };

  const features = [];
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
    const lat = loc?.lat ?? loc?.location?.lat;
    const lng = loc?.lng ?? loc?.location?.lng;
    if (lat == null || lng == null) continue;

    const edges = data?.native_location_data?.recent?.sections
      || data?.recent?.sections
      || [];
    for (const sec of edges) {
      const medias = sec?.layout_content?.medias || [];
      for (const mw of medias) {
        const media = mw.media || mw;
        const code = media.code || media.id;
        if (!code) continue;
        features.push({
          type: 'Feature',
          geometry: { type: 'Point', coordinates: [+lng, +lat] },
          properties: {
            shortcode: code,
            location_id: locId,
            location_name: loc?.name || null,
            caption: media?.caption?.text || null,
            owner: media?.user?.username || null,
            taken_at: media?.taken_at
              ? new Date(media.taken_at * 1000).toISOString()
              : null,
            link: `https://www.instagram.com/p/${code}/`,
            source: 'instagram_web_api',
          },
        });
      }
    }
  }

  if (!gotAny) return result([], 'instagram_geo_unavailable');
  return result(features, 'instagram_web_api');
}
