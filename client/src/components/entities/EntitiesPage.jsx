import React from 'react';
import { useNavigate } from 'react-router-dom';
import { useEntitySearch } from '../../hooks/useSearch.js';
import { entityVisual } from '../../utils/entityVisuals.js';

/** Entities landing — FTS search across the unified entity graph (collector
 *  NER + OSINT-search discoveries). Kanji-substring works (kuromoji FTS). */
export default function EntitiesPage() {
  const navigate = useNavigate();
  const [q, setQ] = React.useState('');
  const { results, loading } = useEntitySearch(q);

  return (
    <div className="h-full overflow-auto p-4">
      <div className="max-w-3xl mx-auto space-y-4">
        <div>
          <h1 className="text-lg font-semibold text-gray-100">Entities</h1>
          <p className="text-xs text-gray-500">
            Every person, org, domain, IP, vessel and location linked across all sources.
          </p>
        </div>

        <input
          value={q}
          onChange={(e) => setQ(e.target.value)}
          placeholder="search entities (kanji or romaji)…"
          className="w-full px-3 py-2 rounded bg-osint-bg border border-osint-border text-sm text-gray-100 focus:border-neon-cyan/50 outline-none"
        />

        {loading && <div className="text-xs text-gray-500">searching…</div>}

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
                  <span className="text-sm text-gray-200 flex-1 truncate">{e.value}</span>
                  <span className="text-xs text-gray-500">{e.mention_count} mentions</span>
                </button>
              </li>
            );
          })}
          {!loading && q && results.length === 0 && (
            <li className="text-sm text-gray-600 py-6 text-center">No matching entities.</li>
          )}
        </ul>
      </div>
    </div>
  );
}
