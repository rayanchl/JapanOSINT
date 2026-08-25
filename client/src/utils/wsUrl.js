/**
 * Resolve the /ws WebSocket URL for the current page.
 *
 * In dev (Vite at :3000) we hit the server directly at the host running the
 * backend; in prod the client is served from the same origin as the server,
 * so window.location.host is correct. VITE_WS_HOST overrides both — useful
 * when the backend lives on a different host than the static files.
 */
export default function wsUrl(path = '/ws') {
  if (typeof window === 'undefined') return null;
  const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  const override = import.meta.env?.VITE_WS_HOST;
  if (override) return `${proto}//${override}${path}`;
  // Dev: Vite serves the SPA on :3000 but the WS lives on the API server
  // (default :4000). Same hostname, different port.
  if (window.location.port === '3000') {
    return `${proto}//${window.location.hostname}:4000${path}`;
  }
  return `${proto}//${window.location.host}${path}`;
}
