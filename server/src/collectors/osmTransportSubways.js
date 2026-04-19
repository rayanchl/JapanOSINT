/**
 * OSM Transport Layer — Subway / Metro / Monorail / Tram (always-on)
 *
 * Dedicated nationwide Overpass pull of `station=subway`, `station=light_rail`,
 * `station=monorail`, and `railway=tram_stop`. Complements ODPT (which covers
 * Tokyo Metro + Toei + a handful of paid operators) with full OSM coverage
 * for Osaka, Nagoya, Sapporo, Sendai, Fukuoka, Kyoto, Kobe, Yokohama metros
 * as well as the Okinawa monorail, tram networks, and people-movers.
 */

import { fetchOverpassTiled } from './_liveHelpers.js';

export default async function collectOsmTransportSubways() {
  const features = await fetchOverpassTiled(
    (bbox) => [
      `node["station"="subway"](${bbox});`,
      `node["station"="light_rail"](${bbox});`,
      `node["station"="monorail"](${bbox});`,
      `node["railway"="tram_stop"](${bbox});`,
      `node["public_transport"="station"]["subway"="yes"](${bbox});`,
      `node["public_transport"="station"]["monorail"="yes"](${bbox});`,
      `node["public_transport"="station"]["tram"="yes"](${bbox});`,
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
        type: el.tags?.station || el.tags?.railway || 'subway',
        network: el.tags?.network || null,
        wikidata: el.tags?.wikidata || null,
        wheelchair: el.tags?.wheelchair || null,
        country: 'JP',
        source: 'osm_transport_subways',
      },
    }),
    { queryTimeout: 180, timeoutMs: 120_000 },
  );

  const list = features || [];
  return {
    type: 'FeatureCollection',
    features: list,
    _meta: {
      source: 'osm_transport_subways',
      fetchedAt: new Date().toISOString(),
      recordCount: list.length,
      live: list.length > 0,
      description: 'OSM always-on layer for subway / metro / monorail / tram / light_rail stops across Japan',
    },
    metadata: {},
  };
}
