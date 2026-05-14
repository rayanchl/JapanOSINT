/**
 * Unified Airports — airport infrastructure (not flight data).
 * Fuses:
 *   - mlitP02Airports (MLIT KSJ P02 civil + joint-use airfield polygons)
 *   - airportInfra (OSM aeroway=aerodrome / navaids / control towers)
 *
 * Dedup keys: ICAO/IATA code first, then nearest-neighbour by name.
 */

import mlitP02Airports from './mlitP02Airports.js';
import airportInfra from './airportInfra.js';
import { createUnifiedCollector } from '../utils/unifiedCollectorTemplate.js';

export default createUnifiedCollector({
  sourceId: 'unified_airports',
  description: 'Deduplicated airport infrastructure - MLIT P02 + OSM aerodromes/navaids',
  upstreams: [
    { name: 'mlit-p02-airports', fn: mlitP02Airports },
    { name: 'airport-infra',     fn: airportInfra },
  ],
  dedupeKeys: [
    (f) => f.properties?.icao || null,
    (f) => f.properties?.iata || null,
    (f) => {
      const n = (f.properties?.name || '').toLowerCase().trim();
      return n.length > 0 ? n : null;
    },
  ],
});
