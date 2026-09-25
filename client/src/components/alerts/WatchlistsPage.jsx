import React, { useEffect, useState } from 'react';
import { Link } from 'react-router-dom';
import { LuRefreshCw, LuEye, LuTrash2, LuPencil, LuX } from 'react-icons/lu';
import { api, errorMessage } from '../../api/client.js';
import { useEntitySearch } from '../../hooks/useSearch.js';
import { entityVisual } from '../../utils/entityVisuals.js';
import { relativeTime, fmtAbs } from '../../utils/time.js';
import {
  Page, Card, Pill, Button, Input, Field, Toggle, Sheet, ConfirmDialog, ErrorNotice, EmptyState, LoadingState,
  BoundNote, CopyButton, toast, cx,
} from '../ui/kit.jsx';
import { isMuted, muteLabel } from './alertShared.jsx';

/**
 * Watchlists — port of iOS `WatchlistsView`. A watchlist is sugar over an
 * alert rule whose predicate is `{entity_ids:[…]}`.
 *   GET  /api/watchlists?limit&cursor → {data:[{id,name,entity_ids,rule_id,enabled,muted_until,created_at,updated_at}], page}
 *   POST /api/watchlists {name, entity_ids}   (ids must exist; empty list refused)
 *   PATCH /api/watchlists/:id {name?, entity_ids?, enabled?}
 *   DELETE /api/watchlists/:id  — also deletes the rule and its history
 */
export default function WatchlistsPage() {
  const [rows, setRows] = useState([]);
  const [page, setPage] = useState(null);
  const [error, setError] = useState(null);
  const [loading, setLoading] = useState(true);
  const [editing, setEditing] = useState(null);   // 'new' | row
  const [confirmDel, setConfirmDel] = useState(null);
  const [busy, setBusy] = useState(null);

  const load = async (cursor) => {
    setLoading(true); setError(null);
    try {
      const j = await api.get('/api/watchlists', { query: { limit: 100, cursor } });
      const d = Array.isArray(j?.data) ? j.data : [];
      setRows((xs) => (cursor ? [...xs, ...d] : d));
      setPage(j?.page || null);
    } catch (e) { setError(e); }
    finally { setLoading(false); }
  };
  useEffect(() => { load(); }, []);

  const toggle = async (w) => {
    setBusy(w.id);
    try {
      const r = await api.patch(`/api/watchlists/${encodeURIComponent(w.id)}`, { enabled: !w.enabled });
      const row = r?.data ?? { ...w, enabled: !w.enabled };
      setRows((xs) => xs.map((x) => (x.id === w.id ? { ...x, ...row } : x)));
    } catch (e) { toast(errorMessage(e), { tone: 'danger', ttl: 6000 }); }
    finally { setBusy(null); }
  };

  const del = async () => {
    const w = confirmDel; setConfirmDel(null); setBusy(w.id);
    try {
      await api.del(`/api/watchlists/${encodeURIComponent(w.id)}`);
      toast('Watchlist deleted', { tone: 'accent' });
      setRows((xs) => xs.filter((x) => x.id !== w.id));
    } catch (e) { toast(errorMessage(e), { tone: 'danger', ttl: 6000 }); }
    finally { setBusy(null); }
  };

  return (
    <Page
      title="Watchlists"
      subtitle="Entities you are following. Any new item that mentions one of them fires the watchlist's alert rule."
      actions={(
        <>
          <Button onClick={() => load()} title="Refresh watchlists"><LuRefreshCw size={13} /></Button>
          <Button variant="primary" onClick={() => setEditing('new')}><LuEye size={13} /> New watchlist</Button>
        </>
      )}
    >
      {error && <ErrorNotice error={error} title="Could not load watchlists" onRetry={() => load()} />}
      {loading && rows.length === 0 && !error && <LoadingState label="Loading watchlists…" />}
      {!loading && !error && rows.length === 0 && (
        <EmptyState title="No watchlists" action={<Button variant="primary" onClick={() => setEditing('new')}>Create a watchlist</Button>}>
          Pick entities from the graph — people, organisations, domains, IPs — and get an inbox event whenever they show up again.
        </EmptyState>
      )}

      {rows.length > 0 && (
        <div className="space-y-1.5">
          {rows.map((w) => {
            const ids = Array.isArray(w.entity_ids) ? w.entity_ids : [];
            return (
              <Card key={w.id} padded={false} className={cx(w.enabled === false && 'opacity-70')}>
                <div className="p-3 flex items-start gap-3 flex-wrap">
                  <div className="min-w-0 flex-1">
                    <div className="flex items-center gap-2 flex-wrap">
                      <span className="text-sm font-medium text-osint-text truncate">{w.name}</span>
                      <Pill tone="accent">watching {ids.length}</Pill>
                      {w.enabled === false && <Pill>disabled</Pill>}
                      {w.enabled == null && <Pill tone="warning" title="The rule behind this watchlist is missing">no rule</Pill>}
                      {isMuted(w.muted_until) && <Pill tone="warning">{muteLabel(w.muted_until)}</Pill>}
                    </div>
                    <div className="flex flex-wrap gap-1 mt-1.5">
                      {ids.slice(0, 12).map((id) => <EntityChip key={id} id={id} />)}
                      {ids.length > 12 && <Pill>+{ids.length - 12} more</Pill>}
                    </div>
                    <div className="text-[11px] text-osint-muted mt-1" title={fmtAbs(w.created_at)}>
                      created {relativeTime(w.created_at)}{w.rule_id && <> · rule <span className="font-mono">{w.rule_id}</span></>}
                    </div>
                  </div>
                  <div className="flex items-center gap-1 flex-wrap">
                    {w.enabled != null && <Toggle on={Boolean(w.enabled)} onChange={() => toggle(w)} disabled={busy === w.id} />}
                    <Button size="sm" onClick={() => setEditing(w)} title="Edit"><LuPencil size={12} /></Button>
                    <CopyButton text={w.id} label="Copy id" />
                    {w.rule_id && <Link to="/console/alerts"><Button size="sm" title={w.rule_id}>Rule</Button></Link>}
                    <Button size="sm" variant="ghost" busy={busy === w.id} onClick={() => setConfirmDel(w)} title="Delete"><LuTrash2 size={12} /></Button>
                  </div>
                </div>
              </Card>
            );
          })}
          <div className="flex items-center justify-between">
            <BoundNote shown={rows.length} total={page?.next_cursor ? rows.length + 1 : rows.length} noun="watchlists" />
            {page?.next_cursor && <Button size="sm" busy={loading} onClick={() => load(page.next_cursor)}>Load more</Button>}
          </div>
        </div>
      )}

      <WatchlistEditor
        open={editing != null}
        watchlist={editing === 'new' ? null : editing}
        onClose={() => setEditing(null)}
        onSaved={(row, isNew) => { setEditing(null); setRows((xs) => (isNew ? [row, ...xs] : xs.map((x) => (x.id === row.id ? { ...x, ...row } : x)))); }}
      />

      <ConfirmDialog
        open={confirmDel != null}
        onClose={() => setConfirmDel(null)}
        onConfirm={del}
        title={`Delete “${confirmDel?.name || ''}”?`}
        confirmLabel="Delete"
        message="Deleting a watchlist also deletes the alert rule behind it and that rule's firing history."
      />
    </Page>
  );
}

/** Entity ids are opaque; show the id itself, linking to the entity lookup. */
function EntityChip({ id, label, type, onRemove }) {
  const v = type ? entityVisual(type) : null;
  return (
    <span className={cx('inline-flex items-center gap-1 px-1.5 py-0.5 rounded border text-[11px] font-mono', v ? v.color : 'border-osint-border text-osint-muted')}>
      {v && <span>{v.label}</span>}
      <span className="truncate max-w-[180px]" title={id}>{label || id}</span>
      {onRemove && <button type="button" onClick={onRemove} className="hover:text-neon-red" title={`Remove ${label || id}`}><LuX size={10} /></button>}
    </span>
  );
}

function WatchlistEditor({ open, watchlist, onClose, onSaved }) {
  const [name, setName] = useState('');
  const [selected, setSelected] = useState([]);   // [{id, label, type}]
  const [q, setQ] = useState('');
  const { results, loading: searching, error: searchError } = useEntitySearch(q);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState(null);

  useEffect(() => {
    if (!open) return;
    setName(watchlist?.name || '');
    setSelected((watchlist?.entity_ids || []).map((id) => ({ id })));
    setQ(''); setError(null);
  }, [open, watchlist]);

  const add = (e) => {
    if (selected.some((s) => s.id === e.entity_id)) return;
    setSelected((xs) => [...xs, { id: e.entity_id, label: e.value, type: e.type }]);
  };
  const remove = (id) => setSelected((xs) => xs.filter((s) => s.id !== id));

  const save = async () => {
    setError(null);
    if (!name.trim()) { setError(new Error('Name is required.')); return; }
    if (selected.length === 0) { setError(new Error('Add at least one entity — an empty watchlist would match everything.')); return; }
    setSaving(true);
    try {
      const ids = selected.map((s) => s.id);
      let r;
      if (watchlist) r = await api.patch(`/api/watchlists/${encodeURIComponent(watchlist.id)}`, { name: name.trim(), entity_ids: ids });
      else r = await api.post('/api/watchlists', { name: name.trim(), entity_ids: ids });
      toast(watchlist ? 'Watchlist updated' : 'Watchlist created', { tone: 'accent' });
      onSaved(r?.data ?? { ...(watchlist || {}), name: name.trim(), entity_ids: ids }, !watchlist);
    } catch (e) { setError(e); }
    finally { setSaving(false); }
  };

  return (
    <Sheet open={open} onClose={onClose} title={watchlist ? 'Edit watchlist' : 'New watchlist'} width="max-w-lg"
      footer={<><Button onClick={onClose}>Cancel</Button><Button variant="primary" busy={saving} onClick={save}>{watchlist ? 'Save' : 'Create'}</Button></>}
    >
      <div className="space-y-3">
        <Field label="Name"><Input value={name} onChange={(e) => setName(e.target.value)} placeholder="Name" /></Field>
        <div>
          <div className="text-[11px] font-medium text-osint-muted mb-1">Watching <span className="font-mono">{selected.length}</span></div>
          {selected.length === 0 ? <div className="text-xs text-osint-muted">No entities yet.</div> : (
            <div className="flex flex-wrap gap-1">{selected.map((s) => <EntityChip key={s.id} id={s.id} label={s.label} type={s.type} onRemove={() => remove(s.id)} />)}</div>
          )}
        </div>
        <Field label="Add entities">
          <Input value={q} onChange={(e) => setQ(e.target.value)} placeholder="Search entities (name, domain, IP…)" />
        </Field>
        {searching && <div className="text-xs text-osint-muted">Searching…</div>}
        {searchError && <ErrorNotice error={{ message: searchError }} title="Entity search failed" />}
        {!searching && q && results.length === 0 && !searchError && <div className="text-xs text-osint-muted">No matching entities.</div>}
        {results.length > 0 && (
          <ul className="divide-y divide-osint-border rounded-md border border-osint-border max-h-56 overflow-auto">
            {results.map((e) => {
              const v = entityVisual(e.type);
              const on = selected.some((s) => s.id === e.entity_id);
              return (
                <li key={e.entity_id}>
                  <button type="button" disabled={on} onClick={() => add(e)} className="w-full flex items-center gap-2 px-2 py-1.5 text-left hover:bg-white/5 disabled:opacity-40">
                    <span className={cx('px-1.5 py-0.5 rounded border text-[10px]', v.color)}>{v.label}</span>
                    <span className="text-sm text-osint-text flex-1 truncate">{e.value}</span>
                    <span className="text-[11px] text-osint-muted font-mono">{e.mention_count} mentions</span>
                  </button>
                </li>
              );
            })}
          </ul>
        )}
        {error && <ErrorNotice error={error} title="Could not save watchlist" />}
      </div>
    </Sheet>
  );
}
