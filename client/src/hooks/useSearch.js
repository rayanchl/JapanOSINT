import { useState, useEffect, useCallback } from 'react';
import { subscribe, startSearch, fetchSuggestions } from '../store/searchStore.js';
import apiUrl from '../utils/apiUrl.js';

/** Subscribe to the live search store (active + completed runs). */
export function useSearchStore() {
  const [s, setS] = useState({ active: [], completed: [] });
  useEffect(() => subscribe(setS), []);
  return { ...s, startSearch, fetchSuggestions };
}

/** Entity FTS autocomplete / search. */
export function useEntitySearch(q, type) {
  const [results, setResults] = useState([]);
  const [loading, setLoading] = useState(false);
  useEffect(() => {
    const query = (q || '').trim();
    if (!query) { setResults([]); return undefined; }
    let alive = true;
    setLoading(true);
    const t = setTimeout(async () => {
      try {
        const url = `/api/entities/search?q=${encodeURIComponent(query)}${type ? `&type=${type}` : ''}`;
        const res = await fetch(apiUrl(url));
        const j = res.ok ? await res.json() : { results: [] };
        if (alive) setResults(j.results || []);
      } catch { if (alive) setResults([]); }
      finally { if (alive) setLoading(false); }
    }, 250);
    return () => { alive = false; clearTimeout(t); };
  }, [q, type]);
  return { results, loading };
}

/** Entity profile + graph + mentions for /entities/:type/:id. */
export function useEntity(type, id) {
  const [profile, setProfile] = useState(null);
  const [graph, setGraph] = useState({ nodes: [], edges: [] });
  const [mentions, setMentions] = useState([]);
  const [depth, setDepth] = useState(1);
  const [loading, setLoading] = useState(true);

  const load = useCallback(async () => {
    setLoading(true);
    try {
      const [p, g, m] = await Promise.all([
        fetch(apiUrl(`/api/entities/${type}/${id}`)),
        fetch(apiUrl(`/api/entities/${type}/${id}/graph?depth=${depth}`)),
        fetch(apiUrl(`/api/entities/${type}/${id}/mentions?limit=50`)),
      ]);
      setProfile(p.ok ? await p.json() : null);
      setGraph(g.ok ? await g.json() : { nodes: [], edges: [] });
      const mj = m.ok ? await m.json() : { mentions: [] };
      setMentions(mj.mentions || []);
    } finally { setLoading(false); }
  }, [type, id, depth]);

  useEffect(() => { load(); }, [load]);
  return { profile, graph, mentions, depth, setDepth, loading, reload: load };
}
