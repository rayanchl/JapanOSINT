import React, { useState } from 'react';
import { api } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { useAuth } from '../../auth/AuthContext.jsx';
import { Section, Button, TextArea, ErrorNotice, LoadingState, ConfirmDialog, BoundNote, cx, toast } from '../ui/kit.jsx';
import { relativeTime } from '../../utils/time.js';

/**
 * Analyst notes on any reference — the iOS `AnnotationsSection`.
 *   GET    /api/annotations?ref_type&ref_id[&case_id]&limit&cursor
 *   POST   /api/annotations          { ref_type, ref_id, body_md, case_id? }
 *   PATCH  /api/annotations/:id      { body_md }
 *   DELETE /api/annotations/:id      → 204 (tombstone)
 * `body_md` is stored verbatim and rendered here as text, never as HTML.
 */
export default function AnnotationsSection({ refType, refId, caseId, title = 'Notes' }) {
  const auth = useAuth();
  const query = new URLSearchParams({ ref_type: refType, ref_id: String(refId), limit: '50' });
  if (caseId) query.set('case_id', caseId);
  const path = `/api/annotations?${query.toString()}`;
  const { data, error, loading, reload, setData } = useApi(path, { deps: [path] });
  const rows = Array.isArray(data?.data) ? data.data : [];
  const [draft, setDraft] = useState('');
  const [busy, setBusy] = useState(false);
  const [postError, setPostError] = useState(null);
  const [editing, setEditing] = useState(null);
  const [editText, setEditText] = useState('');
  const [confirmDel, setConfirmDel] = useState(null);
  const [more, setMore] = useState(false);

  const create = async () => {
    const body_md = draft.trim();
    if (!body_md) return;
    setBusy(true); setPostError(null);
    try {
      const b = { ref_type: refType, ref_id: String(refId), body_md };
      if (caseId) b.case_id = caseId;
      await api.post('/api/annotations', b);
      setDraft('');
      await reload({ silent: true });
      toast('Note added', { tone: 'accent' });
    } catch (e) { setPostError(e); }
    finally { setBusy(false); }
  };

  const saveEdit = async () => {
    if (!editing) return;
    setBusy(true); setPostError(null);
    try {
      await api.patch(`/api/annotations/${encodeURIComponent(editing)}`, { body_md: editText.trim() });
      setEditing(null);
      await reload({ silent: true });
    } catch (e) { setPostError(e); }
    finally { setBusy(false); }
  };

  const remove = async () => {
    const id = confirmDel;
    setBusy(true); setPostError(null);
    try {
      await api.del(`/api/annotations/${encodeURIComponent(id)}`);
      setConfirmDel(null);
      await reload({ silent: true });
    } catch (e) { setPostError(e); setConfirmDel(null); }
    finally { setBusy(false); }
  };

  const loadMore = async () => {
    const cur = data?.page?.next_cursor;
    if (!cur) return;
    setMore(true);
    try {
      const j = await api.get(`${path}&cursor=${encodeURIComponent(cur)}`);
      setData({ ...j, data: [...rows, ...(Array.isArray(j?.data) ? j.data : [])] });
    } catch (e) { setPostError(e); }
    finally { setMore(false); }
  };

  return (
    <Section label={title} right={<span className="font-mono text-[10px] text-osint-muted">{rows.length}{data?.page?.next_cursor ? '+' : ''}</span>}>
      {loading && <LoadingState label="Loading notes…" />}
      {error && <ErrorNotice error={error} title="Could not load notes" onRetry={reload} />}
      {postError && <ErrorNotice error={postError} title="Note request failed" />}
      {!loading && !error && rows.length === 0 && <div className="text-xs text-osint-muted py-1">No notes yet on this {refType.replace('_', ' ')}.</div>}
      <ul className="space-y-2">
        {rows.map((n) => {
          const mine = auth.me?.user?.id && n.author_id === auth.me.user.id;
          return (
            <li key={n.id} className={cx('rounded-md border border-osint-border bg-osint-bg/60 p-2', n.is_deleted && 'opacity-60')}>
              <div className="flex items-center justify-between gap-2 text-[10px] font-mono text-osint-muted">
                <span title={n.created_at}>{mine ? 'you' : (n.author_id || 'unknown author')} · {relativeTime(n.created_at)}{n.updated_at ? ' · edited' : ''}{n.is_deleted ? ' · deleted' : ''}{n.case_id ? ` · case ${n.case_id}` : ''}</span>
                {!n.is_deleted && (
                  <span className="flex gap-1">
                    <Button size="sm" variant="ghost" onClick={() => { setEditing(n.id); setEditText(n.body_md || ''); }}>Edit</Button>
                    <Button size="sm" variant="ghost" onClick={() => setConfirmDel(n.id)}>Delete</Button>
                  </span>
                )}
              </div>
              {editing === n.id ? (
                <div className="mt-1 space-y-1">
                  <TextArea value={editText} onChange={(e) => setEditText(e.target.value)} />
                  <div className="flex justify-end gap-1">
                    <Button size="sm" onClick={() => setEditing(null)}>Cancel</Button>
                    <Button size="sm" variant="primary" busy={busy} disabled={!editText.trim()} onClick={saveEdit}>Save</Button>
                  </div>
                </div>
              ) : (
                <div className="text-sm text-osint-text whitespace-pre-wrap break-words mt-1">{n.body_md}</div>
              )}
            </li>
          );
        })}
      </ul>
      {data?.page?.next_cursor && (
        <div className="flex items-center justify-between mt-2">
          <BoundNote shown={rows.length} total={rows.length + 1} noun="notes (more available)" />
          <Button size="sm" busy={more} onClick={loadMore}>Load more</Button>
        </div>
      )}
      <div className="mt-3 space-y-1">
        <TextArea placeholder="Add a note (Markdown is stored verbatim, shown as text)…" value={draft} onChange={(e) => setDraft(e.target.value)} />
        <div className="flex justify-end">
          <Button variant="primary" size="sm" busy={busy} disabled={!draft.trim()} onClick={create}>Add note</Button>
        </div>
      </div>
      <ConfirmDialog open={Boolean(confirmDel)} onClose={() => setConfirmDel(null)} onConfirm={remove} busy={busy}
        title="Delete note?" confirmLabel="Delete" message="The note is tombstoned server-side (hidden, not erased)." />
    </Section>
  );
}
