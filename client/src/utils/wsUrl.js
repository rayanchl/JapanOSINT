/**
 * Is the /ws push channel turned on for this build?
 *
 * OFF by default, because nothing answers it. The native server
 * (native/core/httpd.c) has no WebSocket endpoint at all — /ws was the deleted
 * Node server's — and vite.config.js proxies only /api, so every hook that
 * opened '/ws' spent its life in a reconnect loop against a socket that cannot
 * exist, and the panels said "reconnecting…" forever. With the flag off,
 * useWebSocket never dials and each consumer uses the polling / HTTP path it
 * already has (see useCameraDiscoveryStream, useDataSources). Set
 * VITE_JO_WEBSOCKET=1 only against a backend that really serves /ws.
 */
export const WS_ENABLED = import.meta.env?.VITE_JO_WEBSOCKET === '1';

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
