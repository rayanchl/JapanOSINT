/**
 * HTTP-path read collectors for the cross-mode unified station layer and its
 * companion station footprint layer. Both tables are populated by the
 * transport runner (see `utils/stationClusterer.js` and
 * `utils/stationFootprintsStore.js`); these endpoints are pure reads wrapped
 * in the standard { type, features, _meta } envelope so the usual
 * respondWithData cache + telemetry path can handle them.
 */

import { getAllLineDotFeatures } from '../utils/stationClusterer.js';
import {
  getAllFootprints,
  footprintCount,
} from '../utils/stationFootprintsStore.js';

/**
 * One Point feature per (cluster, line_color) pair — the point is snapped
 * onto the nearest segment of that line's geometry so the client renders
 * one colored dot directly ON each line at the station. Apple-Maps style.
 * The cluster centroid is no longer emitted separately; the popup system
 * resolves cluster data via /api/transit/station/:cluster_uid/summary.
 */
export async function collectUnifiedStationsRead() {
  let features = [];
  try {
    features = getAllLineDotFeatures();
  } catch (err) {
    console.warn('[unifiedStationsRead] line-dot read failed:', err?.message);
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'station_line_dots',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: features.length > 0,
      description:
        'One dot per (station cluster, line) pair, snapped onto the '
        + 'line geometry — Apple-Maps station dots.',
    },
  };
}

export async function collectUnifiedStationFootprintsRead() {
  let features = [];
  try {
    features = getAllFootprints();
  } catch (err) {
    console.warn('[unifiedStationsRead] footprint read failed:', err?.message);
  }
  let total = 0;
  try { total = footprintCount(); } catch { /* empty table */ }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'station_footprints',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: features.length > 0,
      description: 'Nationwide OSM station-building polygons (floor-plan fills).',
      db_total: total,
    },
  };
}
