/**
 * Unified Bus Network - fuses and deduplicates bus stops + terminals:
 *   - MLIT P11 bus stops (~200k points)
 *   - GTFS-JP nationwide aggregator (gtfs-data.jp)
 *   - busRoutes.js (OSM amenity=bus_station + curated highway terminals)
 *   - osmTransportBuses (always-on OSM transport layer for stops + terminals)
 *
 * Each feature is tagged with `kind: 'stop' | 'terminal'`.
 * Dedup key priority: GTFS-JP qualified stop_id -> name+coord-grid.
 */

import mlitP11BusStops from './mlitP11BusStops.js';
import gtfsJp from './gtfsJp.js';
import busRoutes from './busRoutes.js';
import osmTransportBuses from './osmTransportBuses.js';
import { mergeFeatureCollections, dedupeByKeys, countBySource } from './_dedupe.js';

function tagKind(fc, kind) {
  if (!fc || !Array.isArray(fc.features)) return fc;
  fc.features = fc.features.map(f => ({
    ...f,
    properties: { ...f.properties, kind: f.properties?.kind || kind },
  }));
  return fc;
}

export default async function collectUnifiedBuses() {
  const [p11, gtfs, routes, osm] = await Promise.allSettled([
    mlitP11BusStops(),
    gtfsJp(),
    busRoutes(),
    osmTransportBuses(),
  ]);

  const raw = mergeFeatureCollections([
    p11.status === 'fulfilled' ? tagKind(p11.value, 'stop') : null,
    gtfs.status === 'fulfilled' ? tagKind(gtfs.value, 'stop') : null,
    routes.status === 'fulfilled' ? tagKind(routes.value, 'terminal') : null,
    osm.status === 'fulfilled' ? osm.value : null,
  ]);

  const features = dedupeByKeys(raw, [
    (f) => {
      const id = f.properties?.stop_id;
      if (!id) return null;
      if (String(id).startsWith('GTFSJP_')) return id;
      return null;
    },
  ], { coordPrecision: 4 });

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'unified_buses',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      upstream: {
        'mlit-p11-bus-stops': p11.status === 'fulfilled' ? (p11.value.features?.length || 0) : 0,
        'gtfs-jp': gtfs.status === 'fulfilled' ? (gtfs.value.features?.length || 0) : 0,
        'bus-routes': routes.status === 'fulfilled' ? (routes.value.features?.length || 0) : 0,
        'osm-transport-buses': osm.status === 'fulfilled' ? (osm.value.features?.length || 0) : 0,
      },
      bySource: countBySource(features),
      description: 'Deduplicated nationwide bus stops + terminals - fused MLIT P11 + GTFS-JP + OSM transport + curated highway terminals',
    },
    metadata: {},
  };
}
