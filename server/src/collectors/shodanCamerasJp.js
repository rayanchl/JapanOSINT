/**
 * Shodan camera-search collector (shodan-cameras-jp).
 *
 * Live: Shodan host search for common camera fingerprints in country:JP
 * when SHODAN_API_KEY is set. No key-free fallback; without a key (or on
 * failure) returns an honest empty result — never fabricated cameras.
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

const QUERY = 'country:JP (webcamXP OR "Server: yawcam" OR product:"Hipcam RealServer" '
  + 'OR title:"Network Camera" OR product:"Hikvision" OR product:"Dahua")';

function result(features, source) {
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Shodan country:JP camera search (requires SHODAN_API_KEY)',
    },
  };
}

export default async function collectShodanCamerasJp() {
  const key = envFor('SHODAN_API_KEY');
  if (!key) return result([], 'shodan_cam_no_key');

  const url = `https://api.shodan.io/shodan/host/search?key=${encodeURIComponent(key)}`
    + `&query=${encodeURIComponent(QUERY)}`;
  const data = await fetchJson(url, { timeoutMs: 20000 });
  const matches = data?.matches;
  if (!Array.isArray(matches)) return result([], 'shodan_cam_unavailable');

  const features = matches
    .filter((m) => m?.location?.longitude != null && m?.location?.latitude != null)
    .map((m) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [+m.location.longitude, +m.location.latitude] },
      properties: {
        camera_uid: `${m.ip_str}:${m.port}`,
        ip: m.ip_str,
        port: m.port,
        vendor: m.product || m._shodan?.module || null,
        city: m.location?.city || null,
        org: m.org || null,
        stream_url: `http://${m.ip_str}:${m.port}/`,
        source: 'shodan_api',
      },
    }));
  return result(features, 'shodan_api');
}
