/**
 * MLIT P02 — National Land Numerical Information: Airports
 * 国土数値情報 空港データ P02
 *
 *   https://nlftp.mlit.go.jp/ksj/gml/datalist/KsjTmplt-P02-v1_2.html
 *
 * All civilian and joint-use airfields in Japan as polygons.
 * Falls back to OSM aeroway=aerodrome if MLIT file not reachable.
 */

import { fetchOverpassTiled } from './_liveHelpers.js';
import { createMlitKsjCollector } from '../utils/mlitNormalizer.js';

async function tryOsmAirports() {
  return fetchOverpassTiled(
    (bbox) => [
      `node["aeroway"="aerodrome"](${bbox});`,
      `way["aeroway"="aerodrome"](${bbox});`,
    ].join(''),
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        airport_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || null,
        icao: el.tags?.icao || null,
        iata: el.tags?.iata || null,
        classification: el.tags?.aerodrome || el.tags?.military ? 'joint' : 'civilian',
        source: 'osm_overpass_airport',
      },
    }),
    { queryTimeout: 120, timeoutMs: 60_000 },
  );
}

export default createMlitKsjCollector({
  code: 'P02',
  envKey: 'MLIT_P02_GEOJSON_URL',
  mirrors: [
    'https://nlftp.mlit.go.jp/ksj/gml/data/P02/P02-22/P02-22.geojson',
    'https://nlftp.mlit.go.jp/ksj/gml/data/P02/P02-13/P02-13.geojson',
  ],
  osmFallback: tryOsmAirports,
  osmSourceTag: 'osm_overpass_airport',
  description: 'MLIT KSJ P02 — nationwide airports / airfields',
  envHint: 'Set MLIT_P02_GEOJSON_URL to a hosted GeoJSON copy of MLIT KSJ P02.',
});
