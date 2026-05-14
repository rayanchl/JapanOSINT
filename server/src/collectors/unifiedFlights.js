/**
 * Unified Flights — passthrough to the in-memory snapshot maintained by
 * `utils/planeAdsbPoller.js`. The poller fuses OpenSky live ADS-B (12 s
 * cadence) with AeroDataBox NRT/HND scheduled arrivals/departures (5 min
 * cadence) into a single deduped FeatureCollection. This module exists so
 * the existing `/api/data/unified-flights` route + collector registry
 * plumbing keep working.
 */

import { getSnapshot } from '../utils/planeAdsbPoller.js';

export default async function collectUnifiedFlights() {
  return getSnapshot();
}
