/**
 * MLIT N07 — National Land Numerical Information: Bus Routes
 * 国土数値情報 バスルートデータ N07
 *
 *   https://nlftp.mlit.go.jp/ksj/gml/datalist/KsjTmplt-N07-v1_1.html
 *
 * Like N02, the canonical distribution is GML/Shapefile. Accept a pre-converted
 * GeoJSON URL via env (MLIT_N07_GEOJSON_URL) and fall back to mirror URLs and
 * then OSM route=bus relations.
 */

import { fetchOverpassTiled } from './_liveHelpers.js';
import { createMlitKsjCollector } from '../utils/mlitNormalizer.js';

async function tryOsmBusRoutes() {
  return fetchOverpassTiled(
    (bbox) => `relation["route"="bus"](${bbox});`,
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        route_id: `OSM_REL_${el.id}`,
        name: el.tags?.name || el.tags?.ref || null,
        operator: el.tags?.operator || el.tags?.network || null,
        ref: el.tags?.ref || null,
        from: el.tags?.from || null,
        to: el.tags?.to || null,
        route_type: 'bus',
        source: 'osm_overpass_bus_route',
      },
    }),
    { queryTimeout: 180, timeoutMs: 90_000 },
  );
}

export default createMlitKsjCollector({
  code: 'N07',
  envKey: 'MLIT_N07_GEOJSON_URL',
  mirrors: [
    'https://nlftp.mlit.go.jp/ksj/gml/data/N07/N07-11/N07-11_BusRoute.geojson',
  ],
  osmFallback: tryOsmBusRoutes,
  osmSourceTag: 'osm_overpass_bus_route',
  description: 'MLIT KSJ N07 — nationwide bus routes (government dataset, ~11 release)',
  envHint: 'Set MLIT_N07_GEOJSON_URL to a hosted GeoJSON copy of MLIT KSJ N07 for nationwide bus route geometry.',
});
