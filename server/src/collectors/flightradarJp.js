/**
 * FlightRadar24 Japan live-aircraft collector (flightradar-jp).
 *
 * Live: FR24 public zone feed for the Japan bounding box (no key required).
 * On failure the collector returns an honest empty result — never
 * fabricated aircraft. Key-free, so it is not in the apiCredentials map.
 */

import { fetchJson } from './_liveHelpers.js';

// FR24 feed bounds order: lat_max, lat_min, lon_min, lon_max
const BOUNDS = '46,24,122,146';
const FEED_URL = `https://data-live.flightradar24.com/zones/fcgi/feed.js?bounds=${BOUNDS}`
  + '&faa=1&satellite=1&mlat=1&flarm=1&adsb=1&gnd=1&air=1&vehicles=1&estimated=1&maxage=14400';

export default async function collectFlightradarJp() {
  const features = [];
  const data = await fetchJson(FEED_URL, {
    timeoutMs: 12000,
    headers: { Referer: 'https://www.flightradar24.com/' },
  });

  if (data && typeof data === 'object') {
    for (const [k, v] of Object.entries(data)) {
      if (k === 'full_count' || k === 'version' || !Array.isArray(v)) continue;
      // FR24 row: [icao,lat,lon,track,alt,spd,...,reg(9),type(8),...,from(11),to(12),...,callsign(16)]
      const lat = +v[1];
      const lon = +v[2];
      if (!Number.isFinite(lat) || !Number.isFinite(lon)) continue;
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [lon, lat] },
        properties: {
          flight_id: k,
          callsign: v[16] || v[13] || null,
          registration: v[9] || null,
          aircraft: v[8] || null,
          origin: v[11] || null,
          destination: v[12] || null,
          altitude_ft: Number.isFinite(+v[4]) ? +v[4] : null,
          speed_kt: Number.isFinite(+v[5]) ? +v[5] : null,
          track: Number.isFinite(+v[3]) ? +v[3] : null,
          source: 'fr24_feed',
        },
      });
    }
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: features.length ? 'fr24_feed' : 'fr24_unavailable',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Live aircraft positions over Japan via FR24 zone feed (key-free)',
    },
  };
}
