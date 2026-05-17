/**
 * Windy.com webcams collector (windy-webcams).
 *
 * Live: Windy Webcams API v3 filtered to Japan when WINDY_API_KEY is set.
 * No key-free fallback; without a key (or on failure) returns an honest
 * empty result — never fabricated cams. Missing key surfaces via the
 * apiCredentials map (it is already mapped there).
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

function result(features, source) {
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Windy.com webcams across Japan (requires WINDY_API_KEY)',
    },
  };
}

export default async function collectWindyWebcams() {
  const key = envFor('WINDY_API_KEY');
  if (!key) return result([], 'windy_no_key');

  const url = 'https://api.windy.com/webcams/api/v3/webcams'
    + '?limit=250&include=location,urls,images&lang=en';
  const data = await fetchJson(url, {
    timeoutMs: 15000,
    headers: { 'x-windy-api-key': key },
  });
  const cams = data?.webcams;
  if (!Array.isArray(cams)) return result([], 'windy_unavailable');

  const features = cams
    .filter((w) => w?.location?.longitude != null && w?.location?.latitude != null
      && w.location.latitude >= 24 && w.location.latitude <= 46
      && w.location.longitude >= 122 && w.location.longitude <= 146)
    .map((w) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [+w.location.longitude, +w.location.latitude] },
      properties: {
        camera_uid: `windy-${w.webcamId || w.id}`,
        title: w.title || null,
        city: w.location?.city || null,
        embed_url: w.urls?.detail || w.urls?.edit || null,
        image: w.images?.current?.preview || null,
        status: w.status || null,
        source: 'windy_api',
      },
    }));
  return result(features, 'windy_api');
}
