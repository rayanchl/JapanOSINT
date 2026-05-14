/**
 * MLIT C02 — National Land Numerical Information: Ports / Harbours
 * 国土数値情報 港湾データ C02
 *
 *   https://nlftp.mlit.go.jp/ksj/gml/datalist/KsjTmplt-C02-v2_2.html
 *
 * Covers all MLIT-designated ports: International Strategic Ports,
 * International Hub Ports, Important Ports, Local Ports, and Fishing Ports.
 * Falls back to OSM seamark:type=harbour + harbour=yes if MLIT unreachable.
 */

import { fetchOverpassTiled } from './_liveHelpers.js';
import { createMlitKsjCollector } from '../utils/mlitNormalizer.js';

async function tryOsmHarbours() {
  return fetchOverpassTiled(
    (bbox) => [
      `node["seamark:type"="harbour"](${bbox});`,
      `node["harbour"="yes"](${bbox});`,
      `way["harbour"="yes"](${bbox});`,
      `node["leisure"="marina"](${bbox});`,
      `way["leisure"="marina"](${bbox});`,
    ].join(''),
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        port_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || null,
        name_ja: el.tags?.name || el.tags?.['name:ja'] || null,
        classification: el.tags?.['seamark:harbour:category'] || el.tags?.leisure || null,
        administrator: el.tags?.operator || null,
        source: 'osm_overpass_harbour',
      },
    }),
    { queryTimeout: 120, timeoutMs: 60_000 },
  );
}

export default createMlitKsjCollector({
  code: 'C02',
  envKey: 'MLIT_C02_GEOJSON_URL',
  mirrors: [
    'https://nlftp.mlit.go.jp/ksj/gml/data/C02/C02-22/C02-22.geojson',
    'https://nlftp.mlit.go.jp/ksj/gml/data/C02/C02-06/C02-06.geojson',
  ],
  osmFallback: tryOsmHarbours,
  osmSourceTag: 'osm_overpass_harbour',
  description: 'MLIT KSJ C02 — ports and harbours (International Strategic / Important / Local / Fishing)',
  envHint: 'Set MLIT_C02_GEOJSON_URL to a hosted GeoJSON copy of MLIT KSJ C02.',
});
