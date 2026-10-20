/**
 * Camera visual search (native/core/visualsearchapi.h) — cameras by what their
 * frames SHOW, over public-infrastructure cameras only.
 *
 *   GET  /api/cameras/visual-search?q=&k=&bbox=&since=
 *   POST /api/cameras/visual-search/by-image   {"still_id": "...", k?, bbox?, since?}
 *
 * Both answer {data:[{camera_id, item_uid, view, name, lat, lon, distance,
 * cosine, still|null, frame:{kind, ref_id, captured_at, sha256, evidence_id},
 * frames_matched}], meta:{cameras:{note}, candidates:{exhausted}, …}}. A hit
 * on a camera's weekly reference frame has `still` null and frame.kind
 * "refframe"; its bytes are an evidence blob, not a still.
 *
 * A 503 is the server saying WHY it cannot answer (no vision sidecar, the
 * index not built yet, a model change refused) — that sentence is shown, not
 * a generic failure. Thumbnails go through cameraStills.js's fetch+sniff path
 * (the raw routes are operator-gated octet-stream), so a 401/403 becomes a
 * readable reason instead of a broken image.
 */
import apiUrl from './apiUrl.js';

export const VISUAL_SEARCH_K = 12;

export function visualSearchPath(q, { k = VISUAL_SEARCH_K, bbox, since } = {}) {
  const p = new URLSearchParams();
  p.set('q', q);
  if (k) p.set('k', String(k));
  if (bbox) p.set('bbox', Array.isArray(bbox) ? bbox.join(',') : bbox);
  if (since) p.set('since', since);
  return `/api/cameras/visual-search?${p.toString()}`;
}

/** A human sentence for a failed search; the server's reason stays visible. */
export function visualSearchErrorMessage(status, body) {
  const reason = body?.reason || '';
  if (status === 401) return 'Sign in to search cameras.';
  if (status === 403) return reason ? `Not allowed: ${reason}` : 'Not allowed (403).';
  if (status === 503) {
    return reason
      ? `Visual search is unavailable: ${reason}`
      : 'Visual search is unavailable on this server (503).';
  }
  if (status === 409 || status === 404 || status === 400 || status === 415) {
    return reason ? `${reason} (${body?.error || status})` : `${body?.error || 'Request refused'} (HTTP ${status}).`;
  }
  return body?.error ? `Search failed: ${body.error} (HTTP ${status}).` : `Search failed (HTTP ${status}).`;
}

async function answer(res) {
  let body = null;
  try { body = await res.json(); } catch { /* not JSON */ }
  if (!res.ok) {
    const e = new Error(visualSearchErrorMessage(res.status, body));
    e.status = res.status;
    e.code = body?.error || '';
    throw e;
  }
  return body;
}

export async function fetchVisualSearch(q, opts) {
  return answer(await fetch(apiUrl(visualSearchPath(q, opts))));
}

/** Cameras whose frames look like this still (image→image). */
export async function fetchSimilarToStill(stillId, { k = VISUAL_SEARCH_K } = {}) {
  return answer(await fetch(apiUrl('/api/cameras/visual-search/by-image'), {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ still_id: stillId, k }),
  }));
}

/** "N of M cameras indexed" plus whether the candidate pool ran out early. */
export function coverageNote(meta) {
  if (!meta) return '';
  const parts = [];
  if (meta.cameras?.note) parts.push(meta.cameras.note);
  if (meta.candidates && meta.candidates.exhausted === false) {
    parts.push(`best of the ${meta.candidates.examined} nearest frames examined — more may match`);
  }
  return parts.join(' · ');
}
