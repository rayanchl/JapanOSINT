import { useCallback, useState } from 'react';
import { api } from '../api/client.js';

/**
 * Stateless view-state permalinks (roadmap 38, core/savedsearchapi.c).
 *   POST /api/permalink  { state: { kind, params, map:{lat,lon,zoom,bearing,pitch}, layers:[…] } }
 *        → { data: { token, version:'v1', state } }
 *   GET  /api/permalink/:token → { data: <state> }
 * The share URL is this page with `?p=<token>`; MapPage resolves it on load.
 */
export function buildShareUrl(token) {
  const u = new URL(window.location.href);
  u.search = '';
  u.searchParams.set('p', token);
  u.hash = '';
  return u.toString();
}

export default function usePermalink() {
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState(null);

  const mint = useCallback(async (state) => {
    setBusy(true); setError(null);
    try {
      const j = await api.post('/api/permalink', { state });
      const token = j?.data?.token;
      if (!token) throw new Error('server returned no token');
      return { token, url: buildShareUrl(token), state: j.data.state };
    } catch (e) { setError(e); throw e; }
    finally { setBusy(false); }
  }, []);

  const resolve = useCallback(async (token) => {
    setBusy(true); setError(null);
    try {
      const j = await api.get(`/api/permalink/${encodeURIComponent(token)}`);
      return j?.data ?? null;
    } catch (e) { setError(e); throw e; }
    finally { setBusy(false); }
  }, []);

  return { mint, resolve, busy, error };
}
