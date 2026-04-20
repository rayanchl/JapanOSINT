import express from 'express';
import { getLinesByMode } from '../utils/transportStore.js';

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
    const features = [];
    let truncated = false;
    for (const line of lines) {
      const coords = line?.geometry?.coordinates;
      if (!Array.isArray(coords) || coords.length < 2) continue;
      if (bbox && !lineIntersectsBbox(coords, bbox)) continue;
      if (features.length >= MAX_FEATURES) { truncated = true; break; }
      const p = line.properties || {};
      features.push({
        type: 'Feature',
        geometry: { type: 'LineString', coordinates: coords },
        properties: {
          route_id: p.line_uid || null,
          name: p.name || null,
          line_color: p.line_color || null,
        },
      });
    }
    const body = { type: 'FeatureCollection', features };
    if (truncated) body._meta = { truncated: true, limit: MAX_FEATURES };
    res.json(body);
  } catch (err) {
    console.error('[transit/routes]', err);
    res.status(500).json({ error: 'internal' });
  }
});

export default router;
