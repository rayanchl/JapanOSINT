/**
 * Camera stills (native/core/camera_stills.c) — the timeline list and the
 * operator-gated raw frame.
 *
 *   GET /api/cameras/:id/stills?limit&cursor   → {data, page:{cursor}, meta}
 *   GET /api/camera-stills/:stillId/raw        → bytes, application/octet-stream
 *
 * `:id` is an intel_items uid ("511-alberta-cameras|AB--123"), a view target
 * ("…#view=views/0" — '#' MUST be percent-encoded or the browser treats the
 * rest as a fragment and the server never sees it), or a bare camera_uid. A
 * record-level id lists every view; each frame names its `view`.
 *
 * The raw route is served as octet-stream + nosniff on purpose (untrusted
 * bytes from arbitrary cameras), so the generic fetchImageObjectUrl() — which
 * refuses a non-image/* content type — cannot be used. Instead the bytes are
 * SNIFFED here and only a JPEG/PNG/GIF/WebP signature becomes a typed blob;
 * anything else is refused rather than drawn. Fetched with fetch() so the
 * session interceptor adds the bearer token (an <img src> cannot).
 */
import apiUrl from './apiUrl.js';

export function stillsPath(cameraId, { limit = 24, cursor } = {}) {
  const q = new URLSearchParams();
  if (limit) q.set('limit', String(limit));
  if (cursor) q.set('cursor', cursor);
  return `/api/cameras/${encodeURIComponent(cameraId)}/stills?${q.toString()}`;
}

/** The id to ask the stills API about for a map feature, or null. */
export function cameraIdForProperties(p = {}) {
  // A full intel uid is unambiguous and covers every keyspace (hp tables,
  // trn_* stations); a bare camera_uid is resolved server-side to
  // camera-discovery|<uid>.
  if (typeof p.uid === 'string' && p.uid.includes('|')) return p.uid;
  if (typeof p.camera_uid === 'string' && p.camera_uid) return p.camera_uid;
  return null;
}

export async function fetchStills(cameraId, opts) {
  const res = await fetch(apiUrl(stillsPath(cameraId, opts)));
  let body = null;
  try { body = await res.json(); } catch { /* not JSON */ }
  if (!res.ok) {
    throw new Error(body?.error ? `HTTP ${res.status}: ${body.error}` : `HTTP ${res.status}`);
  }
  return body;
}

/** MIME type from the first bytes, or null when it is not an image we draw. */
export function sniffImageType(u8) {
  if (!u8 || u8.length < 4) return null;
  if (u8[0] === 0xff && u8[1] === 0xd8 && u8[2] === 0xff) return 'image/jpeg';
  if (u8[0] === 0x89 && u8[1] === 0x50 && u8[2] === 0x4e && u8[3] === 0x47) return 'image/png';
  if (u8[0] === 0x47 && u8[1] === 0x49 && u8[2] === 0x46) return 'image/gif';
  if (u8.length >= 12 && u8[0] === 0x52 && u8[1] === 0x49 && u8[2] === 0x46 && u8[3] === 0x46
      && u8[8] === 0x57 && u8[9] === 0x45 && u8[10] === 0x42 && u8[11] === 0x50) return 'image/webp';
  return null;
}

/** A human sentence for a raw-frame failure; the server's code stays visible. */
export function rawErrorMessage(status, code) {
  if (status === 401) return 'Sign in to view raw frames.';
  if (status === 403) {
    return code === 'Platform operator access not configured'
      ? 'Raw frames are operator-only, and no platform operator is configured on this server.'
      : 'Raw frames are restricted to platform operators (403).';
  }
  if (status === 410) {
    return code === 'blob_evicted'
      ? 'The frame record exists but its bytes were evicted by the evidence budget (410).'
      : "The frame record exists but its bytes were not preserved — enable capture_evidence on source 'camera-stills' (410).";
  }
  if (status === 404) return 'Frame not found (404).';
  return code ? `Frame unavailable: ${code} (HTTP ${status}).` : `Frame unavailable (HTTP ${status}).`;
}

/**
 * Resolve to a blob: URL the CALLER must revoke. Rejects with an Error whose
 * `status` is the HTTP status (0 for a non-image body) and whose message is
 * rawErrorMessage()'s sentence.
 */
export async function fetchStillObjectUrl(stillId) {
  const res = await fetch(apiUrl(`/api/camera-stills/${encodeURIComponent(stillId)}/raw`));
  if (!res.ok) {
    let code = '';
    try { code = (await res.json())?.error || ''; } catch { /* not JSON */ }
    const e = new Error(rawErrorMessage(res.status, code));
    e.status = res.status;
    e.code = code;
    throw e;
  }
  const buf = new Uint8Array(await res.arrayBuffer());
  const type = sniffImageType(buf);
  if (!type) {
    const e = new Error('The stored frame is not a recognisable image; it is not displayed.');
    e.status = 0;
    throw e;
  }
  return URL.createObjectURL(new Blob([buf], { type }));
}
