import { useEffect, useRef } from 'react';
import { api } from './client.js';

/**
 * Record a search the user COMMITTED in the workspace's search history
 * (POST /api/search-history, savedsearchapi.c). Every ad-hoc search is
 * recorded since 2026-10-09 — not only saved-search runs.
 *
 * "Committed" is the caller's call, and it must never be a keystroke: the
 * entity box searches as you type, so it records on Enter or on opening a
 * result, never on each change. Result screens record once their first page
 * has loaded, so the entry carries the real result count. The server folds
 * the same search by the same person inside 10 minutes into one row (a reload
 * or a refetch is not a new search), so a second call is harmless.
 *
 * Fire-and-forget: a history write never breaks or delays the search it
 * describes, and a failure is swallowed. Empty params are not recorded.
 */
export function recordSearch(kind, params, resultCount) {
  const clean = {};
  for (const [k, v] of Object.entries(params || {})) {
    if (v != null && v !== '') clean[k] = v;
  }
  if (!Object.keys(clean).length) return Promise.resolve(null);
  const body = { kind, params: clean };
  if (Number.isFinite(resultCount) && resultCount >= 0) body.result_count = Math.floor(resultCount);
  return api.post('/api/search-history', body).catch(() => null);
}

/**
 * Record once per distinct (kind, params) when `ready` turns true — for a
 * results component whose inputs come from a committed URL. `resultCount` is
 * read at that moment and deliberately not a dependency: a count settling
 * later is not a new search.
 */
export function useRecordSearch(kind, params, { ready, resultCount, enabled = true } = {}) {
  const last = useRef(null);
  const key = JSON.stringify([kind, params]);
  useEffect(() => {
    if (!enabled || !ready || last.current === key) return;
    last.current = key;
    recordSearch(kind, params, resultCount);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [enabled, ready, key]);
}

/** The intel page opens on its Sources tab; a history entry for a search must
 *  land on the Search tab. Older entries (saved-search runs) carry only `q`. */
export function intelHistoryParams(params) {
  const p = { ...(params || {}) };
  if (!p.view && (p.q || p.near)) p.view = 'search';
  return p;
}

/** Where an intel history entry re-runs: a search inside one source reopens
 *  that source's list (`view: 'source'`), everything else the Search tab with
 *  all of its params. */
export function intelHistoryRoute(params) {
  const p = intelHistoryParams(params);
  if (p.view === 'source' && p.source) {
    const qs = new URLSearchParams();
    if (p.q) qs.set('q', String(p.q));
    return `/intel/sources/${encodeURIComponent(p.source)}${qs.toString() ? `?${qs}` : ''}`;
  }
  const qs = new URLSearchParams();
  for (const [k, v] of Object.entries(p)) {
    if (v != null && v !== '') qs.set(k, typeof v === 'object' ? JSON.stringify(v) : String(v));
  }
  return `/intel${qs.toString() ? `?${qs}` : ''}`;
}
