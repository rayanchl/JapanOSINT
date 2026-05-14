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
import { createUnifiedCollector } from '../utils/unifiedCollectorTemplate.js';

export default createUnifiedCollector({
  sourceId: 'unified_buses',
  description: 'Deduplicated nationwide bus stops + terminals - fused MLIT P11 + GTFS-JP + OSM transport + curated highway terminals',
  upstreams: [
    { name: 'mlit-p11-bus-stops',  fn: mlitP11BusStops, kind: 'stop' },
    { name: 'gtfs-jp',             fn: gtfsJp,          kind: 'stop' },
    { name: 'bus-routes',          fn: busRoutes,       kind: 'terminal' },
    { name: 'osm-transport-buses', fn: osmTransportBuses /* upstream already tags kind */ },
  ],
  dedupeKeys: [
    (f) => {
      const id = f.properties?.stop_id;
      if (!id) return null;
      if (String(id).startsWith('GTFSJP_')) return id;
      return null;
    },
  ],
  dedupeOpts: { coordPrecision: 4 },
});
