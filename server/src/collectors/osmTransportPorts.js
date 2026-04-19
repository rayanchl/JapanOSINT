/**
 * OSM Transport Layer — Ports / Harbours / Ferry Terminals (always-on)
 *
 * Dedicated nationwide Overpass pull for maritime infrastructure. OSM is not
 * a source of live AIS positions, but it provides authoritative geocoded
 * anchor points (harbours, marinas, ferry terminals, lighthouses) that the
 * unified AIS fuser uses for "at berth" enrichment and as a reference layer
 * when vessel feeds are offline.
 */

import { fetchOverpassTiled } from './_liveHelpers.js';

export default async function collectOsmTransportPorts() {
  const features = await fetchOverpassTiled(
    (bbox) => [
      `node["seamark:type"="harbour"](${bbox});`,
      `node["harbour"="yes"](${bbox});`,
      `way["harbour"="yes"](${bbox});`,
      `node["amenity"="ferry_terminal"](${bbox});`,
      `way["amenity"="ferry_terminal"](${bbox});`,
      `node["leisure"="marina"](${bbox});`,
      `way["leisure"="marina"](${bbox});`,
    ].join(''),
    (el, _i, coords) => {
      const kind = el.tags?.amenity === 'ferry_terminal' ? 'ferry_terminal'
        : el.tags?.leisure === 'marina' ? 'marina'
        : 'harbour';
      return {
        type: 'Feature',
        geometry: { type: 'Point', coordinates: coords },
        properties: {
          port_id: `OSM_${el.id}`,
          name: el.tags?.['name:en'] || el.tags?.name || null,
          name_ja: el.tags?.name || el.tags?.['name:ja'] || null,
          classification: el.tags?.['seamark:harbour:category'] || kind,
          kind,
          operator: el.tags?.operator || null,
          country: 'JP',
          source: 'osm_transport_ports',
        },
      };
    },
    { queryTimeout: 120, timeoutMs: 90_000 },
  );

  const list = features || [];
  return {
    type: 'FeatureCollection',
    features: list,
    _meta: {
      source: 'osm_transport_ports',
      fetchedAt: new Date().toISOString(),
      recordCount: list.length,
      live: list.length > 0,
      description: 'OSM always-on layer for harbours, ferry terminals, and marinas around Japan (reference for AIS dedup)',
    },
    metadata: {},
  };
}
