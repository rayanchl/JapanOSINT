/**
 * OSM Transport Layer — Train Stations (always-on)
 *
 * Dedicated nationwide Overpass pull of `railway=station` and `railway=halt`
 * plus `public_transport=station` with `train=yes`. Mainline rail only —
 * subway/tram/monorail stops are handled by osmTransportSubways.js.
 *
 * Unlike the OSM fallback inside mlitN02Stations / fullTransport / odpt,
 * this collector runs unconditionally so the unified fusers can merge
 * OSM coverage alongside government data (MLIT N02, ODPT) and dedupe.
 */

import { fetchOverpassTiled } from './_liveHelpers.js';
import { computeLineColor } from './_lineColor.js';

export default async function collectOsmTransportTrains() {
  const features = await fetchOverpassTiled(
    (bbox) => [
      `node["railway"="station"](${bbox});`,
      `node["railway"="halt"](${bbox});`,
      `way["railway"="station"](${bbox});`,
      `node["public_transport"="station"]["train"="yes"](${bbox});`,
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
        type: el.tags?.station || 'railway',
        classification: el.tags?.railway || null,
        uic_ref: el.tags?.uic_ref || null,
        wikidata: el.tags?.wikidata || null,
        wheelchair: el.tags?.wheelchair || null,
        line_color: computeLineColor(el.tags, { keys: ['line', 'network', 'operator', 'name'] }),
        country: 'JP',
        source: 'osm_transport_trains',
      },
    }),
    { queryTimeout: 180, timeoutMs: 120_000 },
  );

  const list = features || [];
  return {
    type: 'FeatureCollection',
    features: list,
    _meta: {
      source: 'osm_transport_trains',
      fetchedAt: new Date().toISOString(),
      recordCount: list.length,
      live: list.length > 0,
      description: 'OSM always-on layer for nationwide mainline train stations (railway=station/halt)',
    },
    metadata: {},
  };
}
