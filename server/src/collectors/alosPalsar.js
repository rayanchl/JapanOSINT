/**
 * ALOS / PALSAR collector (alos-palsar) — kind:'intel'.
 *
 * Live: JAXA ALOS PALSAR / PALSAR-2 SAR scenes. JAXA's own G-Portal browse
 * requires an authenticated session, but the ALOS-1 PALSAR archive is also
 * mirrored in the public Alaska Satellite Facility (ASF) DAAC search API
 * (no auth) which we query for scenes intersecting the Japan bbox. Emits one
 * intel item per scene (SAR raster product, not a point cloud).
 *
 * Optional credential JAXA_GPORTAL_TOKEN is read via envFor; when present a
 * note is attached, but the keyless ASF path is always attempted so this
 * collector is real-when-public and honest-empty on failure
 * ('alos-palsar_unavailable'). Never fabricated.
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';
import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'alos-palsar';
// minLon, minLat, maxLon, maxLat
const JP_BBOX = [122.0, 24.0, 146.0, 46.0];

function empty(source, description) {
  return intelEnvelope({
    sourceId: SOURCE_ID,
    items: [],
    live: false,
    description,
    extraMeta: { source },
  });
}

export default async function collectAlosPalsar() {
  // Optional — JAXA G-Portal token (not required; ASF path is keyless).
  const gportalToken = envFor('JAXA_GPORTAL_TOKEN');

  const [w, s, e, n] = JP_BBOX;
  // ASF DAAC SearchAPI (public, no auth). ALOS PALSAR L1.0/L1.1/L1.5 archive.
  const url = 'https://api.daac.asf.alaska.edu/services/search/param'
    + '?platform=ALOS'
    + `&bbox=${w},${s},${e},${n}`
    + '&maxResults=50&output=jsonlite';
  const data = await fetchJson(url, { timeoutMs: 20000 }).catch(() => null);

  // jsonlite returns { results: [...] }
  const rows = Array.isArray(data?.results) ? data.results
    : Array.isArray(data) ? data : null;
  if (!rows || rows.length === 0) {
    return empty('alos-palsar_unavailable',
      'JAXA ALOS PALSAR SAR scenes over Japan — ASF DAAC archive unavailable');
  }

  const items = rows.slice(0, 50).map((r, i) => {
    const sid = r.granuleName || r.sceneName || r.productID || `scene-${i}`;
    const acquired = r.startTime || r.sceneDate || r.processingDate || null;
    return {
      uid: intelUid(SOURCE_ID, sid),
      title: `ALOS PALSAR scene ${sid}`,
      summary: `JAXA ALOS PALSAR SAR scene acquired ${acquired || 'unknown'} over Japan.`,
      body: `ALOS PALSAR L-band SAR scene ${sid} (ASF DAAC archive) intersecting the Japan AOI. Acquired ${acquired || 'unknown'}.${gportalToken ? ' JAXA G-Portal token present (higher-tier products available via G-Portal).' : ''}`,
      link: r.url || 'https://search.asf.alaska.edu/',
      published_at: acquired || new Date().toISOString(),
      tags: ['satellite', 'alos', 'palsar', 'sar', 'jaxa', 'raster'],
      properties: {
        bbox: JP_BBOX,
        acquired,
        sensor: 'PALSAR',
        platform: 'ALOS',
        scene_id: sid,
        beam_mode: r.beamMode || null,
        polarization: r.polarization || null,
        download_url: r.url || null,
      },
    };
  });

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: true,
    description: 'JAXA ALOS PALSAR SAR scenes over Japan (ASF DAAC public archive)',
    extraMeta: { source: 'alos-palsar' },
  });
}
