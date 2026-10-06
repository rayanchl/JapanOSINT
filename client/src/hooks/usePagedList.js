import { useCallback, useEffect, useRef, useState } from 'react';
import { api } from '../api/client.js';

/**
 * Offset-paged server lists: {data:[…], page:{limit, offset, count, total, has_more}}.
 *
 * The evidence, saved-search, search-history and maintenance routes all answer
 * in that shape now (native/core/{evidence,savedsearchapi,maintenanceapi}.c).
 * They used to return one fixed slice with only its own size, so these screens
 * could say no more than "N loaded · more on the server". With a measured
 * `total` and `has_more` they page through instead, and say "showing N of M".
 */

/** `path` plus limit/offset, keeping the query string `path` already carries. */
export function pageUrl(path, limit, offset) {
  const sep = path.includes('?') ? '&' : '?';
  return `${path}${sep}limit=${limit}&offset=${offset}`;
}

/**
 * Append a page to the rows already held, dropping any row already present.
 * Offset paging shifts by one when a row is inserted or deleted between two
 * requests; a duplicate key is that shift, not a second record.
 */
export function appendPage(rows, next, key = 'id') {
  const keyOf = typeof key === 'function' ? key : (r) => r?.[key];
  const seen = new Set(rows.map(keyOf));
  const out = rows.slice();
  for (const r of next || []) {
    const k = keyOf(r);
    if (k != null && seen.has(k)) continue;
    seen.add(k);
    out.push(r);
  }
  return out;
}

/** Where the next page starts: what the server says it served, not rows.length. */
export function nextOffset(page, fallback = 0) {
  if (page && Number.isFinite(page.offset) && Number.isFinite(page.count)) return page.offset + page.count;
  return fallback;
}

/** {total, hasMore} out of a page block. total stays null when the server sent null. */
export function pageBound(page) {
  const total = page && Number.isFinite(page.total) ? page.total : null;
  return { total, hasMore: Boolean(page?.has_more) };
}

/**
 * Load an offset-paged list and page through it.
 *
 *   const { rows, total, hasMore, loadMore, loadingMore, reload } = usePagedList('/api/saved-searches?kind=intel');
 *
 * `rows` is everything loaded so far. A silent `reload()` (after a mutation)
 * re-reads as many rows as are on screen, up to the server's 200 per request,
 * so pinning one entry does not collapse a list the user has paged through.
 */
export function usePagedList(path, { pageSize = 50, deps = [], key = 'id', enabled = true, maxPage = 200 } = {}) {
  const [rows, setRows] = useState([]);
  const [page, setPage] = useState(null);
  const [meta, setMeta] = useState(null);
  const [error, setError] = useState(null);
  const [moreError, setMoreError] = useState(null);
  const [loading, setLoading] = useState(Boolean(path) && enabled);
  const [loadingMore, setLoadingMore] = useState(false);
  const [loaded, setLoaded] = useState(false);
  const seq = useRef(0);
  const shown = useRef(0);
  shown.current = rows.length;

  const reload = useCallback(async ({ silent = false } = {}) => {
    if (!path || !enabled) { setLoading(false); return; }
    const my = ++seq.current;
    if (!silent) setLoading(true);
    const size = silent ? Math.min(maxPage, Math.max(pageSize, shown.current)) : pageSize;
    try {
      const j = await api.get(pageUrl(path, size, 0));
      if (my !== seq.current) return;
      setRows(Array.isArray(j?.data) ? j.data : []);
      setPage(j?.page ?? null);
      setMeta(j?.meta ?? null);
      setError(null);
      setMoreError(null);
      setLoaded(true);
    } catch (e) {
      if (my !== seq.current || e?.name === 'AbortError') return;
      setError(e);
    } finally {
      if (my === seq.current) setLoading(false);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [path, enabled, pageSize, maxPage, ...deps]);

  useEffect(() => { reload(); }, [reload]);

  const loadMore = useCallback(async () => {
    if (!path || !page?.has_more) return;
    const my = seq.current;
    setLoadingMore(true);
    try {
      const j = await api.get(pageUrl(path, pageSize, nextOffset(page, shown.current)));
      if (my !== seq.current) return;              // a reload replaced the list
      setRows((prev) => appendPage(prev, Array.isArray(j?.data) ? j.data : [], key));
      setPage(j?.page ?? null);
      setMoreError(null);
    } catch (e) {
      if (my === seq.current && e?.name !== 'AbortError') setMoreError(e);
    } finally {
      if (my === seq.current) setLoadingMore(false);
    }
  }, [path, page, pageSize, key]);

  const { total, hasMore } = pageBound(page);
  return { rows, page, meta, total, hasMore, error, moreError, loading, loaded, loadingMore, loadMore, reload, setRows };
}

/**
 * Page through a list whose FIRST page arrived inside a bigger response (the
 * maintenance digest and the per-source pipeline view embed each list's first
 * page plus `pages.<name>`). `urlFor(offset)` builds the URL of a later page.
 * Resets whenever `initialRows` changes identity (the parent reloaded).
 */
export function useMoreRows(initialRows, initialPage, urlFor, { key = 'id' } = {}) {
  const [rows, setRows] = useState(initialRows || []);
  const [page, setPage] = useState(initialPage || null);
  const [loadingMore, setLoadingMore] = useState(false);
  const [moreError, setMoreError] = useState(null);
  const gen = useRef(0);

  useEffect(() => {
    gen.current += 1;
    setRows(initialRows || []);
    setPage(initialPage || null);
    setMoreError(null);
    setLoadingMore(false);
  }, [initialRows, initialPage]);

  const loadMore = useCallback(async () => {
    if (!page?.has_more || !urlFor) return;
    const my = gen.current;
    setLoadingMore(true);
    try {
      const j = await api.get(urlFor(nextOffset(page, rows.length)));
      if (my !== gen.current) return;
      setRows((prev) => appendPage(prev, Array.isArray(j?.data) ? j.data : [], key));
      setPage(j?.page ?? null);
      setMoreError(null);
    } catch (e) {
      if (my === gen.current && e?.name !== 'AbortError') setMoreError(e);
    } finally {
      if (my === gen.current) setLoadingMore(false);
    }
  }, [page, rows.length, urlFor, key]);

  const { total, hasMore } = pageBound(page);
  return { rows, page, total, hasMore, loadingMore, moreError, loadMore };
}

export default usePagedList;
