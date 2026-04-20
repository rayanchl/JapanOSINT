import express from 'express';
import db from '../utils/database.js';
import { getLinesByMode } from '../utils/transportStore.js';
import { getDeparturesAt, listHydratedOperators } from '../utils/gtfsStore.js';
import { hydrateOperator } from '../utils/gtfsHydrate.js';
import { getActiveTripsAt } from '../utils/gtfsActiveTrips.js';

const router = express.Router();

const VALID_MODES = new Set(['train', 'subway', 'bus']);
// Upper bound on features per response. Hit mainly by `mode=train`, where
// fused OSM fragments can produce 100k+ rows; cutting off at 5k keeps the
// response a few megabytes at most while still covering any single
// metropolitan area in detail.
const MAX_FEATURES = 5000;

function parseBbox(raw) {
  if (!raw) return null;
  const parts = String(raw).split(',').map(Number);
  if (parts.length !== 4 || parts.some((n) => !Number.isFinite(n))) return 'invalid';
  const [minLng, minLat, maxLng, maxLat] = parts;
  return { minLng, minLat, maxLng, maxLat };
}

function lineIntersectsBbox(coords, bbox) {
  for (const [lng, lat] of coords) {
    if (lng >= bbox.minLng && lng <= bbox.maxLng && lat >= bbox.minLat && lat <= bbox.maxLat) {
      return true;
    }
  }
  return false;
}

// Compact route catalogue consumed by the client-side live-vehicle simulator
// (useLiveVehicles). Only geometry + identity + color are forwarded; richer
// data lives on the unified_* layer endpoints. Optional `bbox=minLng,minLat,
// maxLng,maxLat` clips the result to a viewport; without it, responses are
// capped at MAX_FEATURES and a `_meta.truncated` flag is set.
router.get('/routes', (req, res) => {
  const mode = String(req.query.mode || '').toLowerCase();
  if (!VALID_MODES.has(mode)) {
    return res.status(400).json({ error: 'mode must be train|subway|bus' });
  }
  const bbox = parseBbox(req.query.bbox);
  if (bbox === 'invalid') {
    return res.status(400).json({ error: 'bbox must be minLng,minLat,maxLng,maxLat' });
  }
  try {
    const lines = getLinesByMode(mode);
    // Without a bbox, the first MAX_FEATURES rows cluster by insertion order
    // (typically one prefecture). Scatter the cap by sampling with a
    // deterministic-but-spread-out stride so the truncated result covers
    // Japan rather than a single corner.
    const candidates = [];
    for (const line of lines) {
      const coords = line?.geometry?.coordinates;
      if (!Array.isArray(coords) || coords.length < 2) continue;
      if (bbox && !lineIntersectsBbox(coords, bbox)) continue;
      candidates.push(line);
    }
    let picked = candidates;
    let truncated = false;
    if (candidates.length > MAX_FEATURES) {
      truncated = true;
      const stride = candidates.length / MAX_FEATURES;
      picked = new Array(MAX_FEATURES);
      for (let i = 0; i < MAX_FEATURES; i++) {
        picked[i] = candidates[Math.floor(i * stride)];
      }
    }
    const features = picked.map((line) => {
      const p = line.properties || {};
      return {
        type: 'Feature',
        geometry: { type: 'LineString', coordinates: line.geometry.coordinates },
        properties: {
          route_id: p.line_uid || null,
          name: p.name || null,
          line_color: p.line_color || null,
        },
      };
    });
    const body = { type: 'FeatureCollection', features };
    if (truncated) body._meta = { truncated: true, limit: MAX_FEATURES, total: candidates.length };
    res.json(body);
  } catch (err) {
    console.error('[transit/routes]', err);
    res.status(500).json({ error: 'internal' });
  }
});

router.post('/gtfs/hydrate/:orgId', async (req, res) => {
  const raw = String(req.params.orgId || '');
  const orgId = raw.replace(/[^a-z0-9_-]/gi, '');
  if (!orgId) return res.status(400).json({ error: 'bad orgId' });
  try {
    const result = await hydrateOperator(orgId);
    res.json({ ok: true, orgId, ...result });
  } catch (err) {
    console.error('[transit/gtfs/hydrate]', err);
    res.status(502).json({ error: 'hydrate failed', detail: err?.message });
  }
});

router.get('/gtfs/stop/:stopId/departures', (req, res) => {
  const stopId = String(req.params.stopId || '');
  if (!stopId) return res.status(400).json({ error: 'missing stopId' });
  const limit = Math.min(20, Math.max(1, Number(req.query.limit) || 5));
  const t = req.query.t ? new Date(String(req.query.t)) : new Date();
  if (isNaN(t.getTime())) return res.status(400).json({ error: 'bad t (ISO date)' });
  try {
    const departures = getDeparturesAt(stopId, t, limit);
    res.json({ stop_id: stopId, now: t.toISOString(), departures });
  } catch (err) {
    console.error('[transit/gtfs/departures]', err);
    res.status(500).json({ error: 'internal' });
  }
});

router.get('/gtfs/operators', (_req, res) => {
  try {
    res.json({ operators: listHydratedOperators() });
  } catch (err) {
    console.error('[transit/gtfs/operators]', err);
    res.status(500).json({ error: 'internal' });
  }
});

// Slice C: schedule-grounded vehicle positions. For the given wall-clock
// time `t` (ISO, default now), return every active GTFS trip with its
// projected lat/lon along its shape. Optional `bbox=minLng,minLat,
// maxLng,maxLat` clips to a viewport.
router.get('/active-trips', (req, res) => {
  const t = req.query.t ? new Date(String(req.query.t)) : new Date();
  if (Number.isNaN(t.getTime())) {
    return res.status(400).json({ error: 'bad t (ISO date)' });
  }
  const bbox = parseBbox(req.query.bbox);
  if (bbox === 'invalid') {
    return res.status(400).json({ error: 'bbox must be minLng,minLat,maxLng,maxLat' });
  }
  const limit = Math.min(1000, Math.max(1, Number(req.query.limit) || 500));
  try {
    const trips = getActiveTripsAt({ now: t, bbox: bbox || null, limit });
    res.json({ now: t.toISOString(), count: trips.length, trips });
  } catch (err) {
    console.error('[transit/active-trips]', err);
    res.status(500).json({ error: 'internal' });
  }
});

function secondsSinceMidnight(d) {
  return d.getHours() * 3600 + d.getMinutes() * 60 + d.getSeconds();
}

// Structured summary for a single station: lines served + upcoming
// departures + arrivals + service alerts. Powers the redesigned
// StationPopup in the client.
router.get('/station/:stationUid/summary', (req, res) => {
  const stationUid = String(req.params.stationUid || '');
  if (!stationUid) {
    return res.status(400).json({ error: 'missing stationUid' });
  }

  try {
    const station = db.prepare(`
      SELECT station_uid, mode, name, operator, line, lat, lon, properties
      FROM transport_stations
      WHERE station_uid = ?
    `).get(stationUid);
    if (!station) return res.status(404).json({ error: 'station not found' });

    let props = {};
    try { props = JSON.parse(station.properties); } catch { /* leave empty */ }

    // Lines served: prefer the line_colors[] / line_names[] / line_refs[]
    // arrays populated by the spatial snap. Fall back to the single-line
    // columns when snap hasn't run.
    const lineColors = Array.isArray(props.line_colors) ? props.line_colors : [];
    const lineNames = Array.isArray(props.line_names) ? props.line_names : [];
    const lineRefs = Array.isArray(props.line_refs) ? props.line_refs : [];
    const lines = lineColors.map((color, i) => ({
      color: color || null,
      name: lineNames[i] || null,
      ref: lineRefs[i] || null,
    }));
    if (lines.length === 0 && station.line) {
      lines.push({
        color: props.line_color || null,
        name: station.line,
        ref: props.line_ref || null,
      });
    }

    // Departures / arrivals are GTFS-backed. Most OSM-derived stations won't
    // have a GTFS stop_id at all — that's expected, those rows just return
    // empty arrays.
    const stopId = props.stop_id || station.station_uid;
    const nowSec = secondsSinceMidnight(new Date());

    const departures = db.prepare(`
      SELECT st.trip_id, st.stop_sequence, st.departure_sec,
             tr.headsign, tr.route_id,
             r.short_name AS route_short, r.long_name AS route_long,
             r.color AS route_color,
             rt.departure_delay_s AS delay_s
      FROM gtfs_stop_times st
      JOIN gtfs_trips tr
        ON tr.org_id = st.org_id AND tr.feed_id = st.feed_id AND tr.trip_id = st.trip_id
      LEFT JOIN gtfs_routes r
        ON r.org_id = tr.org_id AND r.feed_id = tr.feed_id AND r.route_id = tr.route_id
      LEFT JOIN gtfs_rt_trip_updates rt
        ON rt.org_id = st.org_id AND rt.trip_id = st.trip_id AND rt.stop_sequence = st.stop_sequence
      WHERE st.stop_id = ? AND st.departure_sec >= ?
      ORDER BY st.departure_sec ASC
      LIMIT 10
    `).all(stopId, nowSec);

    const arrivals = db.prepare(`
      SELECT st.trip_id, st.stop_sequence, st.arrival_sec,
             tr.headsign, tr.route_id,
             r.short_name AS route_short, r.long_name AS route_long,
             r.color AS route_color,
             rt.arrival_delay_s AS delay_s
      FROM gtfs_stop_times st
      JOIN gtfs_trips tr
        ON tr.org_id = st.org_id AND tr.feed_id = st.feed_id AND tr.trip_id = st.trip_id
      LEFT JOIN gtfs_routes r
        ON r.org_id = tr.org_id AND r.feed_id = tr.feed_id AND r.route_id = tr.route_id
      LEFT JOIN gtfs_rt_trip_updates rt
        ON rt.org_id = st.org_id AND rt.trip_id = st.trip_id AND rt.stop_sequence = st.stop_sequence
      WHERE st.stop_id = ? AND st.arrival_sec >= ?
      ORDER BY st.arrival_sec ASC
      LIMIT 10
    `).all(stopId, nowSec);

    // Alerts for any route this station is on. LIKE match against the
    // JSON-encoded route_ids column — it's small enough that a scan is fine.
    const routeIds = new Set();
    for (const row of [...departures, ...arrivals]) {
      if (row.route_id) routeIds.add(row.route_id);
    }
    const alerts = [];
    const stmtAlert = db.prepare(`
      SELECT header_text, description_text, effect
      FROM gtfs_rt_alerts
      WHERE route_ids LIKE ?
      LIMIT 10
    `);
    for (const rid of routeIds) {
      const needle = `%"${rid}"%`;
      for (const r of stmtAlert.all(needle)) alerts.push(r);
      if (alerts.length >= 10) break;
    }

    res.json({
      station: {
        station_uid: station.station_uid,
        mode: station.mode,
        name: station.name,
        operator: station.operator,
        lat: station.lat,
        lon: station.lon,
      },
      lines,
      departures: departures.map((d) => ({ ...d, wall_sec: d.departure_sec })),
      arrivals: arrivals.map((a) => ({ ...a, wall_sec: a.arrival_sec })),
      alerts: alerts.slice(0, 10),
    });
  } catch (err) {
    console.error('[transit/station/summary]', err);
    res.status(500).json({ error: 'internal' });
  }
});

// Structured vehicle info for a single active trip. trip_id arrives as
// "org|feed|tripId" (the composite emitted by getActiveTripsAt) so we
// split it to query the GTFS tables.
router.get('/vehicle/:tripId/info', (req, res) => {
  const raw = decodeURIComponent(String(req.params.tripId || ''));
  const [orgId, feedId, tripId] = raw.split('|');
  if (!orgId || !feedId || !tripId) {
    return res.status(400).json({ error: 'bad tripId (expected org|feed|trip)' });
  }
  try {
    const trip = db.prepare(`
      SELECT t.trip_id, t.route_id, t.service_id, t.headsign,
             r.short_name AS route_short, r.long_name AS route_long, r.color AS route_color
      FROM gtfs_trips t
      LEFT JOIN gtfs_routes r
        ON r.org_id = t.org_id AND r.feed_id = t.feed_id AND r.route_id = t.route_id
      WHERE t.org_id = ? AND t.feed_id = ? AND t.trip_id = ?
    `).get(orgId, feedId, tripId);
    if (!trip) return res.status(404).json({ error: 'trip not found' });

    const nowSec = secondsSinceMidnight(new Date());
    const nextStop = db.prepare(`
      SELECT st.stop_sequence, st.stop_id, st.arrival_sec, st.departure_sec,
             rt.arrival_delay_s AS delay_s
      FROM gtfs_stop_times st
      LEFT JOIN gtfs_rt_trip_updates rt
        ON rt.org_id = st.org_id AND rt.trip_id = st.trip_id AND rt.stop_sequence = st.stop_sequence
      WHERE st.org_id = ? AND st.feed_id = ? AND st.trip_id = ?
        AND st.arrival_sec >= ?
      ORDER BY st.arrival_sec ASC
      LIMIT 1
    `).get(orgId, feedId, tripId, nowSec);

    res.json({
      trip: {
        trip_id: tripId,
        org_id: orgId,
        route_id: trip.route_id,
        headsign: trip.headsign,
        route_short: trip.route_short,
        route_long: trip.route_long,
        route_color: trip.route_color ? `#${trip.route_color}` : null,
      },
      next_stop: nextStop || null,
    });
  } catch (err) {
    console.error('[transit/vehicle/info]', err);
    res.status(500).json({ error: 'internal' });
  }
});

export default router;

