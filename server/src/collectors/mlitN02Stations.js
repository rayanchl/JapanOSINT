/**
 * MLIT N02 — National Land Numerical Information: Railway Stations
 * 国土数値情報 鉄道データ N02
 *
 * MLIT publishes the authoritative nationwide rail dataset (every line and
 * station operated in Japan, updated yearly) at:
 *   https://nlftp.mlit.go.jp/ksj/gml/datalist/KsjTmplt-N02-v3_0.html
 *
 * Distribution is GML/Shapefile. We accept a pre-converted GeoJSON URL via
 * env (MLIT_N02_GEOJSON_URL) and fall through to mirrors and an OSM live
 * Overpass fallback (~10k stations at parity).
 */

import { fetchOverpassTiled } from './_liveHelpers.js';
import { createMlitKsjCollector } from '../utils/mlitNormalizer.js';

async function tryOsmStations() {
  return fetchOverpassTiled(
    (bbox) => [
      `node["railway"="station"](${bbox});`,
      `node["railway"="halt"](${bbox});`,
      `node["railway"="tram_stop"](${bbox});`,
      `node["station"="subway"](${bbox});`,
      `way["railway"="station"](${bbox});`,
    ].join(''),
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        station_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || 'Station',
        name_ja: el.tags?.name || el.tags?.['name:ja'] || null,
        line: el.tags?.line || el.tags?.network || null,
        operator: el.tags?.operator || null,
        classification: el.tags?.railway || el.tags?.station || null,
        wikidata: el.tags?.wikidata || null,
        country: 'JP',
        source: 'osm_overpass_railway',
      },
    }),
    { queryTimeout: 180, timeoutMs: 90_000 },
  );
}

export default createMlitKsjCollector({
  code: 'N02',
  envKey: 'MLIT_N02_GEOJSON_URL',
  mirrors: [
    'https://nlftp.mlit.go.jp/ksj/gml/data/N02/N02-23/N02-23_Station.geojson',
    'https://nlftp.mlit.go.jp/ksj/gml/data/N02/N02-22/N02-22_Station.geojson',
  ],
  osmFallback: tryOsmStations,
  osmSourceTag: 'osm_overpass_railway',
  description: 'MLIT KSJ N02 — authoritative nationwide rail stations (yearly-updated government dataset)',
  envHint: 'Set MLIT_N02_GEOJSON_URL to a hosted GeoJSON copy of MLIT KSJ N02 to enable full nationwide coverage (~10,000 stations).',
});
