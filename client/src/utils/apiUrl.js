/**
 * Resolve the URL for an `/api/...` path.
 *
 * Default behaviour: return the path unchanged so the request goes to the
 * same origin as the SPA. In dev the Vite server proxies `/api/*` to the
 * backend (see `vite.config.*`), and in prod the API is served from the same
 * host as the static files, so a bare path is correct in both cases.
 *
 * `VITE_API_HOST` overrides the host — set it when the static client is
 * served from a different origin than the API (e.g. CDN-hosted SPA pointing
 * at a separate api.example.com backend).
 *
 * Parallels `wsUrl.js`. Callers pass only the path (with leading `/api/...`).
 */
export default function apiUrl(path = '/') {
  if (typeof window === 'undefined') return path;
  const override = import.meta.env?.VITE_API_HOST;
  if (override) {
    const proto = window.location.protocol;
    return `${proto}//${override}${path}`;
  }
  return path;
}
