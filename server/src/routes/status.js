import { Router } from 'express';
import { getAllSources, getSourceById } from '../utils/database.js';
import sourceRegistry from '../utils/sourceRegistry.js';
import { getCredentialStatus } from '../utils/apiCredentials.js';

const router = Router();

// Built once at module load — sourceRegistry is static, so no reason to rebuild
// this Map on every /api/status request.
const registryIdx = new Map();
for (const src of sourceRegistry) registryIdx.set(src.id, src);

function serializeRow(row, reg, creds) {
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
    probeRequestUrl: row.probe_request_url,
    probeRequestMethod: row.probe_request_method,
    probeRequestHeaders: row.probe_request_headers,
    probeResponseStatus: row.probe_response_status,
    probeResponseHeaders: row.probe_response_headers,
    probeResponseBody: row.probe_response_body,
    probeKind: row.probe_kind,
  };
}

// GET /api/status — per-source health + credential configuration.
router.get('/', (_req, res) => {
  try {
    const dbSources = getAllSources();

    const apis = dbSources.map((row) => {
      const reg = registryIdx.get(row.id) || {};
      const creds = getCredentialStatus(row.id);
      return serializeRow(row, reg, creds);
    });

    const summary = {
      total: apis.length,
      online: apis.filter((a) => a.status === 'online').length,
      degraded: apis.filter((a) => a.status === 'degraded').length,
      offline: apis.filter((a) => a.status === 'offline').length,
      pending: apis.filter((a) => a.status === 'pending').length,
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

// GET /api/status/:id — single-source probe detail
router.get('/:id', (req, res) => {
  try {
    const row = getSourceById(req.params.id);
    if (!row) return res.status(404).json({ error: 'Source not found' });
    const reg = registryIdx.get(row.id) || {};
    const creds = getCredentialStatus(row.id);
    res.json(serializeRow(row, reg, creds));
  } catch (err) {
    console.error('[status] Error getting source detail:', err.message);
    res.status(500).json({ error: 'Failed to get source detail' });
  }
});

export default router;
