import React, { useMemo, useState } from 'react';
import { Link } from 'react-router-dom';
import { LuFolder, LuPlus, LuRefreshCw, LuDownload, LuArchive, LuCheck } from 'react-icons/lu';
import { api, errorMessage } from '../../api/client.js';
import {
  usePaged, CASE_STATUSES, CaseStatus, CASE_PRIORITIES, CasePriority, downloadBlob, filenameFromDisposition,
} from '../../hooks/useCases.js';
import {
  Page, Card, Pill, Button, Input, TextArea, Field, Select, Segmented, Sheet, ErrorNotice, EmptyState,
  LoadingState, BoundNote, cx, toast,
} from '../ui/kit.jsx';
import { relativeTime } from '../../utils/time.js';

/**
 * Cases tab — the iOS `CasesTab`. Status filter (server-side `?status=`),
 * client-side name search over the LOADED rows, keyset paging with
 * load-more, create in a sheet, quick status changes from each row.
 */
const STATUS_OPTIONS = [{ value: '', label: 'All' }, ...CASE_STATUSES.map((s) => ({ value: s, label: CaseStatus.label(s) }))];

export default function CasesPage() {
  const [status, setStatus] = useState('open');
  const [search, setSearch] = useState('');
  const [showCreate, setShowCreate] = useState(false);
  const [rowError, setRowError] = useState(null);
  const [busyId, setBusyId] = useState(null);
  const [exporting, setExporting] = useState(false);

  const path = status ? `/api/cases?status=${encodeURIComponent(status)}` : '/api/cases';
  const { rows, setRows, hasMore, error, loading, loadingMore, reload, loadMore } = usePaged(path, { limit: 50 });

  const visible = useMemo(() => {
    const q = search.trim().toLowerCase();
    if (!q) return rows;
    return rows.filter((c) => (c.name || '').toLowerCase().includes(q) || (c.summary || '').toLowerCase().includes(q) || String(c.id).toLowerCase().includes(q));
  }, [rows, search]);

  const patchStatus = async (c, next) => {
    setBusyId(c.id); setRowError(null);
    try {
      const r = await api.patch(`/api/cases/${encodeURIComponent(c.id)}`, { status: next });
      const updated = r?.data ?? r;
      // The row leaves the current status filter when it no longer matches.
      setRows((xs) => (status && updated?.status !== status ? xs.filter((x) => x.id !== c.id) : xs.map((x) => (x.id === c.id ? { ...x, ...updated } : x))));
      toast(`${c.name}: ${CaseStatus.label(next).toLowerCase()}`, { tone: 'accent' });
    } catch (e) { setRowError(e); }
    finally { setBusyId(null); }
  };

  const exportCases = async (format) => {
    setExporting(true);
    try {
      const res = await api.raw(`/api/export/case?format=${format}`);
      if (!res.ok) {
        let body = null; try { body = await res.json(); } catch { /* not json */ }
        throw new Error(body?.error ? `HTTP ${res.status}: ${body.error}` : `HTTP ${res.status}`);
      }
      const blob = await res.blob();
      downloadBlob(blob, filenameFromDisposition(res.headers.get('content-disposition'), `cases.${format}`));
      toast('Export downloaded', { tone: 'accent' });
    } catch (e) { toast(`Export failed: ${errorMessage(e)}`, { tone: 'danger' }); }
    finally { setExporting(false); }
  };

  return (
    <Page
      title="Cases"
      subtitle="Investigations: pin intel items, entities and map features as findings, keep notes and an activity trail, and export a report."
      actions={(
        <>
          <Button onClick={reload} title="Reload cases"><LuRefreshCw size={13} /></Button>
          <Button busy={exporting} onClick={() => exportCases('csv')} title="Export every case as CSV (GET /api/export/case)"><LuDownload size={13} /> CSV</Button>
          <Button busy={exporting} onClick={() => exportCases('json')} title="Export every case as JSON"><LuDownload size={13} /> JSON</Button>
          <Button variant="primary" onClick={() => setShowCreate(true)}><LuPlus size={13} /> New case</Button>
        </>
      )}
    >
      <div className="flex flex-wrap items-center gap-2">
        <Segmented value={status} onChange={setStatus} options={STATUS_OPTIONS} />
        <Input className="flex-1 min-w-[160px]" placeholder="Filter loaded cases by name, summary or id…" value={search} onChange={(e) => setSearch(e.target.value)} />
      </div>

      {error && <ErrorNotice error={error} title="Could not load cases" onRetry={reload} />}
      {rowError && <ErrorNotice error={rowError} title="Status change failed" />}
      {loading && <LoadingState label="Loading cases…" />}

      {!loading && !error && rows.length === 0 && (
        <EmptyState
          icon={<LuFolder size={22} className="mx-auto" />}
          title={status ? `No ${CaseStatus.label(status).toLowerCase()} cases.` : 'No cases yet.'}
          action={<Button variant="primary" onClick={() => setShowCreate(true)}><LuPlus size={13} /> New case</Button>}
        >
          {status ? 'Nothing in this status. Switch to All to see every case in the workspace.' : 'Create a case, then pin findings into it from Intel, Entities, Saved or the map.'}
        </EmptyState>
      )}

      {!loading && rows.length > 0 && visible.length === 0 && (
        <EmptyState title="No loaded case matches the filter.">
          Older cases may not be fetched yet — load more below, then search again.
        </EmptyState>
      )}

      {visible.length > 0 && (
        <ul className="space-y-2">
          {visible.map((c) => (
            <li key={c.id}>
              <Card padded={false} className="hover:border-accent/50 transition-colors">
                <div className="flex items-start gap-3 p-3">
                  <span className={cx('flex items-center justify-center w-8 h-8 rounded-md flex-shrink-0',
                    c.status === 'open' ? 'bg-neon-green/10 text-neon-green' : c.status === 'archived' ? 'bg-neon-cyan/10 text-neon-cyan' : 'bg-osint-panel text-osint-muted')}>
                    {c.status === 'closed' ? <LuCheck size={15} /> : c.status === 'archived' ? <LuArchive size={15} /> : <LuFolder size={15} />}
                  </span>
                  <div className="min-w-0 flex-1">
                    <Link to={`/cases/${encodeURIComponent(c.id)}`} className="block text-sm font-medium text-osint-text hover:text-accent truncate">{c.name || c.id}</Link>
                    {c.summary && <div className="text-xs text-osint-muted line-clamp-2 mt-0.5">{c.summary}</div>}
                    <div className="flex flex-wrap items-center gap-1.5 mt-1.5">
                      <Pill tone={CaseStatus.tone(c.status)}>{CaseStatus.label(c.status).toUpperCase()}</Pill>
                      {c.priority > 0 && <Pill tone={CasePriority.tone(c.priority)}>{CasePriority.label(c.priority).toUpperCase()}</Pill>}
                      <span className="text-[11px] text-osint-muted font-mono">
                        {c.item_count != null && <>{c.item_count} pinned · </>}updated {relativeTime(c.updated_at)}
                      </span>
                    </div>
                  </div>
                  <div className="flex flex-col gap-1 flex-shrink-0">
                    {c.status !== 'open' && <Button size="sm" busy={busyId === c.id} onClick={() => patchStatus(c, 'open')}>Reopen</Button>}
                    {c.status === 'open' && <Button size="sm" busy={busyId === c.id} onClick={() => patchStatus(c, 'closed')}>Close</Button>}
                    {c.status !== 'archived' && <Button size="sm" variant="ghost" busy={busyId === c.id} onClick={() => patchStatus(c, 'archived')}>Archive</Button>}
                  </div>
                </div>
              </Card>
            </li>
          ))}
        </ul>
      )}

      {rows.length > 0 && (
        <div className="flex items-center justify-between gap-2">
          {hasMore
            ? <span className="text-[11px] text-accent font-mono">showing {visible.length} of {rows.length} loaded · more on the server (keyset paging, no total)</span>
            : <BoundNote shown={visible.length} total={rows.length} noun="cases" />}
          {hasMore && <Button busy={loadingMore} onClick={loadMore}>Load more</Button>}
        </div>
      )}

      <CreateCaseSheet open={showCreate} onClose={() => setShowCreate(false)} onCreated={() => { setShowCreate(false); reload(); }} />
    </Page>
  );
}

/** New case (POST /api/cases). */
export function CreateCaseSheet({ open, onClose, onCreated }) {
  const [name, setName] = useState('');
  const [summary, setSummary] = useState('');
  const [priority, setPriority] = useState(0);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState(null);

  const submit = async () => {
    const n = name.trim();
    if (!n) return;
    setBusy(true); setError(null);
    try {
      const body = { name: n, priority: Number(priority) };
      if (summary.trim()) body.summary = summary.trim();
      const r = await api.post('/api/cases', body);
      toast('Case created', { tone: 'accent' });
      setName(''); setSummary(''); setPriority(0);
      onCreated?.(r?.data ?? r);
    } catch (e) { setError(e); }
    finally { setBusy(false); }
  };

  return (
    <Sheet open={open} onClose={onClose} title="New case" footer={(
      <>
        <Button onClick={onClose}>Cancel</Button>
        <Button variant="primary" busy={busy} disabled={!name.trim()} onClick={submit}>Create</Button>
      </>
    )}>
      <div className="space-y-3">
        <Field label="Name" hint="What you'd call this investigation. Required, up to 200 characters.">
          <Input value={name} maxLength={200} onChange={(e) => setName(e.target.value)} autoFocus placeholder="e.g. Port of Yokohama — vessel cluster" />
        </Field>
        <Field label="Summary" hint="Rendered as the executive summary of the exported report.">
          <TextArea value={summary} onChange={(e) => setSummary(e.target.value)} placeholder="Optional" />
        </Field>
        <Field label="Priority">
          <Select value={priority} onChange={(e) => setPriority(Number(e.target.value))}>
            {CASE_PRIORITIES.map((p) => <option key={p} value={p}>{p} · {CasePriority.label(p)}</option>)}
          </Select>
        </Field>
        {error && <ErrorNotice error={error} title="Create failed" />}
      </div>
    </Sheet>
  );
}
