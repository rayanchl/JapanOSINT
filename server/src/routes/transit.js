import express from 'express';
import { getLinesByMode } from '../utils/transportStore.js';
import { getDeparturesAt, listHydratedOperators } from '../utils/gtfsStore.js';
import { hydrateOperator } from '../utils/gtfsHydrate.js';

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

export default router;

