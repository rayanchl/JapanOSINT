import React from 'react';
import { useSearchStore } from '../../hooks/useSearch.js';
import SearchCard from './SearchCard.jsx';

/**
 * Search tab — the ported OSINTsaas interactive entity-pivot search. Query
 * box + 9 AI suggestions, live active runs, completed history. "Pivot" on any
 * entity starts a fresh investigation (the multi-round flow).
 */
export default function SearchPage() {
  const { active, completed, startSearch, fetchSuggestions } = useSearchStore();
  const [query, setQuery] = React.useState('');
  const [suggestions, setSuggestions] = React.useState([]);
  const [busy, setBusy] = React.useState(false);
  const [err, setErr] = React.useState(null);

  React.useEffect(() => {
    const q = query.trim();
    if (q.length < 3) { setSuggestions([]); return undefined; }
    let alive = true;
    const t = setTimeout(async () => {
      const s = await fetchSuggestions(q);
      if (alive) setSuggestions(s);
    }, 350);
    return () => { alive = false; clearTimeout(t); };
  }, [query, fetchSuggestions]);

  const run = async (q) => {
    const qq = (q ?? query).trim();
    if (!qq || busy) return;
    setBusy(true); setErr(null); setSuggestions([]);
    try { await startSearch(qq); setQuery(''); }
    catch (e) { setErr(e.message || 'search failed'); }
    finally { setBusy(false); }
  };

  return (
    <div className="h-full overflow-auto p-4">
      <div className="max-w-3xl mx-auto space-y-4">
        <div>
          <h1 className="text-lg font-semibold text-gray-100">OSINT Search</h1>
          <p className="text-xs text-gray-500">
            Pivot on any entity — person, org, domain, IP, vessel, location — across
            external sources and the JapanOSINT corpus.
          </p>
        </div>

        <form onSubmit={(e) => { e.preventDefault(); run(); }} className="flex gap-2">
          <input
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            placeholder="email, IP, domain, name, company, vessel IMO…"
            className="flex-1 px-3 py-2 rounded bg-osint-bg border border-osint-border text-sm text-gray-100 focus:border-neon-cyan/50 outline-none"
          />
          <button
            type="submit"
            disabled={busy || !query.trim()}
            className="px-4 py-2 rounded text-sm font-medium bg-neon-cyan/15 text-neon-cyan border border-neon-cyan/30 hover:bg-neon-cyan/25 disabled:opacity-40"
          >
            {busy ? '…' : 'Investigate'}
          </button>
        </form>
        {err && <div className="text-xs text-status-offline">{err}</div>}

        {suggestions.length > 0 && (
          <div className="flex flex-wrap gap-1.5">
            {suggestions.map((s, i) => (
              <button
                key={i}
                type="button"
                onClick={() => run(s)}
                className="text-xs px-2 py-1 rounded border border-osint-border text-gray-400 hover:text-neon-cyan hover:border-neon-cyan/40"
              >
                {s}
              </button>
            ))}
          </div>
        )}

        {active.length > 0 && (
          <section className="space-y-2">
            <h2 className="text-xs uppercase tracking-wide text-gray-500">Active</h2>
            {active.map((a) => (
              <SearchCard key={a.request_id} snapshot={a.snapshot} query={a.query} onPivot={run} />
            ))}
          </section>
        )}

        {completed.length > 0 && (
          <section className="space-y-2">
            <h2 className="text-xs uppercase tracking-wide text-gray-500">Completed</h2>
            {completed.map((c) => (
              <SearchCard key={c.request_id} snapshot={c.snapshot} query={c.query} onPivot={run} />
            ))}
          </section>
        )}

        {active.length === 0 && completed.length === 0 && (
          <div className="text-sm text-gray-600 py-12 text-center">
            No investigations yet. Enter an entity above to begin.
          </div>
        )}
      </div>
    </div>
  );
}
