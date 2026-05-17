import { Router } from 'express';
import { utimes } from 'fs/promises';
import { buildMaintenanceDigest } from '../utils/maintenanceReport.js';
import { fileURLToPath } from 'url';
import { dirname, resolve } from 'path';

const __dirname = dirname(fileURLToPath(import.meta.url));
// Touching the entry file's mtime is enough to make `node --watch` respawn
// the process. The dev script (`server/package.json`) runs the server with
// `--watch`, which is what makes this work without a separate supervisor.
const ENTRY = resolve(__dirname, '../index.js');

const router = Router();

/**
 * POST /api/admin/restart
 *
 * Used by the iOS Settings tab to force every collector to re-read its env
 * vars after the user edits an API key. The hot-update path
 * (`apiKeysStore.setKey`) only catches collectors that read `process.env`
 * at call time; many capture the value at module-load (`const X = process.env.Y`)
 * and are stuck with the original value until restart.
 *
 * Replies first, then touches the entry file's mtime in a 200 ms `setTimeout`
 * so the response actually flushes before `--watch` tears down the process.
 * No `process.exit()` — letting `--watch` drive the restart keeps the
 * shutdown clean (open sockets get FIN, scheduler timers cancel) instead of
 * dropping the runtime mid-flight.
 */
router.post('/restart', (_req, res) => {
  res.json({ ok: true, restarting: true });
  setTimeout(async () => {
    try {
      const now = new Date();
      await utimes(ENTRY, now, now);
      console.log('[admin] restart triggered (mtime touch on index.js)');
    } catch (err) {
      console.error('[admin] restart failed:', err.message);
    }
  }, 200);
});

/**
 * GET /api/admin/maintenance?hours=24
 *
 * Maintenance-pod digest for the iOS admin tab: fix-success per class /
 * source, quarantined collectors, what's awaiting PR review, what got
 * auto-fixed, and live LLM concurrency. Read-only.
 */
router.get('/maintenance', (req, res) => {
  const hours = Math.min(Math.max(Number(req.query.hours) || 24, 1), 720);
  try {
    res.json(buildMaintenanceDigest({ hours }));
  } catch (err) {
    console.error('[admin] maintenance digest failed:', err.message);
    res.status(500).json({ error: 'digest_failed' });
  }
});

export default router;
