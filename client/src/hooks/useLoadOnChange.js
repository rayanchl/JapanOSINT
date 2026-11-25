import { useEffect } from 'react';

/**
 * Run a component's own async loader when its identity changes (mount, or a
 * dependency of the loader changed), without setting state in the effect body.
 *
 * The loader starts from a microtask, so every setState it performs (including
 * a leading `setLoading(true)`) happens from an async callback, and a run that
 * is superseded or unmounted before the microtask fires never starts (which
 * also keeps a StrictMode double-mount to one request). In-flight staleness is
 * still the loader's own business (its `seq` guard).
 *
 *   const load = useCallback(async () => { setLoading(true); ... }, [filter]);
 *   useLoadOnChange(load);
 */
export default function useLoadOnChange(load) {
  useEffect(() => {
    let live = true;
    queueMicrotask(() => { if (live) load(); });
    return () => { live = false; };
  }, [load]);
}
