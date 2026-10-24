/**
 * Fetch an image from the API and hand back an object URL for <img src>.
 *
 * WHY NOT <img src="/api/…">: an image request is made by the browser, not by
 * window.fetch, so the interceptor in auth/session.js never sees it — no
 * `Authorization: Bearer`, no `X-Tenant-Id`. Against a server that requires a
 * session, every such <img> is a 401 that renders as a broken image with no
 * reason anywhere. Going through fetch() picks up the session (and its one
 * refresh-and-retry on 401) like every other API call.
 *
 * Resolves to a blob: URL the CALLER must URL.revokeObjectURL() when done.
 * Rejects with an Error whose message says why: the server's own `error`
 * field when it sent one (the camera proxy answers 404 "camera not found",
 * 502 for an upstream that is not an image), else the HTTP status; and a 2xx
 * that is not image/* is refused rather than drawn as a broken image.
 */
export default async function fetchImageObjectUrl(url, init) {
  const res = await fetch(url, init);
  if (!res.ok) {
    let why = '';
    try {
      const body = await res.json();
      why = body?.error || body?.detail || '';
    } catch { /* not JSON: the status is the message */ }
    throw new Error(why ? `HTTP ${res.status}: ${why}` : `HTTP ${res.status}`);
  }
  const ct = (res.headers.get('content-type') || '').toLowerCase();
  if (!ct.startsWith('image/')) {
    throw new Error(`not an image (${ct || 'no content-type'})`);
  }
  const blob = await res.blob();
  return URL.createObjectURL(blob);
}
