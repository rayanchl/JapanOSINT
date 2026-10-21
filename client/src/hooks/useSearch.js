import { useState, useEffect, useCallback, useRef } from 'react';
import { subscribe, startSearch, fetchSuggestions, fetchSuggestionSet, attachRun, getRun } from '../store/searchStore.js';
import apiUrl from '../utils/apiUrl.js';

/** Subscribe to the live search store (active + completed runs). */
export function useSearchStore() {
  const [s, setS] = useState({ active: [], completed: [], lastError: null });
  useEffect(() => subscribe(setS), []);
  return { ...s, startSearch, fetchSuggestions, attachRun };
}

/** Settled typing before the ordinary (non-expanded) suggest request. The
 *  iOS tab waits 3s before asking the LLM; the web used 350ms, which spammed
 *  the suggest model on every keystroke. */
export const SUGGEST_DEBOUNCE_MS = 1500;
/** A typing pause this long, on a query of SUGGEST_MIN_CHARS or more, asks
 *  once with `expand=1` (the LLM's extra ideas). Well above the debounce, so
 *  the ordinary answer has landed and been read before the model is asked. */
export const SUGGEST_EXPAND_PAUSE_MS = 4000;
export const SUGGEST_MIN_CHARS = 3;

/**
 * Search-box suggestions: the debounced fast path, plus `expand=1` (the LLM's
 * ideas, core/suggest.h) on an explicit action — `expand()` — or once after a
 * SUGGEST_EXPAND_PAUSE_MS pause.
 *
 * At most ONE expand request per distinct query, ever (`asked`); a query
 * change aborts the one in flight, and a late answer for an older query is
 * dropped. Expansion is offered only when the server answered by retrieval
 * (`meta.retrieval`): without it the ordinary answer already IS the LLM's and
 * the server ignores `expand`, so asking again would only repeat it.
 *
 * Nothing here records search history — these are not committed searches.
 */
export function useSearchSuggestions(query) {
  const q = (query || '').trim();
  // { q, suggestions, items, meta, expanded, failed } — the answer on show.
  const [set, setSet] = useState(null);
  const [expanding, setExpanding] = useState(null);
  const [pausedOn, setPausedOn] = useState(null);
  const asked = useRef(new Set());
  const inflight = useRef(null);
  const current = useRef(q);
  current.current = q;

  // The fast path: one non-expanded request per settled query.
  useEffect(() => {
    if (q.length < SUGGEST_MIN_CHARS) { setSet(null); return undefined; }
    let alive = true;
    const t = setTimeout(async () => {
      const s = await fetchSuggestionSet(q);
      if (alive) setSet({ q, suggestions: [], items: [], meta: null, ...s, expanded: false, failed: false });
    }, SUGGEST_DEBOUNCE_MS);
    return () => { alive = false; clearTimeout(t); };
  }, [q]);

  // A changed query abandons the expand in flight for the old one.
  useEffect(() => () => { inflight.current?.abort(); inflight.current = null; }, [q]);

  const expand = useCallback(async (raw) => {
    const eq = (raw ?? current.current).trim();
    if (!eq || asked.current.has(eq)) return;
    asked.current.add(eq);
    inflight.current?.abort();
    const ctl = new AbortController();
    inflight.current = ctl;
    setExpanding(eq);
    try {
      const s = await fetchSuggestionSet(eq, { expand: true, signal: ctl.signal });
      if (ctl.signal.aborted || current.current !== eq) return;   // stale: dropped
      // A failed expand keeps the ordinary answer on show and says so.
      setSet((prev) => (s
        ? { q: eq, ...s, expanded: true, failed: false }
        : { ...(prev && prev.q === eq ? prev : { q: eq, suggestions: [], items: [], meta: null }), expanded: true, failed: true }));
    } finally {
      if (inflight.current === ctl) { inflight.current = null; setExpanding(null); }
    }
  }, []);

  // The long pause, noted per query and reset by every change.
  useEffect(() => {
    setPausedOn(null);
    if (q.length < SUGGEST_MIN_CHARS) return undefined;
    const t = setTimeout(() => setPausedOn(q), SUGGEST_EXPAND_PAUSE_MS);
    return () => clearTimeout(t);
  }, [q]);

  const canExpand = !!set && set.q === q && !!set.meta?.retrieval && !set.expanded
    && expanding !== q && !asked.current.has(q);

  // Paused on this query and its ordinary answer is in (either order): ask once.
  useEffect(() => {
    if (pausedOn && pausedOn === q && canExpand) expand(q);
  }, [pausedOn, q, canExpand, expand]);

  const clear = useCallback(() => setSet(null), []);
  return { set, expanding: expanding === q, canExpand, expand, clear };
}

/** One run by request id, live-updated; re-attaches through the results
 *  endpoint when the tab does not already hold it. */
export function useRun(requestId) {
  const [run, setRun] = useState(() => getRun(requestId));
  const [error, setError] = useState(null);
  useEffect(() => {
    setError(null);
    if (!requestId) return undefined;
    if (!getRun(requestId)) attachRun(requestId).catch((e) => setError(e));
    return subscribe(() => setRun(getRun(requestId)));
  }, [requestId]);
  return { run, error };
}

/** Entity FTS autocomplete / search.
 *
 * A failed request is NOT an empty result. Both used to collapse onto
 * `setResults([])`, and the screen reading this hook then printed
 * "No matching entities." — an assertion about the corpus made out of a 500.
 * `error` keeps the two apart all the way to the render. */
export function useEntitySearch(q, type) {
  const [results, setResults] = useState([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);
  // The query the current `results` answer. `loading` is false on the render
  // before the debounce starts, so "not loading" alone does not mean the
  // results belong to the box's text — a caller recording a search needs this.
  const [resolved, setResolved] = useState(null);
  useEffect(() => {
    const query = (q || '').trim();
    if (!query) { setResults([]); setError(null); setResolved(null); return undefined; }
    let alive = true;
    setLoading(true);
    const t = setTimeout(async () => {
      try {
        const url = `/api/entities/search?q=${encodeURIComponent(query)}${type ? `&type=${type}` : ''}`;
        const res = await fetch(apiUrl(url));
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const j = await res.json();
        if (alive) { setResults(j.results || []); setError(null); setResolved(query); }
      } catch (e) {
        if (alive) { setResults([]); setError(e.message || 'request failed'); setResolved(null); }
      }
      finally { if (alive) setLoading(false); }
    }, 250);
    return () => { alive = false; clearTimeout(t); };
  }, [q, type]);
  return { results, loading, error, resolved };
}

/** How many mentions the timeline asks for. EntityProfile prints this bound
 *  against profile.mention_count, so the two must not drift apart. */
export const MENTION_LIMIT = 50;

/** Entity profile + graph + mentions for /entities/:type/:id. */
export function useEntity(type, id) {
  const [profile, setProfile] = useState(null);
  const [graph, setGraph] = useState({ nodes: [], edges: [] });
  const [mentions, setMentions] = useState([]);
  const [depth, setDepth] = useState(1);
  const [loading, setLoading] = useState(true);
  // One slot per request. A 500 on the graph must not reach the screen as
  // "No relationships yet.", nor a 500 on mentions as "No mentions." — those
  // are statements about the entity, and a failed request stated nothing.
  const [errors, setErrors] = useState({ profile: null, graph: null, mentions: null });

  const load = useCallback(async () => {
    setLoading(true);
    const next = { profile: null, graph: null, mentions: null };
    const take = async (settled, key, onOk, onFail) => {
      if (settled.status === 'rejected') {
        next[key] = settled.reason?.message || 'request failed';
        onFail();
        return;
      }
      const res = settled.value;
      if (!res.ok) { next[key] = `HTTP ${res.status}`; onFail(); return; }
      try { onOk(await res.json()); }
      catch (e) { next[key] = e.message || 'unreadable response'; onFail(); }
    };
    try {
      const [p, g, m] = await Promise.allSettled([
        fetch(apiUrl(`/api/entities/${type}/${id}`)),
        fetch(apiUrl(`/api/entities/${type}/${id}/graph?depth=${depth}`)),
        fetch(apiUrl(`/api/entities/${type}/${id}/mentions?limit=${MENTION_LIMIT}`)),
      ]);
      await take(p, 'profile', (j) => setProfile(j), () => setProfile(null));
      await take(g, 'graph', (j) => setGraph(j), () => setGraph({ nodes: [], edges: [] }));
      await take(m, 'mentions', (j) => setMentions(j.mentions || []), () => setMentions([]));
    } finally {
      setErrors(next);
      setLoading(false);
    }
  }, [type, id, depth]);

  useEffect(() => { load(); }, [load]);
  return { profile, graph, mentions, depth, setDepth, loading, errors, reload: load };
}
