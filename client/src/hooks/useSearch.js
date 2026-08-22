import { useState, useEffect, useCallback } from 'react';
import { subscribe, startSearch, fetchSuggestions } from '../store/searchStore.js';
import apiUrl from '../utils/apiUrl.js';

/** Subscribe to the live search store (active + completed runs). */
export function useSearchStore() {
  const [s, setS] = useState({ active: [], completed: [] });
  useEffect(() => subscribe(setS), []);
  return { ...s, startSearch, fetchSuggestions };
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
  useEffect(() => {
    const query = (q || '').trim();
    if (!query) { setResults([]); setError(null); return undefined; }
    let alive = true;
    setLoading(true);
    const t = setTimeout(async () => {
      try {
        const url = `/api/entities/search?q=${encodeURIComponent(query)}${type ? `&type=${type}` : ''}`;
        const res = await fetch(apiUrl(url));
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const j = await res.json();
        if (alive) { setResults(j.results || []); setError(null); }
      } catch (e) {
        if (alive) { setResults([]); setError(e.message || 'request failed'); }
      }
      finally { if (alive) setLoading(false); }
    }, 250);
    return () => { alive = false; clearTimeout(t); };
  }, [q, type]);
  return { results, loading, error };
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
