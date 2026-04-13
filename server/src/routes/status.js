import { Router } from 'express';
import { getAllSources } from '../utils/database.js';
import sourceRegistry from '../utils/sourceRegistry.js';
import { getCredentialStatus } from '../utils/apiCredentials.js';

const router = Router();

/**
 * Build the registry index once per request. The registry is the source of
 * truth for free/url/nameJa/description/updateInterval/layer; the DB is the
 * source of truth for runtime health (status, last_check, response_time).
 */
function buildRegistryIndex() {
  const idx = new Map();
  for (const src of sourceRegistry) idx.set(src.id, src);
  return idx;
}

// GET /api/status — per-source health + credential configuration.
router.get('/', (_req, res) => {
  try {
    const dbSources = getAllSources();
    const registryIdx = buildRegistryIndex();

    const apis = dbSources.map((row) => {
      const reg = registryIdx.get(row.id) || {};
      const creds = getCredentialStatus(row.id);

      return {
        id: row.id,
        name: row.name,
        nameJa: reg.nameJa || null,
        type: row.type,
        category: row.category,
        url: row.url,
        description: reg.description || null,
        free: reg.free ?? null,
        layer: reg.layer || null,
        updateInterval: reg.updateInterval || null,
        status: row.status,
        lastCheck: row.last_check,
        lastSuccess: row.last_success,
        responseTimeMs: row.response_time_ms,
        recordsCount: row.records_count,
        errorMessage: row.error_message,
        requiresKey: creds.requiresKey,
        configured: creds.configured,
        envVars: creds.envVars,
        missingVars: creds.missingVars,
      };
    });

    const summary = {
      total: apis.length,
      online: apis.filter((a) => a.status === 'online').length,
      degraded: apis.filter((a) => a.status === 'degraded').length,
      offline: apis.filter((a) => a.status === 'offline').length,
      requiresKey: apis.filter((a) => a.requiresKey).length,
      configured: apis.filter((a) => a.requiresKey && a.configured).length,
      missingKey: apis.filter((a) => a.requiresKey && !a.configured).length,
      working: apis.filter(
        (a) => a.status === 'online' && (!a.requiresKey || a.configured),
      ).length,
    };

    res.json({
      summary,
      apis,
      timestamp: new Date().toISOString(),
    });
  } catch (err) {
    console.error('[status] Error building status:', err.message);
    res.status(500).json({ error: 'Failed to build API status' });
  }
});

export default router;
