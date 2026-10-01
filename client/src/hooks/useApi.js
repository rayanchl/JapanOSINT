import { useCallback, useEffect, useRef, useState } from 'react';
import { api } from '../api/client.js';

/**
 * Load a JSON resource with the three states a page must keep apart:
 * `loading`, `error` (the request failed — NOT an empty result) and `data`.
 *
 *   const { data, error, loading, reload } = useApi('/api/alerts');
 *
 * `path` null/undefined skips the request. `deps` re-fetches when changed.
 * `transform` maps the raw body before it lands in `data`.
 */
export function useApi(path, { deps = [], transform, enabled = true, poll = 0 } = {}) {
  const [data, setData] = useState(null);
  const [error, setError] = useState(null);
  const [loading, setLoading] = useState(Boolean(path) && enabled);
  const [loadedAt, setLoadedAt] = useState(null);
  const seq = useRef(0);
  const transformRef = useRef(transform);
  transformRef.current = transform;

  const load = useCallback(async ({ silent = false } = {}) => {
    if (!path || !enabled) { setLoading(false); return; }
    const my = ++seq.current;
    if (!silent) setLoading(true);
    try {
      const j = await api.get(path);
      if (my !== seq.current) return;
      setData(transformRef.current ? transformRef.current(j) : j);
      setError(null);
      setLoadedAt(Date.now());
    } catch (e) {
      if (my !== seq.current || e?.name === 'AbortError') return;
      setError(e);
    } finally {
      if (my === seq.current) setLoading(false);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [path, enabled, ...deps]);

  useEffect(() => { load(); }, [load]);

  useEffect(() => {
    if (!poll || !path || !enabled) return undefined;
    const t = setInterval(() => load({ silent: true }), poll);
    return () => clearInterval(t);
  }, [poll, path, enabled, load]);

  return { data, error, loading, loadedAt, reload: load, setData };
}

/** Imperative mutation wrapper: `const { run, busy, error } = useMutation(fn)`. */
export function useMutation(fn) {
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState(null);
  const run = useCallback(async (...args) => {
    setBusy(true); setError(null);
    try { return await fn(...args); }
    catch (e) { setError(e); throw e; }
    finally { setBusy(false); }
  }, [fn]);
  return { run, busy, error, clearError: () => setError(null) };
}

export default useApi;
