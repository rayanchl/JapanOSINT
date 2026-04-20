import express from 'express';
import { getLinesByMode } from '../utils/transportStore.js';

const router = express.Router();

const VALID_MODES = new Set(['train', 'subway', 'bus']);

// Compact route catalogue consumed by the client-side live-vehicle simulator
// (useLiveVehicles). Only geometry + identity + color are forwarded; richer
// data lives on the unified_* layer endpoints.
router.get('/routes', (req, res) => {
  const mode = String(req.query.mode || '').toLowerCase();
  if (!VALID_MODES.has(mode)) {
    return res.status(400).json({ error: 'mode must be train|subway|bus' });
  }
  try {
    const lines = getLinesByMode(mode);
    const features = [];
    for (const line of lines) {
      const coords = line?.geometry?.coordinates;
      if (!Array.isArray(coords) || coords.length < 2) continue;
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
    res.json({ type: 'FeatureCollection', features });
  } catch (err) {
    console.error('[transit/routes]', err);
    res.status(500).json({ error: 'internal' });
  }
});

export default router;
