/**
 * HTTP-path collector for /api/data/cameras.
 *
 * The real discovery fan-out (OSM + JMA + MLIT + expressway + broadcast +
 * Insecam + Shodan + aggregator scrapers, 23 channels total) runs in the
 * background via cameraRunner.js + scheduler.js's hourly cron. Results are
 * deduplicated and persisted into the SQLite `cameras` table.
 *
 * This read-side collector surfaces that table as a conformant
 * FeatureCollection so respondWithData can cache it + emit layer_work_*
 * telemetry like every other collector. No live scraping happens on HTTP
 * requests — toggling the Cameras layer is always a millisecond DB read.
 */

import { getAllCameras, cameraStats } from '../utils/cameraStore.js';
import { isRunInFlight } from '../utils/cameraRunner.js';

export default async function collectCameras() {
  let fc;
  try {
    fc = getAllCameras();
  } catch (err) {
    console.warn('[cameras] DB read failed:', err?.message);
    fc = { type: 'FeatureCollection', features: [], _meta: {} };
  }
  const features = Array.isArray(fc?.features) ? fc.features : [];

  let stats = { total: 0, new24h: 0, byType: {} };
  try { stats = cameraStats(); } catch { /* fresh DB */ }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'camera_store',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: features.length > 0,
      description: 'Japan webcams + CCTV discovered by the camera-discovery fan-out (23 channels), deduplicated in the cameras SQLite table',
      run_in_flight: isRunInFlight(),
      db_total: stats.total,
      db_new_24h: stats.new24h,
      by_type: stats.byType,
    },
  };
}
