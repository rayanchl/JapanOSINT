import { Router } from 'express';
import {
  getAllSources,
  getSourceById,
  getLogsBySourceId,
  getStats,
} from '../utils/database.js';

const router = Router();

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
