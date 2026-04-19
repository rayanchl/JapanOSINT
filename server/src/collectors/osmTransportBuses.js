/**
 * OSM Transport Layer — Buses (always-on)
 *
 * Dedicated nationwide Overpass pull of bus stops, bus stations (terminals),
 * and bus platforms. Runs alongside MLIT P11 and GTFS-JP so the unified bus
 * fuser can dedupe OSM contributions against government + operator data.
 *
 * Each feature is tagged with `kind: 'stop' | 'terminal'` to match the
 * convention used by unifiedBuses.js.
 */

import { fetchOverpassTiled } from './_liveHelpers.js';

export default async function collectOsmTransportBuses() {
  const features = await fetchOverpassTiled(
    (bbox) => [
      `node["highway"="bus_stop"](${bbox});`,
      `node["public_transport"="platform"]["bus"="yes"](${bbox});`,
      `node["amenity"="bus_station"](${bbox});`,
      `way["amenity"="bus_station"](${bbox});`,
      `node["public_transport"="station"]["bus"="yes"](${bbox});`,
    ].join(''),
    (el, _i, coords) => {
      const isTerminal = el.tags?.amenity === 'bus_station'
        || el.tags?.public_transport === 'station';
      return {
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
          kind: isTerminal ? 'terminal' : 'stop',
          source: 'osm_transport_buses',
        },
      };
    },
    { queryTimeout: 180, timeoutMs: 150_000 },
  );

  const list = features || [];
  return {
    type: 'FeatureCollection',
    features: list,
    _meta: {
      source: 'osm_transport_buses',
      fetchedAt: new Date().toISOString(),
      recordCount: list.length,
      live: list.length > 0,
      description: 'OSM always-on layer for bus stops + terminals (highway=bus_stop, amenity=bus_station, public_transport=platform)',
    },
    metadata: {},
  };
}
