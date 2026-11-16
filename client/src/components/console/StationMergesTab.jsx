import React, { useEffect, useState } from 'react';
import { LuRefreshCw, LuCheck, LuX } from 'react-icons/lu';
import { api, ApiError } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { Section, Pill, Button, Input, Segmented, ErrorNotice, EmptyState, LoadingState, PagedFooter, toast } from '../ui/kit.jsx';

/**
 * Station-merge review — GET /api/admin/station-merges?status=… and
 * POST /api/admin/station-merges/:id/approve|reject (core/station_merges.c).
 * The station clusterer asks an LLM judge about pairs its own name rule could
 * not decide and records each verdict as PENDING with the model and its
 * reason. Nothing merges until an operator approves: the clusterer applies
 * only same=1 AND status=approved, on its next run. A decision can be
 * reversed; each one is written to the audit chain by the server.
 */

const STATUSES = [
  { value: 'pending', label: 'Pending' },
  { value: 'approved', label: 'Approved' },
  { value: 'rejected', label: 'Rejected' },
  { value: 'all', label: 'All' },
];
const STATUS_TONE = { pending: 'warning', approved: 'accent', rejected: 'neutral' };
const PAGE = 100;

const forbidden = (e) => e instanceof ApiError && e.status === 403;

export function MergeRow({ m, onDecided }) {
  const [busy, setBusy] = useState(null);
  const [note, setNote] = useState('');
  const [err, setErr] = useState(null);
  const decide = async (approve) => {
    setBusy(approve ? 'approve' : 'reject'); setErr(null);
    try {
      const body = note.trim() ? { note: note.trim() } : undefined;
      const r = await api.post(`/api/admin/station-merges/${encodeURIComponent(m.id)}/${approve ? 'approve' : 'reject'}`, body);
      onDecided(r?.data);
      toast(`${m.name_a || m.uid_a} / ${m.name_b || m.uid_b}: ${approve ? 'approved' : 'rejected'}`, { tone: 'success' });
    } catch (e) { setErr(e); } finally { setBusy(null); }
  };
  return (
    <li className="px-3 py-3 space-y-2" data-testid={`merge-${m.id}`}>
      <div className="flex items-center gap-2 flex-wrap">
        <span className="text-sm text-osint-text">{m.name_a || '—'}</span>
        <span className="text-osint-muted">↔</span>
        <span className="text-sm text-osint-text">{m.name_b || '—'}</span>
        <Pill tone={m.same ? 'accent' : 'neutral'}>{m.same ? 'judge: same' : 'judge: different'}</Pill>
        <Pill tone="neutral">conf {typeof m.confidence === 'number' ? m.confidence.toFixed(2) : '—'}</Pill>
        <Pill tone={STATUS_TONE[m.status] || 'neutral'}>{m.status}</Pill>
        {m.applied && <Pill tone="accent" title="Merged on the clusterer's next run">applied</Pill>}
        <span className="ml-auto text-[10px] font-mono text-osint-muted">
          {typeof m.dist_m === 'number' ? `${Math.round(m.dist_m)} m · ` : ''}{m.model || 'model unknown'} · {m.decided_at || ''}
        </span>
      </div>
      <div className="text-[10px] font-mono text-osint-muted">{m.uid_a} · {m.uid_b}</div>
      {m.reason && <div className="text-[11px] text-osint-muted">“{m.reason}”</div>}
      {m.reviewed_by && <div className="text-[10px] font-mono text-osint-muted">reviewed by {m.reviewed_by}{m.reviewed_at ? ` · ${m.reviewed_at}` : ''}</div>}
      <div className="flex items-center gap-2 flex-wrap">
        <Input aria-label={`note for ${m.id}`} className="flex-1 min-w-[10rem]" placeholder="Note for the audit log (optional)" maxLength={500} value={note} onChange={(e) => setNote(e.target.value)} />
        <Button size="sm" variant="primary" busy={busy === 'approve'} disabled={busy !== null || m.status === 'approved'} onClick={() => decide(true)}><LuCheck size={12} /> Approve</Button>
        <Button size="sm" busy={busy === 'reject'} disabled={busy !== null || m.status === 'rejected'} onClick={() => decide(false)}><LuX size={12} /> Reject</Button>
      </div>
      {err && <ErrorNotice error={err} title={forbidden(err) ? 'The server refused this account (platform-operator allowlist)' : 'Could not record the decision'} />}
    </li>
  );
}

export default function StationMergesTab() {
  const [status, setStatus] = useState('pending');
  const { data, error, loading, reload } = useApi(`/api/admin/station-merges?status=${status}&limit=${PAGE}`, { deps: [status] });
  const [rows, setRows] = useState([]);
  const [page, setPage] = useState(null);
  const [moreBusy, setMoreBusy] = useState(false);
  const [moreErr, setMoreErr] = useState(null);
  useEffect(() => { setRows(Array.isArray(data?.data) ? data.data : []); setPage(data?.page || null); }, [data]);
  const meta = data?.meta || {};

  // A decided row leaves a status-filtered list; it stays (updated) under "all".
  const decided = (row) => {
    if (!row?.id) return;
    setRows((rs) => (status === 'all' || row.status === status ? rs.map((r) => (r.id === row.id ? row : r)) : rs.filter((r) => r.id !== row.id)));
  };
  const more = async () => {
    setMoreBusy(true); setMoreErr(null);
    try {
      const r = await api.get(`/api/admin/station-merges?status=${status}&limit=${PAGE}&offset=${rows.length}`);
      setRows((rs) => [...rs, ...(Array.isArray(r?.data) ? r.data : [])]);
      setPage(r?.page || null);
    } catch (e) { setMoreErr(e); } finally { setMoreBusy(false); }
  };

  const counts = meta.counts || {};
  return (
    <div className="space-y-4">
      <Section
        label="Station merges"
        right={<div className="flex items-center gap-2"><Segmented value={status} onChange={setStatus} options={STATUSES} /><Button size="sm" variant="ghost" onClick={() => reload()}><LuRefreshCw size={12} /></Button></div>}
        padded={false}
      >
        <div className="px-3 pt-2 text-[11px] text-osint-muted">
          Verdicts the LLM judge gave on station pairs the clusterer's name rule could not decide. Only an approved “same” verdict merges, on the clusterer's next run; the confidence is the judge's, for you to weigh. Pending {counts.pending ?? '—'} · approved {counts.approved ?? '—'} · rejected {counts.rejected ?? '—'}.
          {meta.judge_enabled === false && ' The judge is not running here (no LLM_BASE_URL, or JO_STATION_JUDGE_MAX=0), so no new verdicts are being recorded.'}
          {meta.judge_enabled && ` Up to ${meta.judge_budget_per_run} pairs are asked per clusterer run.`}
        </div>
        {loading && !data && <LoadingState />}
        {error && <div className="p-2"><ErrorNotice error={error} title={forbidden(error) ? 'The server refused this account (platform-operator allowlist)' : 'Could not load station merges'} onRetry={reload} /></div>}
        {!error && data && rows.length === 0 && <EmptyState title={`No ${status === 'all' ? '' : `${status} `}verdicts.`} />}
        {rows.length > 0 && <ul className="divide-y divide-osint-border">{rows.map((m) => <MergeRow key={m.id} m={m} onDecided={decided} />)}</ul>}
        {page && <div className="px-3 pb-2"><PagedFooter shown={rows.length} total={page.total} hasMore={page.has_more} busy={moreBusy} onMore={more} error={moreErr} noun="verdicts" /></div>}
      </Section>
    </div>
  );
}
