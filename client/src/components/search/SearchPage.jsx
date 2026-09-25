import React, { useCallback, useEffect, useRef, useState } from 'react';
import { useSearchParams } from 'react-router-dom';
import { LuHistory, LuSearch } from 'react-icons/lu';
import { useSearchStore } from '../../hooks/useSearch.js';
import { attachRun } from '../../store/searchStore.js';
import { api } from '../../api/client.js';
import SearchCard from './SearchCard.jsx';
import SearchAura from './SearchAura.jsx';
import SearchHistoryDropdown from './SearchHistoryDropdown.jsx';
import { isJapanese } from './pipeline.js';
import { Button, Input, Pill, SectionLabel, ErrorNotice, EmptyState, cx } from '../ui/kit.jsx';

/**
 * Search tab — the ported entity-pivot search (iOS `SearchTab`). Query box +
 * LLM suggestions, live SSE progress cards, completed history. "Pivot" on any
 * entity starts a fresh investigation (the multi-round flow).
 *
 * Deep links: `?q=<query>` runs a query; `?run=<request_id>` re-attaches to a
 * run; `?t=<permalink token>` resolves a view-state token (kind osint/intel
 * with params.q) — see RunActions.SearchShareSheet.
 */
export default function SearchPage() {
  const { active, completed, lastError, startSearch, fetchSuggestions } = useSearchStore();
  const [query, setQuery] = useState('');
  const [suggestions, setSuggestions] = useState([]);
  const [busy, setBusy] = useState(false);
  const [err, setErr] = useState(null);
  const [historyOpen, setHistoryOpen] = useState(false);
  const [linkNotice, setLinkNotice] = useState(null);
  const [searchParams, setSearchParams] = useSearchParams();
  const consumed = useRef(false);

  useEffect(() => {
    const q = query.trim();
    if (q.length < 3) { setSuggestions([]); return undefined; }
    let alive = true;
    // The iOS tab waits 3s of settled typing before asking the LLM; the web
    // used 350ms, which spammed the suggest model on every keystroke.
    const t = setTimeout(async () => {
      const s = await fetchSuggestions(q);
      if (alive) setSuggestions(s);
    }, 1500);
    return () => { alive = false; clearTimeout(t); };
  }, [query, fetchSuggestions]);

  const run = useCallback(async (q) => {
    const qq = (q ?? query).trim();
    if (!qq || busy) return;
    setBusy(true); setErr(null); setSuggestions([]); setHistoryOpen(false);
    try { await startSearch(qq); setQuery(''); }
    catch (e) { setErr(e); }
    finally { setBusy(false); }
  }, [query, busy, startSearch]);

  // Consume a deep link exactly once, then strip it so a refresh doesn't
  // replay the run.
  useEffect(() => {
    if (consumed.current) return;
    const q = searchParams.get('q');
    const runId = searchParams.get('run');
    const token = searchParams.get('t');
    if (!q && !runId && !token) return;
    consumed.current = true;
    (async () => {
      try {
        if (q) { await run(q); }
        else if (runId) { await attachRun(runId); }
        else if (token) {
          const r = await api.get(`/api/permalink/${encodeURIComponent(token)}`);
          const st = r?.data || {};
          const pq = st.params?.q ?? st.params?.query;
          if (st.params?.request_id) {
            try { await attachRun(st.params.request_id); }
            catch { if (pq) await run(pq); else throw new Error('the linked run is no longer on the server and the link carried no query'); }
          } else if (pq) {
            await run(pq);
          } else {
            setLinkNotice(`This link opens a “${st.kind || 'unknown'}” view, not an OSINT search — nothing to run here.`);
          }
        }
      } catch (e) {
        setErr(e);
      } finally {
        setSearchParams({}, { replace: true });
      }
    })();
  }, [searchParams, setSearchParams, run]);

  const aura = busy || active.length > 0 ? 1 : 0;
  const jp = isJapanese(query);

  return (
    <div className="h-full overflow-auto relative">
      <SearchAura intensity={aura} />
      <div className="relative max-w-3xl mx-auto p-4 md:p-6 space-y-4">
        <header>
          <h1 className="font-mono text-xl font-bold tracking-tight text-osint-text">OSINT Search</h1>
          <p className="text-xs text-osint-muted">
            Pivot on any entity — person, org, domain, IP, vessel, location — across external sources and the JapanOSINT corpus.
          </p>
        </header>

        <div className="relative">
          <form onSubmit={(e) => { e.preventDefault(); run(); }} className="flex gap-2">
            <div className="relative flex-1">
              <LuSearch size={14} className="absolute left-2.5 top-1/2 -translate-y-1/2 text-osint-muted pointer-events-none" />
              <Input
                value={query}
                onChange={(e) => setQuery(e.target.value)}
                onFocus={() => setHistoryOpen(false)}
                placeholder="email, IP, domain, name, company, vessel IMO…"
                className="pl-8 pr-8"
                autoComplete="off"
                spellCheck={false}
              />
              <button
                type="button"
                onClick={() => setHistoryOpen((v) => !v)}
                title="Recent searches"
                aria-label="Recent searches"
                className={cx('absolute right-2 top-1/2 -translate-y-1/2', historyOpen ? 'text-accent' : 'text-osint-muted hover:text-accent')}
              >
                <LuHistory size={14} />
              </button>
            </div>
            <Button type="submit" variant="primary" size="lg" busy={busy} disabled={!query.trim()}>Investigate</Button>
          </form>
          {historyOpen && (
            <div className="absolute left-0 right-0 top-full mt-1 z-30">
              <SearchHistoryDropdown open={historyOpen} onRun={run} onClose={() => setHistoryOpen(false)} />
            </div>
          )}
        </div>

        {jp && (
          <div className="flex items-center gap-2 text-[11px] text-osint-muted">
            <Pill tone="accent">日本語</Pill>
            Japanese query — the corpus lookup normalises kana/kanji and romaji on the server; nothing is translated on this page.
          </div>
        )}

        {(err || lastError) && <ErrorNotice error={err || new Error(lastError)} title="Search could not start" />}
        {linkNotice && <div className="text-xs text-accent border border-accent/40 bg-accent/10 rounded-md px-3 py-2">{linkNotice}</div>}

        {suggestions.length > 0 && (
          <div className="flex flex-wrap gap-1.5">
            {suggestions.map((s, i) => (
              <button
                key={i}
                type="button"
                onClick={() => run(s)}
                className="text-[11px] px-2 py-1 rounded-full border border-osint-border text-osint-muted hover:text-accent hover:border-accent/40"
              >
                {s}
              </button>
            ))}
          </div>
        )}

        {active.length > 0 && (
          <section className="space-y-2">
            <SectionLabel right={<span className="font-mono text-[10px] text-osint-muted">{active.length}</span>}>Active</SectionLabel>
            {active.map((a) => (
              <SearchCard key={a.request_id} run={a} snapshot={a.snapshot} query={a.query} onPivot={run} defaultExpanded />
            ))}
          </section>
        )}

        {completed.length > 0 && (
          <section className="space-y-2">
            <SectionLabel right={<span className="font-mono text-[10px] text-osint-muted">{completed.length} of the last 30 kept in this tab</span>}>Completed</SectionLabel>
            {completed.map((c) => (
              <SearchCard key={c.request_id} run={c} snapshot={c.snapshot} query={c.query} onPivot={run} />
            ))}
          </section>
        )}

        {active.length === 0 && completed.length === 0 && (
          <EmptyState title="No investigations yet.">Enter an entity above to begin, or reopen a recent search from the history button.</EmptyState>
        )}
      </div>
    </div>
  );
}
