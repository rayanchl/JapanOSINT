import React, { useEffect, useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { LuRefreshCw, LuPlay, LuPencil, LuTrash2, LuBellRing, LuPin, LuPinOff, LuHistory, LuEraser } from 'react-icons/lu';
import { api, errorMessage, ApiError } from '../../api/client.js';
import { usePagedList } from '../../hooks/usePagedList.js';
import { useMemberNames, authorLabel } from '../../hooks/useMembers.js';
import { relativeTime, fmtAbs } from '../../utils/time.js';
import {
  Page, Section, Card, Pill, Button, Input, Field, Segmented, Sheet, ConfirmDialog, ErrorNotice, EmptyState,
  LoadingState, PagedFooter, KV, toast, cx,
} from '../ui/kit.jsx';
import { ChannelsEditor, validateChannels, emptyChannel } from './alertShared.jsx';

/**
 * Saved searches + search history — port of iOS `SavedSearchesView` and
 * `SearchHistoryView`. Both are SHARED with the workspace: every member sees
 * every member's entries (each row carries `user_id` and `mine`). Only the
 * author can pin, rename or delete a saved search, or clear their own history.
 *   GET   /api/saved-searches?kind&pinned&mine&limit&offset → {data:[{id,name,kind,params,pinned,created_at,last_run_at,run_count,user_id,mine}],
 *           page:{limit,offset,count,total,has_more}, meta:{scope}} — total is measured under the same filters
 *   POST  /api/saved-searches {name?, kind, params, pinned?}
 *   PATCH /api/saved-searches/:id {name?, params?, pinned?}   (kind immutable)
 *   DELETE /api/saved-searches/:id
 *   POST  /api/saved-searches/:id/run → bookkeeping only (meta.executed=false); the client re-issues the query
 *   POST  /api/saved-searches/:id/to-alert {channels, dedup_window_sec?, storm_cap_per_hour?, enabled?} — intel kind only
 *   GET   /api/search-history?kind&mine&limit&offset → {data:[{id,kind,params,result_count,ts,user_id,mine}], page:{…,total,has_more}} · DELETE clears your own
 */
const KINDS = ['all', 'intel', 'osint', 'entity', 'breach', 'map'];
/** ?mine=1 narrows both lists (and their totals) to the caller's own entries. */
const WHOSE = [{ value: 'all', label: 'everyone' }, { value: 'mine', label: 'mine' }];

/** Where a saved search of `kind` executes in this client. */
export function routeFor(kind, params) {
  const p = params && typeof params === 'object' ? params : {};
  const q = p.q ?? p.query ?? p.text ?? '';
  const enc = (v) => encodeURIComponent(String(v));
  switch (kind) {
    case 'intel': {
      const qs = new URLSearchParams();
      for (const [k, v] of Object.entries(p)) if (v != null && v !== '') qs.set(k, String(v));
      return `/intel${qs.toString() ? `?${qs}` : ''}`;
    }
    case 'osint': return `/search${q ? `?q=${enc(q)}` : ''}`;
    case 'entity': return `/entities${q ? `?q=${enc(q)}` : ''}`;
    case 'breach': return `/intel?source=breach${q ? `&q=${enc(q)}` : ''}`;
    case 'map': {
      const qs = new URLSearchParams();
      for (const [k, v] of Object.entries(p)) if (v != null && v !== '') qs.set(k, typeof v === 'object' ? JSON.stringify(v) : String(v));
      return `/${qs.toString() ? `?${qs}` : ''}`;
    }
    default: return '/search';
  }
}

function paramsSummary(params) {
  if (!params || typeof params !== 'object') return '';
  return Object.entries(params).filter(([, v]) => v != null && v !== '').map(([k, v]) => `${k}=${typeof v === 'object' ? JSON.stringify(v) : v}`).join(' · ');
}

export default function SavedSearchesPage() {
  const navigate = useNavigate();
  const [kind, setKind] = useState('all');
  const [whose, setWhose] = useState('all');
  const qs = new URLSearchParams();
  if (kind !== 'all') qs.set('kind', kind);
  if (whose === 'mine') qs.set('mine', '1');
  const path = `/api/saved-searches${qs.toString() ? `?${qs}` : ''}`;
  const list = usePagedList(path, { pageSize: 50 });
  const { rows, error, loading, loaded, reload } = list;
  const names = useMemberNames();
  const [renaming, setRenaming] = useState(null);
  const [toAlert, setToAlert] = useState(null);
  const [confirmDel, setConfirmDel] = useState(null);
  const [busy, setBusy] = useState(null);

  const run = async (s) => {
    setBusy(s.id);
    try {
      await api.post(`/api/saved-searches/${encodeURIComponent(s.id)}/run`);
    } catch (e) {
      // Bookkeeping failure must not block the search itself; say so.
      toast(`Run not recorded: ${errorMessage(e)}`, { tone: 'warning', ttl: 5000 });
    } finally { setBusy(null); }
    navigate(routeFor(s.kind, s.params));
  };

  const pin = async (s) => {
    setBusy(s.id);
    try { await api.patch(`/api/saved-searches/${encodeURIComponent(s.id)}`, { pinned: !s.pinned }); await reload({ silent: true }); }
    catch (e) { toast(errorMessage(e), { tone: 'danger', ttl: 6000 }); }
    finally { setBusy(null); }
  };

  const del = async () => {
    const s = confirmDel; setConfirmDel(null); setBusy(s.id);
    try { await api.del(`/api/saved-searches/${encodeURIComponent(s.id)}`); toast('Saved search deleted', { tone: 'accent' }); await reload({ silent: true }); }
    catch (e) { toast(errorMessage(e), { tone: 'danger', ttl: 6000 }); }
    finally { setBusy(null); }
  };

  return (
    <Page
      title="Saved searches"
      subtitle="Re-run a search, pin the ones you use daily, or turn an intel search into an alert rule. Shared with everyone in this workspace; only the author can pin, rename or delete."
      actions={(
        <>
          <Segmented value={kind} onChange={setKind} options={KINDS.map((k) => ({ value: k, label: k }))} />
          <Segmented value={whose} onChange={setWhose} options={WHOSE} />
          <Button onClick={() => reload()} title="Reload"><LuRefreshCw size={13} /></Button>
        </>
      )}
    >
      {error && <ErrorNotice error={error} title="Could not load saved searches" onRetry={reload} />}
      {loading && !loaded && <LoadingState label="Loading saved searches…" />}
      {!loading && !error && rows.length === 0 && (
        <EmptyState title="No saved searches">Save a search from the Search, Intel or Entities tabs and it appears here.</EmptyState>
      )}

      {rows.length > 0 && (
        <div className="space-y-1.5">
          {rows.map((s) => (
            <Card key={s.id} padded={false}>
              <div className="p-3 flex items-start gap-3 flex-wrap">
                <div className="min-w-0 flex-1">
                  <div className="flex items-center gap-2 flex-wrap">
                    {s.pinned && <LuPin size={12} className="text-accent" />}
                    <span className={cx('text-sm truncate', s.name ? 'text-osint-text font-medium' : 'text-osint-muted italic')}>{s.name || 'unnamed bookmark'}</span>
                    <Pill tone="cyan">{s.kind}</Pill>
                    <Pill title="run count">{s.run_count ?? 0} run{s.run_count === 1 ? '' : 's'}</Pill>
                  </div>
                  <div className="text-[11px] text-osint-muted font-mono mt-0.5 break-words">{paramsSummary(s.params) || 'no parameters'}</div>
                  <div className="text-[11px] text-osint-muted mt-0.5">
                    saved by {authorLabel(s, names)} {relativeTime(s.created_at)}{s.last_run_at && <span title={fmtAbs(s.last_run_at)}> · last run {relativeTime(s.last_run_at)}</span>}
                  </div>
                </div>
                <div className="flex items-center gap-1 flex-wrap">
                  <Button size="sm" variant="primary" busy={busy === s.id} onClick={() => run(s)} title="Run"><LuPlay size={12} /> Run</Button>
                  {s.mine !== false && <Button size="sm" onClick={() => pin(s)} title={s.pinned ? 'Unpin' : 'Pin'}>{s.pinned ? <LuPinOff size={12} /> : <LuPin size={12} />}</Button>}
                  {s.mine !== false && <Button size="sm" onClick={() => setRenaming(s)} title="Rename"><LuPencil size={12} /></Button>}
                  <Button size="sm" onClick={() => setToAlert(s)} disabled={s.kind !== 'intel'} title={s.kind === 'intel' ? 'Turn into alert' : `Only intel searches can become alerts (this one is ${s.kind})`}><LuBellRing size={12} /> Alert</Button>
                  {s.mine !== false && <Button size="sm" variant="ghost" onClick={() => setConfirmDel(s)} title="Delete"><LuTrash2 size={12} /></Button>}
                </div>
              </div>
            </Card>
          ))}
          <PagedFooter shown={rows.length} total={list.total} hasMore={list.hasMore} busy={list.loadingMore} onMore={list.loadMore} error={list.moreError} noun="saved searches" />
        </div>
      )}

      <HistorySection mine={whose === 'mine'} onRun={(h) => navigate(routeFor(h.kind, h.params))} />

      <RenameSheet search={renaming} onClose={() => setRenaming(null)} onSaved={() => { setRenaming(null); reload({ silent: true }); }} />
      <ToAlertSheet search={toAlert} onClose={() => setToAlert(null)} onCreated={() => { setToAlert(null); }} />

      <ConfirmDialog
        open={confirmDel != null}
        onClose={() => setConfirmDel(null)}
        onConfirm={del}
        title="Delete this saved search?"
        confirmLabel="Delete"
        message="Alert rules already created from it are not affected."
      />
    </Page>
  );
}

function RenameSheet({ search, onClose, onSaved }) {
  const [name, setName] = useState('');
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState(null);
  useEffect(() => { if (search) { setName(search.name || ''); setError(null); } }, [search]);
  const save = async () => {
    setSaving(true); setError(null);
    try { await api.patch(`/api/saved-searches/${encodeURIComponent(search.id)}`, { name: name.trim() }); toast('Renamed', { tone: 'accent' }); onSaved(); }
    catch (e) { setError(e); }
    finally { setSaving(false); }
  };
  return (
    <Sheet open={search != null} onClose={onClose} title="Rename saved search" width="max-w-sm"
      footer={<><Button onClick={onClose}>Cancel</Button><Button variant="primary" busy={saving} onClick={save}>Save</Button></>}
    >
      <Field label="Name" hint="Leave the name empty to make this an unnamed bookmark."><Input value={name} onChange={(e) => setName(e.target.value)} /></Field>
      {error && <ErrorNotice error={error} title="Rename failed" className="mt-2" />}
    </Sheet>
  );
}

function ToAlertSheet({ search, onClose, onCreated }) {
  const [name, setName] = useState('');
  const [channels, setChannels] = useState([emptyChannel('email')]);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState(null);
  const [why, setWhy] = useState(null);
  useEffect(() => { if (search) { setName(''); setChannels([emptyChannel('email')]); setError(null); setWhy(null); } }, [search]);

  const create = async () => {
    setError(null); setWhy(null);
    const ce = validateChannels(channels);
    if (ce) { setError(new Error(ce)); return; }
    setSaving(true);
    try {
      const body = { channels };
      if (name.trim()) body.name = name.trim();
      await api.post(`/api/saved-searches/${encodeURIComponent(search.id)}/to-alert`, body);
      toast('Alert rule created', { tone: 'accent' });
      onCreated();
    } catch (e) {
      if (e instanceof ApiError && e.status === 400) setWhy(e.detail);
      setError(e);
    } finally { setSaving(false); }
  };

  return (
    <Sheet open={search != null} onClose={onClose} title="Turn into alert" width="max-w-lg"
      footer={<><Button onClick={onClose}>Cancel</Button><Button variant="primary" busy={saving} onClick={create}>Create</Button></>}
    >
      {search && (
        <div className="space-y-3">
          <Section label="What gets saved">
            <KV pairs={[['search', search.name || search.id], ['kind', search.kind], ['params', paramsSummary(search.params) || '—']]} />
            <div className="text-[11px] text-osint-muted mt-1">The FTS query and source filter become the rule predicate. Other parameters are dropped, and the server says which.</div>
          </Section>
          <Section label="Rule">
            <Field label="Rule name" hint="Defaults to the saved search's own name."><Input value={name} onChange={(e) => setName(e.target.value)} placeholder={search.name || 'Saved search alert'} /></Field>
          </Section>
          <Section label="Deliver to">
            <ChannelsEditor channels={channels} onChange={setChannels} />
          </Section>
          {why && <div className="rounded-md border border-accent/40 bg-accent/10 px-3 py-2 text-xs text-accent">Why can't this be an alert? {why}</div>}
          {error && !why && <ErrorNotice error={error} title="Could not create the rule" />}
        </div>
      )}
    </Sheet>
  );
}

function HistorySection({ onRun, mine = false }) {
  const list = usePagedList(`/api/search-history${mine ? '?mine=1' : ''}`, { pageSize: 50 });
  const { rows, error, loading, loaded, reload, meta } = list;
  const names = useMemberNames();
  const [confirmClear, setConfirmClear] = useState(false);
  const [busy, setBusy] = useState(false);
  const clear = async () => {
    setConfirmClear(false); setBusy(true);
    try { await api.del('/api/search-history'); toast('History cleared', { tone: 'accent' }); await reload({ silent: true }); }
    catch (e) { toast(errorMessage(e), { tone: 'danger', ttl: 6000 }); }
    finally { setBusy(false); }
  };
  return (
    <Section
      label={mine ? 'Recent · yours' : 'Recent · this workspace'}
      right={(
        <div className="flex gap-1">
          <Button size="sm" variant="ghost" onClick={() => reload()} title="Reload"><LuRefreshCw size={11} /></Button>
          <Button size="sm" variant="ghost" busy={busy} disabled={rows.length === 0} onClick={() => setConfirmClear(true)} title="Clear history"><LuEraser size={11} /> Clear</Button>
        </div>
      )}
      padded={false}
    >
      <div className="px-3 pt-2 text-[11px] text-osint-muted flex items-center gap-1"><LuHistory size={11} /> Searches run in this workspace are recorded here, and every member can read this list. Clear removes only your own.</div>
      {error && <div className="p-3"><ErrorNotice error={error} title="Could not load history" onRetry={reload} /></div>}
      {loading && !loaded && <LoadingState label="Loading history…" />}
      {!loading && !error && rows.length === 0 && <div className="p-3 text-xs text-osint-muted">No recent searches.</div>}
      {rows.length > 0 && (
        <ul className="divide-y divide-osint-border">
          {rows.map((h) => (
            <li key={h.id} className="px-3 py-2 flex items-center gap-2 text-xs">
              <Pill tone="cyan">{h.kind}</Pill>
              <span className="font-mono text-osint-text truncate flex-1">{paramsSummary(h.params) || '—'}</span>
              <span className="text-osint-muted whitespace-nowrap" title={h.user_id || undefined}>{authorLabel(h, names)}</span>
              <span className="font-mono text-osint-muted whitespace-nowrap" title={fmtAbs(h.ts)}>{h.result_count != null ? `${h.result_count} results · ` : ''}{relativeTime(h.ts)}</span>
              <Button size="sm" onClick={() => onRun(h)} title="Run this search again"><LuPlay size={11} /></Button>
            </li>
          ))}
        </ul>
      )}
      {rows.length > 0 && (
        <div className="px-3 py-2 space-y-0.5">
          <PagedFooter shown={rows.length} total={list.total} hasMore={list.hasMore} busy={list.loadingMore} onMore={list.loadMore} error={list.moreError} noun="recent searches" />
          {meta?.retained_max != null && <div className="text-[10px] text-osint-muted">the server keeps each member's {meta.retained_max} most recent searches</div>}
        </div>
      )}
      <ConfirmDialog open={confirmClear} onClose={() => setConfirmClear(false)} onConfirm={clear} title="Clear search history?" confirmLabel="Clear history" message="Removes every search you ran from the workspace history. Your teammates' searches and all saved searches are kept." />
    </Section>
  );
}
