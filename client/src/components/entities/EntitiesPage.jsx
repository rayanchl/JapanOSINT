import React from 'react';
import { useNavigate, useSearchParams } from 'react-router-dom';
import { useEntitySearch } from '../../hooks/useSearch.js';
import { recordSearch, useRecordSearch } from '../../api/searchHistory.js';
import { entityVisual } from '../../utils/entityVisuals.js';
import { downloadExport } from '../../hooks/useCases.js';
import { errorMessage } from '../../api/client.js';

/** Entities landing — FTS search across the unified entity graph (collector
 *  NER + OSINT-search discoveries). Kanji-substring works (kuromoji FTS).
 *  `/entities?q=…` pre-fills the box (saved-search runs and deep links). */
export default function EntitiesPage() {
  const navigate = useNavigate();
  const [params] = useSearchParams();
  const [q, setQ] = React.useState(() => params.get('q') || '');
  React.useEffect(() => { const pq = params.get('q'); if (pq != null && pq !== '') setQ(pq); }, [params]);
  // Hybrid = FTS + the entity vector index fused by RRF (?mode=hybrid,
  // entityapi.c). Off by default: it embeds the query on every settled
  // keystroke, and the plain FTS answer is what this box always gave.
  const [hybrid, setHybrid] = React.useState(false);
  const { results, loading, error, resolved, meta } = useEntitySearch(q, undefined, hybrid ? 'hybrid' : undefined);
  const arms = meta?.arms;
  // GET /api/export/entities?q= — every matching entity (full-text, the
  // export's own filter), not just the results on screen; the server states
  // its row cap in the file. Wired 2026-11-07; there was no UI for it.
  const [exportState, setExportState] = React.useState({ busy: false, error: null });
  const exportEntities = async () => {
    setExportState({ busy: true, error: null });
    try { await downloadExport('entities', { q: q.trim() || undefined }, 'csv'); setExportState({ busy: false, error: null }); }
    catch (e) { setExportState({ busy: false, error: errorMessage(e) }); }
  };
  // The box searches as you type, so a keystroke is never recorded. A search
  // is committed by Enter, by opening a result, or by arriving with ?q= (a
  // link, a history or saved-search re-run) once its results are in.
  const settled = !loading && !error && q.trim() !== '' && resolved === q.trim();
  const commit = () => { if (settled) recordSearch('entity', { q: q.trim() }, results.length); };
  const arrivedQ = params.get('q') || '';
  useRecordSearch('entity', { q: arrivedQ.trim() }, {
    enabled: arrivedQ.trim() !== '' && q.trim() === arrivedQ.trim(),
    ready: settled, resultCount: results.length,
  });

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
          onKeyDown={(e) => { if (e.key === 'Enter') commit(); }}
          placeholder="search entities (kanji or romaji)…"
          className="w-full px-3 py-2 rounded bg-osint-bg border border-osint-border text-sm text-osint-text focus:border-neon-cyan/50 outline-none"
        />

        <div className="flex items-center gap-2 text-xs text-osint-muted">
          <button type="button" disabled={exportState.busy} onClick={exportEntities} className="px-2 py-0.5 rounded border border-osint-border hover:text-neon-cyan disabled:opacity-40">
            {exportState.busy ? 'Exporting…' : q.trim() ? 'Export matching entities (CSV)' : 'Export all entities (CSV)'}
          </button>
          {exportState.error && <span className="text-red-300 font-mono">export refused — {exportState.error}</span>}
        </div>
        <label className="flex items-center gap-2 text-xs text-osint-muted">
          <input type="checkbox" checked={hybrid} onChange={(e) => setHybrid(e.target.checked)} />
          also match by meaning (hybrid: full-text + vector index)
        </label>
        {hybrid && arms && !loading && !error && (
          <div className="text-[11px] text-osint-muted font-mono">
            {arms.mode === 'hybrid'
              ? `fused ${arms.fusion || 'rrf'}: full-text ${arms.fts?.hits ?? 0} hit(s)${arms.fts?.total != null ? ` of ${arms.fts.total}` : ''}, vector ${arms.vector?.hits ?? 0} hit(s) — each arm's top ${arms.window ?? '?'}`
              : `vector arm not used${arms.vector?.reason ? ` — ${arms.vector.reason}` : ''}; this is the plain full-text answer`}
          </div>
        )}
        {hybrid && !arms && meta && !loading && !error && (
          <div className="text-[11px] text-osint-muted font-mono">the server answered without a vector arm{meta.note ? ` — ${meta.note}` : ''}; this is the plain full-text answer</div>
        )}

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
                  onClick={() => { commit(); navigate(`/entities/${encodeURIComponent(String(e.type).toLowerCase())}/${encodeURIComponent(e.entity_id)}`); }}
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
