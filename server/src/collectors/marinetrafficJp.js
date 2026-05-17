/**
 * MarineTraffic Japan vessel-position collector (marinetraffic-jp).
 *
 * Live: MarineTraffic Exportvessels REST API (Japan bbox) when
 * MARINETRAFFIC_API_KEY is set. No key-free fallback; without a key (or on
 * failure) returns an honest empty result — never fabricated vessels.
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
      description: 'Live vessel positions around Japan (requires MARINETRAFFIC_API_KEY)',
    },
  };
}

export default async function collectMarinetrafficJp() {
  const key = envFor('MARINETRAFFIC_API_KEY');
  if (!key) return result([], 'marinetraffic_no_key');

  const url = `https://services.marinetraffic.com/api/exportvessels/v:8/${encodeURIComponent(key)}`
    + '/MINLAT:24/MAXLAT:46/MINLON:122/MAXLON:146/protocol:jsono';
  const data = await fetchJson(url, { timeoutMs: 20000 });
  const rows = Array.isArray(data) ? data : null;
  if (!rows) return result([], 'marinetraffic_unavailable');

  const features = rows
    .filter((r) => r.LON != null && r.LAT != null)
    .map((r) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [+r.LON, +r.LAT] },
      properties: {
        mmsi: r.MMSI,
        imo: r.IMO || null,
        vessel_name: r.SHIPNAME || null,
        ship_type: r.TYPE_NAME || r.SHIPTYPE || null,
        speed: r.SPEED != null ? +r.SPEED / 10 : null,
        heading: r.HEADING != null ? +r.HEADING : null,
        destination: r.DESTINATION || null,
        source: 'marinetraffic_api',
      },
    }));
  return result(features, 'marinetraffic_api');
}
