/**
 * HTTP-path collectors for unified transport layers.
 *
 * The real ingest (fusing MLIT + ODPT + OSM, dedupe, line-colour snapping)
 * happens in transportRunner.js on an hourly cron + boot warm-up. That path
 * persists everything into the `transport_stations` + `transport_lines`
 * SQLite tables via transportStore.js.
 *
 * These five default exports are the READ side — they just surface the DB
 * contents as a conformant { type, features, _meta } FeatureCollection so
 * /api/data/unified-* can flow through the standard respondWithData cache +
 * layer_work_* telemetry path like every other collector.
 *
 * If the DB has never been populated (fresh install, transport runner
 * hasn't fired yet), they return an empty FC with live: false so the
 * upstream cache and client can see "nothing yet" honestly.
 */

import {
  getTransportFeatureCollection, transportStats,
} from '../utils/transportStore.js';

function readMode(mode, description) {
  let fc;
  try {
    fc = getTransportFeatureCollection(mode);
  } catch (err) {
    console.warn(`[transportRead:${mode}] DB read failed:`, err?.message);
    fc = { type: 'FeatureCollection', features: [], _meta: {} };
  }
  const features = Array.isArray(fc?.features) ? fc.features : [];
  let stats = { stations: 0, lines: 0, new24h: 0 };
  try { stats = transportStats(mode); } catch { /* fresh DB */ }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: `transport_store:${mode}`,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: features.length > 0,
      description,
      db_stations: stats.stations,
      db_lines: stats.lines,
      db_new_24h: stats.new24h,
    },
  };
}

export async function collectUnifiedTrainsRead() {
  return readMode('train', 'Unified train stations + tracks from transport_store (sweep-populated DB)');
}
export async function collectUnifiedSubwaysRead() {
  return readMode('subway', 'Unified subway / tram / monorail stations + tracks from transport_store');
}
export async function collectUnifiedBusesRead() {
  return readMode('bus', 'Unified bus stops + MLIT N07 bus routes from transport_store');
}
export async function collectUnifiedAisShipsRead() {
  return readMode('ship', 'Unified AIS ship positions from transport_store');
}
export async function collectUnifiedPortInfraRead() {
  return readMode('port', 'Unified port infrastructure (berths, harbours) from transport_store');
}
export async function collectUnifiedAirportsRead() {
  return readMode('airport', 'Unified airport infrastructure (MLIT P02 + OSM aeroway) from transport_store');
}
export async function collectUnifiedFlightsRead() {
  return readMode('flight', 'Unified live flights (ADS-B + airport schedules) from transport_store');
}
