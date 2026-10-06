import React, { useCallback, useEffect, useRef, useState } from 'react';
import { LuRefreshCw, LuUsers } from 'react-icons/lu';
import { api } from '../../api/client.js';
import { useMemberNames, authorLabel } from '../../hooks/useMembers.js';
import { relativeTime, fmtAbs } from '../../utils/time.js';
import { Pill, Button, Segmented, SectionLabel, ErrorNotice, LoadingState, BoundNote } from '../ui/kit.jsx';

/**
 * Every OSINT investigation started in this workspace, by any member —
 * the query, who ran it, its status and a preview of the LLM synthesis.
 *   GET /api/search/runs?limit&mine=1&cursor → {data:[{request_id, query,
 *       user_id, mine, created_at, status, phase, degraded?,
 *       synthesis_preview, synthesis_bytes?, synthesis_truncated?}],
 *       page:{limit, count, total, next_cursor}, meta:{scope}}
 * Decided 2026-10-05: everything in a workspace is visible to its members.
 * The list above it ("Active"/"Completed") is only what THIS tab started;
 * opening a row here attaches the full run through /api/search/results/:id.
 */

const STATUS_TONE = { running: 'accent', completed: 'success', error: 'danger', unknown: 'neutral' };

export function runStatusTone(status) { return STATUS_TONE[status] || 'neutral'; }

export function runStatusTitle(row) {
  if (row?.status === 'unknown') return 'No live or stored record of this run: it was interrupted by a restart or failed before it finished.';
  if (row?.status === 'running') return `${row.phase || 'running'}${row.progress_percent != null ? ` · ${row.progress_percent}%` : ''}`;
  return row?.phase || row?.status || '';
}

/** The synthesis preview, saying when it is only the start of a longer text. */
export function synthesisLine(row) {
  const p = row?.synthesis_preview;
  if (!p) return null;
  if (!row.synthesis_truncated) return p;
  return `${p.trimEnd()}… (preview of ${row.synthesis_bytes} bytes — open the run to read it all)`;
}

export const RUNS_PAGE = 50;

export default function WorkspaceRuns({ onOpen }) {
  const names = useMemberNames();
  const [who, setWho] = useState('all');
  const [rows, setRows] = useState([]);
  const [page, setPage] = useState(null);
  const [meta, setMeta] = useState(null);
  const [error, setError] = useState(null);
  const [loading, setLoading] = useState(true);
  const [loadingMore, setLoadingMore] = useState(false);
  const [opening, setOpening] = useState(null);
  const seq = useRef(0);

  const fetchPage = useCallback((cursor) => api.get('/api/search/runs', {
    query: { limit: RUNS_PAGE, mine: who === 'mine' ? '1' : undefined, cursor: cursor || undefined },
  }), [who]);

  const load = useCallback(async () => {
    const my = ++seq.current;
    setLoading(true);
    try {
      const j = await fetchPage(null);
      if (my !== seq.current) return;
      setRows(Array.isArray(j?.data) ? j.data : []);
      setPage(j?.page || null); setMeta(j?.meta || null); setError(null);
    } catch (e) {
      if (my === seq.current) setError(e);
    } finally { if (my === seq.current) setLoading(false); }
  }, [fetchPage]);

  useEffect(() => { load(); }, [load]);

  const more = async () => {
    if (!page?.next_cursor || loadingMore) return;
    const my = seq.current;
    setLoadingMore(true);
    try {
      const j = await fetchPage(page.next_cursor);
      if (my !== seq.current) return;
      setRows((xs) => [...xs, ...(Array.isArray(j?.data) ? j.data : [])]);
      setPage(j?.page || null); setMeta(j?.meta || null);
    } catch (e) { if (my === seq.current) setError(e); }
    finally { setLoadingMore(false); }
  };

  const open = async (r) => {
    setOpening(r.request_id);
    try { await onOpen(r.request_id); } catch (e) { setError(e); }
    finally { setOpening(null); }
  };

  return (
    <section className="space-y-2">
      <SectionLabel right={(
        <div className="flex items-center gap-1">
          <Segmented value={who} onChange={setWho} options={[{ value: 'all', label: 'everyone' }, { value: 'mine', label: 'mine' }]} />
          <Button size="sm" variant="ghost" onClick={() => load()} title="Reload"><LuRefreshCw size={11} /></Button>
        </div>
      )}>Workspace runs</SectionLabel>
      <div className="text-[11px] text-osint-muted flex items-center gap-1">
        <LuUsers size={11} /> {meta?.scope === 'user' ? 'Your investigations in this workspace.' : 'Every member’s investigations in this workspace, with who ran each one.'}
      </div>
      {error && <ErrorNotice error={error} title="Could not load the workspace's runs" onRetry={load} />}
      {loading && rows.length === 0 && <LoadingState label="Loading runs…" />}
      {!loading && !error && rows.length === 0 && <div className="text-xs text-osint-muted">No investigations have been run in this workspace yet.</div>}
      {rows.length > 0 && (
        <ul className="divide-y divide-osint-border rounded-[10px] border border-osint-border">
          {rows.map((r) => (
            <li key={r.request_id} className="px-3 py-2 flex items-start gap-2 text-xs">
              <div className="min-w-0 flex-1">
                <div className="flex items-center gap-2 flex-wrap">
                  <span className="text-osint-text font-medium truncate">{r.query || '(query not recorded)'}</span>
                  <Pill tone={runStatusTone(r.status)} title={runStatusTitle(r)}>{r.status}</Pill>
                  {r.degraded && <Pill tone="warning" title="A stage of this run did not complete">degraded</Pill>}
                </div>
                <div className="text-[11px] text-osint-muted mt-0.5">
                  <span title={r.user_id || undefined}>{authorLabel(r, names)}</span> · <span title={fmtAbs(r.created_at)}>{relativeTime(r.created_at)}</span>
                </div>
                {synthesisLine(r) && <div className="text-[11px] text-osint-muted mt-1 line-clamp-2 break-words">{synthesisLine(r)}</div>}
              </div>
              <Button size="sm" busy={opening === r.request_id} onClick={() => open(r)} title="Open this run">Open</Button>
            </li>
          ))}
        </ul>
      )}
      {rows.length > 0 && (
        <div className="flex items-center justify-between gap-2">
          <BoundNote shown={rows.length} total={page?.total ?? null} more={Boolean(page?.next_cursor)} noun="runs" />
          {page?.next_cursor && <Button size="sm" busy={loadingMore} onClick={more}>Load more</Button>}
        </div>
      )}
    </section>
  );
}
