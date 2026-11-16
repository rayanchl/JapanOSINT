/**
 * The two operator probe controls on a source — ONE copy (2026-11-07).
 *
 *   POST /api/status/:id/probe              → probe_* fields + reachable/error
 *   POST /api/status/:id/consent {consent}  → {ok, id, probeConsent}
 *
 * Both are operator-gated (httpd.c). They existed twice: SourcesPanel called
 * them with a raw `fetch` that turned any refusal into "HTTP 403" and threw
 * the server's own reason away, and ApiKeysPage called them through `api`.
 * Both now go through the shared client, so a refusal carries the server's
 * error body (ApiError.detail) wherever it is shown.
 *
 * Responses are PARTIAL rows; fold them with `mergeStatusRow`.
 */
import { api } from './client.js';

export function probeSource(id) {
  return api.post(`/api/status/${encodeURIComponent(id)}/probe`);
}

/** The server reads `consent` and 400s anything else. */
export function setProbeConsent(id, consent) {
  return api.post(`/api/status/${encodeURIComponent(id)}/consent`, { consent: Boolean(consent) });
}

/**
 * Fold a probe / consent response into the /api/status row it belongs to.
 * Those responses are PARTIAL (probe: probe* fields + reachable/error;
 * consent: {ok,id,probeConsent}); replacing the row with one dropped
 * requiresKey/envVars/name, and the keyed-source lists — which filter on
 * requiresKey — then lost the row entirely.
 */
export function mergeStatusRow(rows, updated) {
  if (!Array.isArray(rows) || !updated?.id) return rows;
  const { ok: _ok, ...patch } = updated;
  return rows.map((s) => (s.id === updated.id ? { ...s, ...patch } : s));
}

/** Toast text for a probe response: the server sends `reachable` + `error`
 *  and an HTTP status, never a `status` field. */
export function probeSummary(r) {
  if (!r || typeof r !== 'object') return 'Probe: no response body';
  const code = r.probeResponseStatus != null ? `HTTP ${r.probeResponseStatus}` : null;
  if (r.reachable === true) return `Probe: reachable${code ? ` (${code})` : ''}`;
  if (r.reachable === false) return `Probe: unreachable — ${r.error || code || 'no response'}`;
  return `Probe finished${code ? ` (${code})` : ''}`;
}
