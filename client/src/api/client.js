/**
 * Small typed-ish HTTP helper for the JapanOSINT backend. Every call goes
 * through `window.fetch`, which the auth interceptor (`auth/session.js`)
 * wraps to attach the bearer + tenant header and retry once on 401.
 *
 * Errors are thrown, never swallowed into an empty result: a page that gets
 * an `ApiError` must render it as a failed request, not as "nothing here"
 * (house rule 1 — a failed fetch is not a statement about the data).
 */
import apiUrl from '../utils/apiUrl.js';

export class ApiError extends Error {
  constructor(status, message, body) {
    super(message || `HTTP ${status}`);
    this.name = 'ApiError';
    this.status = status;
    this.body = body;
  }
  /** The server's own error string, when it sent one. */
  get detail() {
    const b = this.body;
    if (b && typeof b === 'object') return b.detail || b.error || b.message || this.message;
    return this.message;
  }
}

async function parse(res) {
  const text = await res.text();
  if (!text) return null;
  const ct = res.headers.get('content-type') || '';
  if (ct.includes('json') || /^[[{]/.test(text.trim())) {
    try { return JSON.parse(text); } catch { return text; }
  }
  return text;
}

async function request(method, path, { body, headers, query, signal, raw } = {}) {
  let url = path;
  if (query && typeof query === 'object') {
    const qs = new URLSearchParams();
    for (const [k, v] of Object.entries(query)) {
      if (v === undefined || v === null || v === '') continue;
      qs.set(k, String(v));
    }
    const s = qs.toString();
    if (s) url += (url.includes('?') ? '&' : '?') + s;
  }
  const init = { method, headers: { ...(headers || {}) }, signal };
  if (body !== undefined) {
    if (body instanceof FormData || body instanceof Blob || typeof body === 'string') {
      init.body = body;
    } else {
      init.headers['Content-Type'] = 'application/json';
      init.body = JSON.stringify(body);
    }
  }
  let res;
  try {
    res = await fetch(apiUrl(url), init);
  } catch (e) {
    if (e?.name === 'AbortError') throw e;
    throw new ApiError(-1, `could not reach the backend (${e.message || 'network error'})`);
  }
  if (raw) return res;
  const data = await parse(res);
  if (!res.ok) {
    const msg = (data && typeof data === 'object' && (data.detail || data.error || data.message)) || `HTTP ${res.status}`;
    throw new ApiError(res.status, msg, data);
  }
  return data;
}

export const api = {
  get: (path, opts) => request('GET', path, opts),
  post: (path, body, opts) => request('POST', path, { ...opts, body }),
  put: (path, body, opts) => request('PUT', path, { ...opts, body }),
  patch: (path, body, opts) => request('PATCH', path, { ...opts, body }),
  del: (path, opts) => request('DELETE', path, opts),
  /** Raw Response, for blobs / streams. */
  raw: (path, opts) => request(opts?.method || 'GET', path, { ...opts, raw: true }),
};

/** Human message for any thrown error. */
export function errorMessage(e) {
  if (!e) return 'unknown error';
  if (e instanceof ApiError) return e.status > 0 ? `HTTP ${e.status}: ${e.detail}` : e.detail;
  return e.message || String(e);
}

/** True when the server says this account may not use an operator surface. */
export function isForbidden(e) { return e instanceof ApiError && e.status === 403; }

export default api;
