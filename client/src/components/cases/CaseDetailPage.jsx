import React, { Suspense, useEffect, useMemo, useState } from 'react';
import { Link, useNavigate, useParams } from 'react-router-dom';
import { LuPencil, LuTrash2, LuFileText, LuRefreshCw, LuPinOff, LuArrowLeft, LuMessageSquare, LuExternalLink } from 'react-icons/lu';
import { api, errorMessage } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { useAuth } from '../../auth/AuthContext.jsx';
import {
  usePaged, CASE_STATUSES, CaseStatus, CASE_PRIORITIES, CasePriority, CaseRefType, CaseActivityKind,
  CASE_ROLES, refLink, downloadBlob, filenameFromDisposition,
} from '../../hooks/useCases.js';
import {
  Page, Section, Card, Pill, Button, Input, TextArea, Field, Select, Segmented, Sheet,
  ErrorNotice, EmptyState, LoadingState, BoundNote, CopyButton, cx, toast,
} from '../ui/kit.jsx';
import { relativeTime, fmtAbs } from '../../utils/time.js';

/* Written concurrently by the Intel agent. Absence degrades to "no section"
 * — the Notes pane below is self-sufficient. */
const AnnotationsSection = React.lazy(() =>
  import('../intel/AnnotationsSection.jsx').catch(() => ({ default: () => null })));

const PANES = [
  { value: 'findings', label: 'Findings' },
  { value: 'notes', label: 'Notes' },
  { value: 'activity', label: 'Activity' },
  { value: 'members', label: 'Members' },
];

export default function CaseDetailPage() {
  const { id } = useParams();
  const navigate = useNavigate();
  const auth = useAuth();
  const [pane, setPane] = useState('findings');
  const [showEdit, setShowEdit] = useState(false);
  const [showReport, setShowReport] = useState(false);
  const [showDelete, setShowDelete] = useState(false);

  const detailPath = `/api/cases/${encodeURIComponent(id)}`;
  const { data, error, loading, reload, setData } = useApi(detailPath, { deps: [id] });
  const c = data?.data ?? null;

  const myRole = c?.my_case_role;
  const canWrite = auth.canManageWorkspace || myRole === 'lead' || myRole === 'contributor' || myRole == null;
  const canManageRoster = auth.canManageWorkspace || myRole === 'lead';

  const onSaved = (updated) => { setData({ ...data, data: { ...c, ...updated } }); setShowEdit(false); };

  const remove = async () => {
    try {
      await api.del(detailPath);
      toast('Case deleted');
      navigate('/cases', { replace: true });
    } catch (e) { toast(`Delete failed: ${errorMessage(e)}`, { tone: 'danger' }); }
  };

  return (
    <Page
      wide
      title={(
        <span className="flex items-center gap-2">
          <Link to="/cases" className="text-osint-muted hover:text-accent" title="All cases"><LuArrowLeft size={16} /></Link>
          {c?.name || (loading ? 'Loading…' : 'Case')}
        </span>
      )}
      subtitle={c ? (
        <span className="font-mono">
          created {relativeTime(c.created_at)} · updated {relativeTime(c.updated_at)}
          {c.closed_at && <> · closed {fmtAbs(c.closed_at)}</>}
          {c.created_by && <> · by {c.created_by}</>}
        </span>
      ) : null}
      actions={c && (
        <>
          <Button onClick={reload} title="Reload case"><LuRefreshCw size={13} /></Button>
          <Button onClick={() => setShowReport(true)}><LuFileText size={13} /> Report</Button>
          <Button disabled={!canWrite} onClick={() => setShowEdit(true)}><LuPencil size={13} /> Edit</Button>
          <Button variant="danger" disabled={!canWrite} onClick={() => setShowDelete(true)}><LuTrash2 size={13} /></Button>
        </>
      )}
    >
      {error && <ErrorNotice error={error} title="Could not load this case" onRetry={reload} />}
      {loading && !c && <LoadingState label="Loading case…" />}

      {c && (
        <>
          <Card>
            <div className="flex flex-wrap items-center gap-1.5">
              <Pill tone={CaseStatus.tone(c.status)}>{CaseStatus.label(c.status).toUpperCase()}</Pill>
              <Pill tone={CasePriority.tone(c.priority)}>PRIORITY {c.priority} · {CasePriority.label(c.priority).toUpperCase()}</Pill>
              <Pill tone="neutral">{c.item_count ?? 0} pinned</Pill>
              {myRole && <Pill tone="accent">you: {myRole}</Pill>}
              <span className="ml-auto text-[11px] text-osint-muted font-mono flex items-center gap-1">{c.id} <CopyButton text={c.id} label="copy id" /></span>
            </div>
            {c.summary
              ? <p className="text-sm text-osint-text mt-3 whitespace-pre-wrap">{c.summary}</p>
              : <p className="text-xs text-osint-muted mt-3">No summary yet. Add one — it becomes the executive summary of the exported report.</p>}
          </Card>

          <div className="overflow-x-auto">
            <Segmented size="md" value={pane} onChange={setPane} options={PANES.map((p) => ({
              ...p,
              count: p.value === 'findings' ? c.item_count : p.value === 'members' ? (c.members || []).length : undefined,
            }))} />
          </div>

          {pane === 'findings' && <FindingsPane caseId={id} counts={c.item_counts} canWrite={canWrite} onChanged={reload} />}
          {pane === 'notes' && <NotesPane caseId={id} canWrite={canWrite} />}
          {pane === 'activity' && <ActivityPane caseId={id} canWrite={canWrite} />}
          {pane === 'members' && <MembersPane caseId={id} members={c.members || []} canManage={canManageRoster} onChanged={reload} />}
        </>
      )}

      {c && <EditCaseSheet open={showEdit} onClose={() => setShowEdit(false)} caseRow={c} onSaved={onSaved} />}
      {c && <ReportSheet open={showReport} onClose={() => setShowReport(false)} caseRow={c} />}
      {c && <DeleteCaseSheet open={showDelete} onClose={() => setShowDelete(false)} caseRow={c} onConfirm={remove} />}
    </Page>
  );
}

/* ── Findings ───────────────────────────────────────────────────────────── */

function FindingsPane({ caseId, counts, canWrite, onChanged }) {
  const [refType, setRefType] = useState('');
  const path = `/api/cases/${encodeURIComponent(caseId)}/items${refType ? `?ref_type=${encodeURIComponent(refType)}` : ''}`;
  const { rows, setRows, hasMore, error, loading, loadingMore, reload, loadMore } = usePaged(path, { limit: 100, deps: [refType] });
  const [busyKey, setBusyKey] = useState(null);
  const [unpinError, setUnpinError] = useState(null);

  const typeOptions = useMemo(() => {
    const present = Object.entries(counts || {}).filter(([k, v]) => k !== 'total' && v > 0)
      .sort((a, b) => CaseRefType.sortIndex(a[0]) - CaseRefType.sortIndex(b[0]));
    return [{ value: '', label: 'All', count: counts?.total }, ...present.map(([k, v]) => ({ value: k, label: CaseRefType.label(k), count: v }))];
  }, [counts]);

  const grouped = useMemo(() => {
    const g = new Map();
    for (const it of rows) { if (!g.has(it.ref_type)) g.set(it.ref_type, []); g.get(it.ref_type).push(it); }
    return [...g.entries()].sort((a, b) => CaseRefType.sortIndex(a[0]) - CaseRefType.sortIndex(b[0]));
  }, [rows]);

  const unpin = async (it) => {
    const key = `${it.ref_type}:${it.ref_id}`;
    setBusyKey(key); setUnpinError(null);
    try {
      await api.del(`/api/cases/${encodeURIComponent(caseId)}/items?ref_type=${encodeURIComponent(it.ref_type)}&ref_id=${encodeURIComponent(it.ref_id)}`);
      setRows((xs) => xs.filter((x) => !(x.ref_type === it.ref_type && x.ref_id === it.ref_id)));
      toast('Unpinned');
      onChanged?.();
    } catch (e) { setUnpinError(e); }
    finally { setBusyKey(null); }
  };

  return (
    <div className="space-y-3">
      <div className="overflow-x-auto"><Segmented value={refType} onChange={setRefType} options={typeOptions} /></div>
      {error && <ErrorNotice error={error} title="Could not load pinned items" onRetry={reload} />}
      {unpinError && <ErrorNotice error={unpinError} title="Unpin failed" />}
      {loading && <LoadingState label="Loading findings…" />}
      {!loading && !error && rows.length === 0 && (
        <EmptyState title="Nothing pinned yet.">
          Pin intel items, entities, map features or cameras into this case from their own pages — every pin keeps a display snapshot of what was cited.
        </EmptyState>
      )}
      {grouped.map(([type, items]) => (
        <Section key={type} label={`${CaseRefType.label(type)} · ${items.length}`} padded={false}>
          <ul className="divide-y divide-osint-border">
            {items.map((it) => <FindingRow key={`${it.ref_type}:${it.ref_id}`} item={it} canWrite={canWrite} busy={busyKey === `${it.ref_type}:${it.ref_id}`} onUnpin={() => unpin(it)} />)}
          </ul>
        </Section>
      ))}
      {rows.length > 0 && (
        <div className="flex items-center justify-between">
          {hasMore
            ? <span className="text-[11px] text-accent font-mono">showing {rows.length} loaded · more on the server</span>
            : <BoundNote shown={rows.length} total={rows.length} noun="pinned items" />}
          {hasMore && <Button busy={loadingMore} onClick={loadMore}>Load more</Button>}
        </div>
      )}
    </div>
  );
}

function snapshotSummary(it) {
  const d = it.snapshot?.data;
  if (!d || typeof d !== 'object') return null;
  const title = d.title || d.name || d.canonical || d.display_name || d.label || null;
  const sub = d.summary || d.description || d.source_id || d.type || null;
  const link = d.link || d.url || null;
  return { title, sub, link, lat: d.lat ?? d.latitude, lon: d.lon ?? d.lng ?? d.longitude };
}

function FindingRow({ item, canWrite, busy, onUnpin }) {
  const navigate = useNavigate();
  const [open, setOpen] = useState(false);
  const s = snapshotSummary(item);
  const to = refLink(item.ref_type, item.ref_id, item.snapshot);
  const title = item.label || s?.title || `${item.ref_type} · ${item.ref_id}`;
  const resolved = item.snapshot?.resolved !== false && item.snapshot?.data != null;
  return (
    <li className="px-3 py-2">
      <div className="flex items-start gap-3">
        <div className="min-w-0 flex-1">
          {to
            ? <Link to={to} className="text-sm text-osint-text hover:text-accent font-medium block truncate">{title}</Link>
            : <div className="text-sm text-osint-text font-medium truncate">{title}</div>}
          {s?.sub && <div className="text-xs text-osint-muted truncate">{String(s.sub)}</div>}
          <div className="text-[11px] text-osint-muted font-mono mt-0.5 flex flex-wrap gap-x-2">
            <span>{item.ref_type} · {item.ref_id}</span>
            <span>pinned {relativeTime(item.added_at)}{item.added_by ? ` by ${item.added_by}` : ''}</span>
            {item.snapshot?.origin && <span>snapshot: {item.snapshot.origin}{item.snapshot.captured_at ? ` @ ${fmtAbs(item.snapshot.captured_at)}` : ''}</span>}
          </div>
          {!resolved && (
            <div className="text-[11px] text-accent mt-1">
              The referenced row couldn't be read when this was pinned, so there is no display copy. The pin is kept as a bare reference — it is still evidence that this was cited.
            </div>
          )}
        </div>
        <div className="flex items-center gap-1 flex-shrink-0">
          {s?.link && /^https?:\/\//i.test(String(s.link)) && (
            <a href={s.link} target="_blank" rel="noreferrer" className="text-osint-muted hover:text-accent p-1" title="Open source link"><LuExternalLink size={13} /></a>
          )}
          {Number.isFinite(Number(s?.lat)) && Number.isFinite(Number(s?.lon)) && (
            <Button size="sm" onClick={() => { navigate('/'); setTimeout(() => window.dispatchEvent(new CustomEvent('japanosint:flyto', { detail: { lat: Number(s.lat), lon: Number(s.lon), zoom: 14 } })), 150); }} title="Show on map">Map</Button>
          )}
          {resolved && <Button size="sm" variant="ghost" onClick={() => setOpen((v) => !v)}>{open ? 'Hide' : 'Data'}</Button>}
          {canWrite && <Button size="sm" variant="ghost" busy={busy} onClick={onUnpin} title="Unpin"><LuPinOff size={13} /></Button>}
        </div>
      </div>
      {open && resolved && (
        <pre className="mt-2 text-[11px] font-mono text-osint-muted bg-osint-bg border border-osint-border rounded-md p-2 overflow-x-auto max-h-64">
          {JSON.stringify(item.snapshot.data, null, 2)}
        </pre>
      )}
    </li>
  );
}

/* ── Notes (annotations scoped to the case) ─────────────────────────────── */

function NotesPane({ caseId, canWrite }) {
  const path = `/api/annotations?case_id=${encodeURIComponent(caseId)}&include_deleted=1`;
  const { rows, hasMore, error, loading, loadingMore, reload, loadMore } = usePaged(path, { limit: 200 });
  const items = usePaged(`/api/cases/${encodeURIComponent(caseId)}/items`, { limit: 100 });
  const [target, setTarget] = useState('');
  const [body, setBody] = useState('');
  const [busy, setBusy] = useState(false);
  const [postError, setPostError] = useState(null);

  useEffect(() => {
    if (!target && items.rows.length) setTarget(`${items.rows[0].ref_type} ${items.rows[0].ref_id}`);
  }, [items.rows, target]);

  const add = async () => {
    const [ref_type, ref_id] = target.split(' ');
    if (!ref_type || !ref_id || !body.trim()) return;
    setBusy(true); setPostError(null);
    try {
      await api.post('/api/annotations', { ref_type, ref_id, case_id: caseId, body_md: body.trim() });
      setBody('');
      toast('Note added', { tone: 'accent' });
      reload();
    } catch (e) { setPostError(e); }
    finally { setBusy(false); }
  };

  return (
    <div className="space-y-3">
      <Section label="Add a note">
        {items.error && <ErrorNotice error={items.error} title="Could not list pinned items to attach to" onRetry={items.reload} />}
        {!items.loading && !items.error && items.rows.length === 0 && (
          <div className="text-xs text-osint-muted">Pin a finding first — a note is attached to something.</div>
        )}
        {items.rows.length > 0 && (
          <div className="space-y-2">
            <Field label="Attach to">
              <Select value={target} onChange={(e) => setTarget(e.target.value)} className="w-full">
                {items.rows.map((it) => (
                  <option key={`${it.ref_type}:${it.ref_id}`} value={`${it.ref_type} ${it.ref_id}`}>
                    {CaseRefType.label(it.ref_type)} · {it.label || snapshotSummary(it)?.title || it.ref_id}
                  </option>
                ))}
              </Select>
            </Field>
            <TextArea value={body} onChange={(e) => setBody(e.target.value)} placeholder="Markdown note…" disabled={!canWrite} />
            {postError && <ErrorNotice error={postError} title="Note not saved" />}
            <div className="flex justify-end"><Button variant="primary" busy={busy} disabled={!canWrite || !body.trim()} onClick={add}>Add note</Button></div>
          </div>
        )}
      </Section>

      {error && <ErrorNotice error={error} title="Could not load notes" onRetry={reload} />}
      {loading && <LoadingState label="Loading notes…" />}
      {!loading && !error && rows.length === 0 && <EmptyState title="No notes on this case yet." />}
      {rows.length > 0 && (
        <Section label={`Notes · ${rows.length}`} padded={false}>
          <ul className="divide-y divide-osint-border">
            {rows.map((a) => (
              <li key={a.id} className={cx('px-3 py-2', a.is_deleted && 'opacity-60')}>
                <div className="text-[11px] text-osint-muted font-mono flex flex-wrap gap-x-2">
                  <span>{a.ref_type} · {a.ref_id}</span>
                  <span>{fmtAbs(a.created_at)}</span>
                  {a.author_id && <span>by {a.author_id}</span>}
                  {a.updated_at && a.updated_at !== a.created_at && <span>edited {relativeTime(a.updated_at)}</span>}
                  {a.is_deleted && <Pill tone="danger">note deleted</Pill>}
                </div>
                {a.is_deleted
                  ? <div className="text-xs text-osint-muted mt-1">The row survives on purpose — an audit trail you can quietly edit is not an audit trail.</div>
                  : <div className="text-sm text-osint-text whitespace-pre-wrap mt-1">{a.body_md}</div>}
              </li>
            ))}
          </ul>
        </Section>
      )}
      {hasMore && <div className="flex justify-end"><Button busy={loadingMore} onClick={loadMore}>Load more notes</Button></div>}
      <div className="text-[11px] text-osint-muted">Tombstones for deleted notes are only returned to analyst-and-up accounts; below that role the server omits them and says so in its meta.</div>
      <Suspense fallback={null}><AnnotationsSection refType="case" refId={caseId} caseId={caseId} /></Suspense>
    </div>
  );
}

/* ── Activity ───────────────────────────────────────────────────────────── */

function ActivityPane({ caseId, canWrite }) {
  const path = `/api/cases/${encodeURIComponent(caseId)}/activity`;
  const { rows, setRows, hasMore, error, loading, loadingMore, reload, loadMore } = usePaged(path, { limit: 100 });
  const [text, setText] = useState('');
  const [busy, setBusy] = useState(false);
  const [postError, setPostError] = useState(null);

  const comment = async () => {
    const t = text.trim();
    if (!t) return;
    setBusy(true); setPostError(null);
    try {
      const r = await api.post(path, { body: t });
      const row = r?.data ?? r;
      if (row?.id != null) setRows((xs) => [row, ...xs]); else reload();
      setText('');
    } catch (e) { setPostError(e); }
    finally { setBusy(false); }
  };

  return (
    <div className="space-y-3">
      <Section label="Comment">
        <div className="flex gap-2">
          <Input value={text} onChange={(e) => setText(e.target.value)} placeholder="Add a comment to the activity trail…" disabled={!canWrite}
            onKeyDown={(e) => { if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); comment(); } }} />
          <Button variant="primary" busy={busy} disabled={!canWrite || !text.trim()} onClick={comment}><LuMessageSquare size={13} /> Post</Button>
        </div>
        {postError && <ErrorNotice error={postError} title="Comment not posted" className="mt-2" />}
      </Section>
      {error && <ErrorNotice error={error} title="Could not load activity" onRetry={reload} />}
      {loading && <LoadingState label="Loading activity…" />}
      {!loading && !error && rows.length === 0 && <EmptyState title="No activity recorded." />}
      {rows.length > 0 && (
        <Card padded={false}>
          <ul className="divide-y divide-osint-border">
            {rows.map((ev) => (
              <li key={ev.id} className="px-3 py-2 flex items-start gap-3">
                <Pill tone={CaseActivityKind.tone(ev.kind)} className="mt-0.5 flex-shrink-0">{CaseActivityKind.label(ev.kind)}</Pill>
                <div className="min-w-0 flex-1">
                  {ev.body && <div className={cx('text-sm whitespace-pre-wrap', CaseActivityKind.isSystem(ev.kind) ? 'text-osint-muted' : 'text-osint-text')}>{ev.body}</div>}
                  {ev.target_ref && <div className="text-[11px] text-osint-muted font-mono">→ {ev.target_ref}</div>}
                  {Array.isArray(ev.mentions) && ev.mentions.length > 0 && <div className="text-[11px] text-neon-cyan font-mono">@ {ev.mentions.join(', ')}</div>}
                  <div className="text-[11px] text-osint-muted font-mono mt-0.5">{fmtAbs(ev.ts)}{ev.actor_id ? ` · ${ev.actor_id}` : ''}</div>
                </div>
              </li>
            ))}
          </ul>
        </Card>
      )}
      {hasMore && <div className="flex justify-end"><Button busy={loadingMore} onClick={loadMore}>Load older activity</Button></div>}
    </div>
  );
}

/* ── Members ────────────────────────────────────────────────────────────── */

function MembersPane({ caseId, members, canManage, onChanged }) {
  const [draft, setDraft] = useState(members);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState(null);
  const ws = useApi(canManage ? '/api/members' : null, { deps: [canManage] });
  useEffect(() => setDraft(members), [members]);

  const wsMembers = useMemo(() => {
    const d = ws.data;
    const list = Array.isArray(d?.data) ? d.data : Array.isArray(d?.members) ? d.members : Array.isArray(d) ? d : [];
    return list.filter((m) => m?.user_id && !draft.some((x) => x.user_id === m.user_id));
  }, [ws.data, draft]);

  const dirty = JSON.stringify(draft.map((m) => [m.user_id, m.role])) !== JSON.stringify(members.map((m) => [m.user_id, m.role]));

  const save = async () => {
    setBusy(true); setError(null);
    try {
      await api.put(`/api/cases/${encodeURIComponent(caseId)}/members`, { members: draft.map((m) => ({ user_id: m.user_id, role: m.role || 'contributor' })) });
      toast('Roster saved', { tone: 'accent' });
      onChanged?.();
    } catch (e) { setError(e); }
    finally { setBusy(false); }
  };

  return (
    <div className="space-y-3">
      <Section label={`Roster · ${draft.length}`} padded={false}>
        {draft.length === 0 && <div className="p-3 text-xs text-osint-muted">No explicit roster. Workspace owners and admins can always read and edit; everyone else sees the case according to their workspace role.</div>}
        <ul className="divide-y divide-osint-border">
          {draft.map((m) => (
            <li key={m.user_id} className="px-3 py-2 flex items-center gap-3">
              <div className="min-w-0 flex-1">
                <div className="text-sm text-osint-text truncate">{m.display_name || m.email || m.user_id}</div>
                <div className="text-[11px] text-osint-muted font-mono truncate">{m.email && m.display_name ? `${m.email} · ` : ''}{m.user_id}</div>
              </div>
              {canManage ? (
                <>
                  <Select value={m.role || 'contributor'} onChange={(e) => setDraft((xs) => xs.map((x) => (x.user_id === m.user_id ? { ...x, role: e.target.value } : x)))}>
                    {CASE_ROLES.map((r) => <option key={r} value={r}>{r}</option>)}
                  </Select>
                  <Button size="sm" variant="ghost" onClick={() => setDraft((xs) => xs.filter((x) => x.user_id !== m.user_id))}>Remove</Button>
                </>
              ) : <Pill tone={m.role === 'lead' ? 'accent' : 'neutral'}>{m.role || 'member'}</Pill>}
            </li>
          ))}
        </ul>
      </Section>
      {canManage && (
        <Section label="Add from workspace">
          {ws.error && <ErrorNotice error={ws.error} title="Could not list workspace members" onRetry={ws.reload} />}
          {ws.loading && <LoadingState label="Loading workspace members…" />}
          {!ws.loading && !ws.error && wsMembers.length === 0 && <div className="text-xs text-osint-muted">Every workspace member is already on the roster.</div>}
          {wsMembers.length > 0 && (
            <ul className="divide-y divide-osint-border">
              {wsMembers.map((m) => (
                <li key={m.user_id} className="py-1.5 flex items-center gap-3">
                  <div className="min-w-0 flex-1 text-sm text-osint-text truncate">{m.display_name || m.email || m.user_id}<span className="text-[11px] text-osint-muted font-mono"> · {m.role || ''}</span></div>
                  <Button size="sm" onClick={() => setDraft((xs) => [...xs, { user_id: m.user_id, email: m.email, display_name: m.display_name, role: 'contributor' }])}>Add</Button>
                </li>
              ))}
            </ul>
          )}
        </Section>
      )}
      {error && <ErrorNotice error={error} title="Roster not saved" />}
      {canManage && (
        <div className="flex justify-end gap-2">
          <Button disabled={!dirty} onClick={() => setDraft(members)}>Discard</Button>
          <Button variant="primary" busy={busy} disabled={!dirty} onClick={save}>Save roster</Button>
        </div>
      )}
      {!canManage && <div className="text-[11px] text-osint-muted">Read-only: the roster is set by the case lead or a workspace owner/admin (PUT /api/cases/:id/members).</div>}
    </div>
  );
}

/* ── Edit sheet ─────────────────────────────────────────────────────────── */

function EditCaseSheet({ open, onClose, caseRow, onSaved }) {
  const [name, setName] = useState(caseRow.name || '');
  const [summary, setSummary] = useState(caseRow.summary || '');
  const [status, setStatus] = useState(caseRow.status || 'open');
  const [priority, setPriority] = useState(caseRow.priority ?? 0);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState(null);
  useEffect(() => {
    if (open) { setName(caseRow.name || ''); setSummary(caseRow.summary || ''); setStatus(caseRow.status || 'open'); setPriority(caseRow.priority ?? 0); setError(null); }
  }, [open, caseRow]);

  const save = async () => {
    setBusy(true); setError(null);
    try {
      const body = {};
      if (name.trim() !== (caseRow.name || '')) body.name = name.trim();
      if (summary !== (caseRow.summary || '')) body.summary = summary;
      if (status !== caseRow.status) body.status = status;
      if (Number(priority) !== Number(caseRow.priority)) body.priority = Number(priority);
      if (Object.keys(body).length === 0) { onClose(); return; }
      const r = await api.patch(`/api/cases/${encodeURIComponent(caseRow.id)}`, body);
      toast('Case saved', { tone: 'accent' });
      onSaved(r?.data ?? { ...caseRow, ...body });
    } catch (e) { setError(e); }
    finally { setBusy(false); }
  };

  return (
    <Sheet open={open} onClose={onClose} title="Edit case" footer={(
      <>
        <Button onClick={onClose}>Cancel</Button>
        <Button variant="primary" busy={busy} disabled={!name.trim()} onClick={save}>Save</Button>
      </>
    )}>
      <div className="space-y-3">
        <Field label="Name"><Input value={name} maxLength={200} onChange={(e) => setName(e.target.value)} /></Field>
        <Field label="Summary" hint="Rendered as the executive summary of the exported report."><TextArea value={summary} onChange={(e) => setSummary(e.target.value)} /></Field>
        <div className="grid grid-cols-2 gap-3">
          <Field label="Status" hint="Closing stamps closed_at server-side; reopening clears it.">
            <Select value={status} onChange={(e) => setStatus(e.target.value)} className="w-full">
              {CASE_STATUSES.map((s) => <option key={s} value={s}>{CaseStatus.label(s)}</option>)}
            </Select>
          </Field>
          <Field label="Priority">
            <Select value={priority} onChange={(e) => setPriority(Number(e.target.value))} className="w-full">
              {CASE_PRIORITIES.map((p) => <option key={p} value={p}>{p} · {CasePriority.label(p)}</option>)}
            </Select>
          </Field>
        </div>
        {error && <ErrorNotice error={error} title="Save failed" />}
      </div>
    </Sheet>
  );
}

/* ── Report sheet (POST /api/cases/:id/report, streamed md/html) ───────── */

const REPORT_SECTIONS = ['header', 'summary', 'findings', 'entities', 'citations', 'audit'];

function ReportSheet({ open, onClose, caseRow }) {
  const [format, setFormat] = useState('md');
  const [sections, setSections] = useState(REPORT_SECTIONS);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState(null);
  const [result, setResult] = useState(null); // { text, filename, contentType }

  const run = async () => {
    setBusy(true); setError(null); setResult(null);
    try {
      const res = await api.raw(`/api/cases/${encodeURIComponent(caseRow.id)}/report`, {
        method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ format, sections }),
      });
      if (!res.ok) {
        let body = null; try { body = await res.json(); } catch { /* not json */ }
        throw new Error(body?.error ? `HTTP ${res.status}: ${body.error}` : `HTTP ${res.status}`);
      }
      const text = await res.text();
      setResult({
        text,
        contentType: res.headers.get('content-type') || (format === 'html' ? 'text/html' : 'text/markdown'),
        filename: filenameFromDisposition(res.headers.get('content-disposition'), `case-${caseRow.id}.${format}`),
      });
    } catch (e) { setError(e); }
    finally { setBusy(false); }
  };

  const toggle = (s) => setSections((xs) => (xs.includes(s) ? xs.filter((x) => x !== s) : [...xs, s]));

  return (
    <Sheet open={open} onClose={onClose} title={`Report — ${caseRow.name}`} width="max-w-3xl" footer={(
      <>
        <Button onClick={onClose}>Close</Button>
        {result && <CopyButton text={result.text} label="Copy" size="md" />}
        {result && <Button onClick={() => downloadBlob(new Blob([result.text], { type: result.contentType }), result.filename)}>Download {result.filename}</Button>}
        <Button variant="primary" busy={busy} disabled={sections.length === 0} onClick={run}>{result ? 'Regenerate' : 'Generate'}</Button>
      </>
    )}>
      <div className="space-y-3">
        <div className="flex flex-wrap items-center gap-3">
          <Field label="Format"><Segmented value={format} onChange={setFormat} options={[{ value: 'md', label: 'Markdown' }, { value: 'html', label: 'HTML' }]} /></Field>
          <Field label="Sections" hint="A section you leave out is omitted; the classification banner and provenance footer are always emitted.">
            <div className="flex flex-wrap gap-1.5">
              {REPORT_SECTIONS.map((s) => (
                <button key={s} type="button" onClick={() => toggle(s)}
                  className={cx('px-2 py-0.5 rounded-full border text-[11px] font-mono', sections.includes(s) ? 'border-accent/40 text-accent bg-accent/10' : 'border-osint-border text-osint-muted')}>
                  {s}
                </button>
              ))}
            </div>
          </Field>
        </div>
        {error && <ErrorNotice error={error} title="Report failed" />}
        {busy && <LoadingState label="Streaming the report from the server…" />}
        {result && (
          format === 'html'
            ? <iframe title="report" sandbox="" srcDoc={result.text} className="w-full h-[60vh] rounded-md border border-osint-border bg-white" />
            : <pre className="text-xs font-mono text-osint-text whitespace-pre-wrap bg-osint-bg border border-osint-border rounded-md p-3 max-h-[60vh] overflow-auto">{result.text}</pre>
        )}
        {result && <div className="text-[11px] text-osint-muted font-mono">{result.text.length.toLocaleString()} characters · {result.contentType}</div>}
      </div>
    </Sheet>
  );
}

/* ── Delete (typed confirmation, like the iOS sheet) ───────────────────── */

function DeleteCaseSheet({ open, onClose, caseRow, onConfirm }) {
  const [typed, setTyped] = useState('');
  const [busy, setBusy] = useState(false);
  useEffect(() => { if (open) setTyped(''); }, [open]);
  const ok = typed.trim() === (caseRow.name || '').trim() || typed.trim() === 'DELETE';
  return (
    <Sheet open={open} onClose={onClose} title="Delete case" width="max-w-md" footer={(
      <>
        <Button onClick={onClose}>Cancel</Button>
        <Button variant="danger" busy={busy} disabled={!ok} onClick={async () => { setBusy(true); try { await onConfirm(); } finally { setBusy(false); } }}>Delete permanently</Button>
      </>
    )}>
      <div className="space-y-3 text-sm">
        <div className="text-neon-red font-semibold">This cannot be undone</div>
        <div className="text-osint-muted">
          Deleting “{caseRow.name}” permanently removes its pinned references ({caseRow.item_count ?? 0}), notes, activity trail and roster.
          The intel items, entities and breach records themselves are not deleted — only this case's record of citing them.
        </div>
        <Field label={`Type “${caseRow.name}” — or the word DELETE — to enable the button.`}>
          <Input value={typed} onChange={(e) => setTyped(e.target.value)} autoFocus />
        </Field>
      </div>
    </Sheet>
  );
}
