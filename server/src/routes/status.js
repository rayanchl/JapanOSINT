import { Router } from 'express';
import { getAllSources, getSourceById, setProbeConsent } from '../utils/database.js';
import sourceRegistry from '../utils/sourceRegistry.js';
import { getCredentialStatus } from '../utils/apiCredentials.js';
import { listSources as listIntelAggregates } from '../utils/intelStore.js';
import { fetchSource } from '../utils/scheduler.js';
import { getBroadcaster } from '../utils/collectorTap.js';
import { STRIP_LAYER_IDS } from './layers.js';

const router = Router();

// Built once at module load — sourceRegistry is static, so no reason to rebuild
// this Map on every /api/status request.
const registryIdx = new Map();
for (const src of sourceRegistry) registryIdx.set(src.id, src);

function serializeRow(row, reg, creds, intelAgg) {
  const probeConsent = row.probe_consent === 1;
  const gated = creds.requiresKey && !probeConsent;
  return {
    id: row.id,
    name: row.name,
    probeConsent,
    gated,
    // Polymorphic master aggregates — every collector mirrors here so this
    // is the authoritative "how many rows do we hold for this source" count.
    // Replaces the legacy records_count which was per-source-table specific.
    intelTotal:      intelAgg?.item_count ?? 0,
    intelGeocoded:   intelAgg?.geocoded ?? 0,
    intelUngeocoded: intelAgg?.ungeocoded ?? 0,
    intelAwaitingGeo: intelAgg?.awaiting_geo ?? 0,
    intelLastFetched: intelAgg?.last_fetched ?? null,
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
    // One indexed scan per request — listIntelAggregates is a single
    // GROUP BY on intel_items + the partial geom indexes. Cheap.
    const intelByid = new Map();
    for (const a of listIntelAggregates()) intelByid.set(a.source_id, a);

    const apis = dbSources
      .filter((row) => {
        if (STRIP_LAYER_IDS.has(row.id)) return false;
        // Also drop rows whose registry layer is stripped — covers
        // ingredients whose source-id ≠ layer-id.
        const reg = registryIdx.get(row.id);
        if (reg && STRIP_LAYER_IDS.has(reg.layer)) return false;
        return true;
      })
      .map((row) => {
        const reg = registryIdx.get(row.id) || {};
        const creds = getCredentialStatus(row.id);
        return serializeRow(row, reg, creds, intelByid.get(row.id));
      });

    const summary = {
      total: apis.length,
      // Gated rows are surfaced separately and excluded from the live-state
      // counters so the user sees an honest picture: gated sources aren't
      // "offline" — they're un-probed by design until the user opts in.
      online: apis.filter((a) => !a.gated && a.status === 'online').length,
      degraded: apis.filter((a) => !a.gated && a.status === 'degraded').length,
      offline: apis.filter((a) => !a.gated && a.status === 'offline').length,
      pending: apis.filter((a) => !a.gated && a.status === 'pending').length,
      gated: apis.filter((a) => a.gated).length,
      requiresKey: apis.filter((a) => a.requiresKey).length,
      configured: apis.filter((a) => a.requiresKey && a.configured).length,
      missingKey: apis.filter((a) => a.requiresKey && !a.configured).length,
      working: apis.filter(
        (a) => !a.gated && a.status === 'online' && (!a.requiresKey || a.configured),
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

// POST /api/status/:id/probe — one-shot manual probe with configured auth.
// Reuses scheduler.fetchSource so request/timeout/error handling stay
// identical to the cron path.
router.post('/:id/probe', async (req, res) => {
  try {
    const row = getSourceById(req.params.id);
    if (!row) return res.status(404).json({ error: 'Source not found' });
    const reg = registryIdx.get(row.id);
    if (!reg || typeof reg.url !== 'string' || !/^https?:\/\//i.test(reg.url)) {
      return res.status(400).json({ error: 'Source has no probeable URL' });
    }
    await fetchSource(reg, getBroadcaster());
    const refreshed = getSourceById(row.id);
    const creds = getCredentialStatus(row.id);
    res.json(serializeRow(refreshed, reg, creds));
  } catch (err) {
    console.error('[status] manual probe failed:', err.message);
    res.status(500).json({ error: 'Probe failed', detail: err.message });
  }
});

// POST /api/status/:id/consent { allow: boolean } — toggle whether the
// scheduler is allowed to auto-probe this keyed source.
router.post('/:id/consent', (req, res) => {
  try {
    const row = getSourceById(req.params.id);
    if (!row) return res.status(404).json({ error: 'Source not found' });
    const allow = req.body?.allow === true;
    setProbeConsent(row.id, allow);
    const refreshed = getSourceById(row.id);
    const reg = registryIdx.get(row.id) || {};
    const creds = getCredentialStatus(row.id);
    res.json(serializeRow(refreshed, reg, creds));
  } catch (err) {
    console.error('[status] consent update failed:', err.message);
    res.status(500).json({ error: 'Failed to update consent' });
  }
});

export default router;
