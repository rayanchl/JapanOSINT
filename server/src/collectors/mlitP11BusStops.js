/**
 * MLIT P11 — National Land Numerical Information: Bus Stops
 * 国土数値情報 バス停留所 P11
 *
 *   https://nlftp.mlit.go.jp/ksj/gml/datalist/KsjTmplt-P11-v2_2.html
 *
 * ~200,000 bus stops across all operators.
 * Falls back to OSM highway=bus_stop if MLIT download is unavailable.
 */

import { fetchOverpassTiled } from './_liveHelpers.js';
import { createMlitKsjCollector } from '../utils/mlitNormalizer.js';

async function tryOsmBusStops() {
  return fetchOverpassTiled(
    (bbox) => [
      `node["highway"="bus_stop"](${bbox});`,
      `node["public_transport"="platform"]["bus"="yes"](${bbox});`,
      `node["amenity"="bus_station"](${bbox});`,
    ].join(''),
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        stop_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || 'Bus stop',
        name_ja: el.tags?.name || el.tags?.['name:ja'] || null,
        operator: el.tags?.operator || el.tags?.network || null,
        route: el.tags?.route_ref || null,
        shelter: el.tags?.shelter || null,
        wheelchair: el.tags?.wheelchair || null,
        source: 'osm_overpass_bus_stop',
      },
    }),
    { queryTimeout: 180, timeoutMs: 120_000 },
  );
}

export default createMlitKsjCollector({
  code: 'P11',
  envKey: 'MLIT_P11_GEOJSON_URL',
  mirrors: [
    'https://nlftp.mlit.go.jp/ksj/gml/data/P11/P11-22/P11-22.geojson',
    'https://nlftp.mlit.go.jp/ksj/gml/data/P11/P11-10/P11-10.geojson',
  ],
  osmFallback: tryOsmBusStops,
  osmSourceTag: 'osm_overpass_bus_stop',
  timeoutMs: 45000,
  description: 'MLIT KSJ P11 — nationwide bus stops (停留所)',
  envHint: 'Set MLIT_P11_GEOJSON_URL to a hosted GeoJSON copy of MLIT KSJ P11 for ~200k bus stops.',
});
