import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { api } from '../api/client.js';

/**
 * Cursor-paged reader for the intel feed envelope
 *   { data: [...], page: { next_cursor, limit, total | total_gte }, meta }
 * served by /api/intel/items and /api/intel/search (core/intelapi.c).
 *
 * Every accepted filter is a query key: source, q, qAlt, lang, since, until,
 * record_type, sub_source_id, has_geom, tag, limit, sort, total, collapse,
 * lang_view. A failed request lands in `error` and is never an empty page.
 */
export function useIntelItems(path, params, { enabled = true } = {}) {
  const [items, setItems] = useState([]);
  const [page, setPage] = useState(null);
  const [meta, setMeta] = useState(null);
  const [error, setError] = useState(null);
  const [loading, setLoading] = useState(Boolean(enabled));
  const [loadingMore, setLoadingMore] = useState(false);
  const seq = useRef(0);
  const key = JSON.stringify(params || {});

  const fetchPage = useCallback(async (cursor) => {
    const q = { ...(params || {}) };
    if (cursor) q.cursor = cursor;
    return api.get(path, { query: q });
  }, [path, key]); // eslint-disable-line react-hooks/exhaustive-deps

  const load = useCallback(async () => {
    if (!enabled) { setLoading(false); return; }
    const my = ++seq.current;
    setLoading(true);
    try {
      const j = await fetchPage(null);
      if (my !== seq.current) return;
      setItems(Array.isArray(j?.data) ? j.data : []);
      setPage(j?.page || null);
      setMeta(j?.meta || null);
      setError(null);
    } catch (e) {
      if (my !== seq.current) return;
      setError(e);
    } finally { if (my === seq.current) setLoading(false); }
  }, [fetchPage, enabled]);

  useEffect(() => { load(); }, [load]);

  const loadMore = useCallback(async () => {
    const cur = page?.next_cursor;
    if (!cur || loadingMore) return;
    const my = seq.current;
    setLoadingMore(true);
    try {
      const j = await fetchPage(cur);
      if (my !== seq.current) return;
      setItems((xs) => [...xs, ...(Array.isArray(j?.data) ? j.data : [])]);
      setPage(j?.page || null);
      setMeta(j?.meta || null);
      setError(null);
    } catch (e) {
      if (my === seq.current) setError(e);
    } finally { setLoadingMore(false); }
  }, [page, loadingMore, fetchPage]);

  const total = page?.total ?? null;
  const totalGte = page?.total_gte ?? null;
  return { items, page, meta, error, loading, loadingMore, reload: load, loadMore,
    hasMore: Boolean(page?.next_cursor), total, totalGte };
}

/** Human "since" presets → ISO timestamps. */
export const SINCE_PRESETS = [
  { value: '', label: 'Any time' },
  { value: '1h', label: '1 h' },
  { value: '24h', label: '24 h' },
  { value: '7d', label: '7 d' },
  { value: '30d', label: '30 d' },
];
export function sinceIso(preset, now = Date.now()) {
  const m = /^(\d+)([hd])$/.exec(preset || '');
  if (!m) return undefined;
  const ms = Number(m[1]) * (m[2] === 'h' ? 3600e3 : 86400e3);
  // Floored to the whole minute, so two calls a few ms apart agree. This is a
  // second line of defence for a caller that computes it during render (see
  // useSinceIso, the first): a per-millisecond value in a request key re-keys
  // the request on every render.
  return new Date(Math.floor((now - ms) / 60000) * 60000).toISOString();
}

/**
 * `sinceIso(preset)` pinned for as long as the preset is unchanged. Calling
 * sinceIso() during render gave a new millisecond timestamp every render; it
 * is part of useIntelItems' params key, so every render refetched and every
 * response re-rendered — an unbounded request loop (84 COUNT queries per
 * 500 ms against the server). The window is anchored when the user picks it.
 */
export function useSinceIso(preset) {
  return useMemo(() => sinceIso(preset), [preset]);
}
