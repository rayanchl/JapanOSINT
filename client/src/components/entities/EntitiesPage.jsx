import React from 'react';
import { useNavigate, useSearchParams } from 'react-router-dom';
import { useEntitySearch } from '../../hooks/useSearch.js';
import { entityVisual } from '../../utils/entityVisuals.js';

/** Entities landing — FTS search across the unified entity graph (collector
 *  NER + OSINT-search discoveries). Kanji-substring works (kuromoji FTS).
 *  `/entities?q=…` pre-fills the box (saved-search runs and deep links). */
export default function EntitiesPage() {
  const navigate = useNavigate();
  const [params] = useSearchParams();
  const [q, setQ] = React.useState(() => params.get('q') || '');
  React.useEffect(() => { const pq = params.get('q'); if (pq != null && pq !== '') setQ(pq); }, [params]);
  const { results, loading, error } = useEntitySearch(q);

  return (
    <div className="h-full overflow-auto p-4">
      <div className="max-w-3xl mx-auto space-y-4">
        <div>
          <h1 className="text-lg font-semibold text-osint-text">Entities</h1>
          <p className="text-xs text-osint-muted">
            Every person, org, domain, IP, vessel and location linked across all sources.
          </p>
        </div>

        <input
          value={q}
          onChange={(e) => setQ(e.target.value)}
          placeholder="search entities (kanji or romaji)…"
          className="w-full px-3 py-2 rounded bg-osint-bg border border-osint-border text-sm text-osint-text focus:border-neon-cyan/50 outline-none"
        />

        {loading && <div className="text-xs text-osint-muted">searching…</div>}

        {/* A search that could not be run says so. Rendering it as an empty
          * result list would be a claim about the corpus we never obtained. */}
        {!loading && error && (
          <div className="rounded border border-red-500/40 bg-red-500/10 px-3 py-2 text-xs text-red-300">
            Entity search failed ({error}). This is not a statement that nothing matched.
          </div>
        )}

        <ul className="space-y-1">
          {results.map((e) => {
            const v = entityVisual(e.type);
            return (
              <li key={e.entity_id}>
                <button
                  type="button"
                  onClick={() => navigate(`/entities/${encodeURIComponent(String(e.type).toLowerCase())}/${encodeURIComponent(e.entity_id)}`)}
                  className="w-full flex items-center gap-2 text-left rounded border border-osint-border bg-osint-surface px-3 py-2 hover:border-neon-cyan/40"
                >
                  <span className={`px-1.5 py-0.5 rounded border text-[11px] ${v.color}`}>{v.label}</span>
                  <span className="text-sm text-osint-text flex-1 truncate">{e.value}</span>
                  <span className="text-xs text-osint-muted">{e.mention_count} mentions</span>
                </button>
              </li>
            );
          })}
          {!loading && !error && q && results.length === 0 && (
            <li className="text-sm text-osint-muted py-6 text-center">No matching entities.</li>
          )}
        </ul>
      </div>
    </div>
  );
}
