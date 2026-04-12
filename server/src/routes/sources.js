import { Router } from 'express';
import {
  getAllSources,
  getSourceById,
  getLogsBySourceId,
  getStats,
} from '../utils/database.js';
import {
  downloadExtract,
  getExtractInfo,
  isExtractFresh,
} from '../utils/geofabrikExtract.js';

const router = Router();

// GET /api/sources/geofabrik/status - local OSM extract status
router.get('/geofabrik/status', (_req, res) => {
  const info = getExtractInfo();
  res.json({
    fresh: isExtractFresh(),
    info: info || { exists: false },
  });
});

// POST /api/sources/geofabrik/refresh - trigger a (re)download
router.post('/geofabrik/refresh', async (req, res) => {
  const force = req.query.force === '1' || req.query.force === 'true';
  const result = await downloadExtract({ force });
  if (result.ok) res.json(result);
  else res.status(502).json(result);
});

// GET /api/sources/stats - aggregate stats (must be before /:id)
router.get('/stats', (_req, res) => {
  try {
    const stats = getStats();
    res.json(stats);
  } catch (err) {
    console.error('[sources] Error fetching stats:', err.message);
    res.status(500).json({ error: 'Failed to fetch stats' });
  }
});

// GET /api/sources - list all sources
router.get('/', (_req, res) => {
  try {
    const sources = getAllSources();
    res.json(sources);
  } catch (err) {
    console.error('[sources] Error listing sources:', err.message);
    res.status(500).json({ error: 'Failed to list sources' });
  }
});

// GET /api/sources/:id - single source detail
router.get('/:id', (req, res) => {
  try {
    const source = getSourceById(req.params.id);
    if (!source) {
      return res.status(404).json({ error: 'Source not found' });
    }
    res.json(source);
  } catch (err) {
    console.error('[sources] Error fetching source:', err.message);
    res.status(500).json({ error: 'Failed to fetch source' });
  }
});

// GET /api/sources/:id/logs - fetch logs for a source
router.get('/:id/logs', (req, res) => {
  try {
    const source = getSourceById(req.params.id);
    if (!source) {
      return res.status(404).json({ error: 'Source not found' });
    }
    const limit = Math.min(parseInt(req.query.limit) || 50, 500);
    const logs = getLogsBySourceId(req.params.id, limit);
    res.json(logs);
  } catch (err) {
    console.error('[sources] Error fetching logs:', err.message);
    res.status(500).json({ error: 'Failed to fetch logs' });
  }
});

export default router;
