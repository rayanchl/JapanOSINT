/**
 * Collector Follow API
 *
 * GET /api/follow/recent?limit=500
 *   Returns the in-memory ring buffer of collector tap events so a newly
 *   connected client can paint history before the WS stream starts
 *   delivering live events.
 */

import { Router } from 'express';
import { getRecentEvents } from '../utils/collectorTap.js';

const router = Router();

router.get('/recent', (req, res) => {
  const limit = Number.parseInt(req.query.limit, 10);
  const events = getRecentEvents(Number.isFinite(limit) ? limit : 500);
  res.json({
    count: events.length,
    events,
    timestamp: new Date().toISOString(),
  });
});

export default router;
