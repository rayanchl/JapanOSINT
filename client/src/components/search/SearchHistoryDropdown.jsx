import React, { useState } from 'react';
import { Link } from 'react-router-dom';
import { LuUsers, LuRotateCcw, LuTrash2 } from 'react-icons/lu';
import { api } from '../../api/client.js';
import { usePagedList } from '../../hooks/usePagedList.js';
import { intelHistoryRoute } from '../../api/searchHistory.js';
import { useMemberNames, authorLabel } from '../../hooks/useMembers.js';
import { Pill, Button, ErrorNotice, LoadingState, ConfirmDialog, PagedFooter, cx, toast } from '../ui/kit.jsx';
import { relativeTime } from '../../utils/time.js';

/** Kinds `native/core/savedsearchapi.c` accepts, with the iOS tones. */
const KIND = {
  osint: { label: 'OSINT', tone: 'accent' },
  intel: { label: 'Intel', tone: 'cyan' },
  entity: { label: 'Entity', tone: 'purple' },
  breach: { label: 'Breach', tone: 'danger' },
  map: { label: 'Map', tone: 'success' },
};

/** Query terms out of a params bag (SearchParamsSummary on iOS). */
export function paramsSummary(params) {
  if (!params || typeof params !== 'object') return null;
  const q = params.q ?? params.query ?? params.value ?? params.text;
  if (typeof q === 'string' && q.trim()) return q.trim();
  // `view` only says which tab an intel entry opens on; it is not a term.
  const parts = Object.entries(params).filter(([k, v]) => k !== 'view' && v != null && v !== '').map(([k, v]) => `${k}=${typeof v === 'object' ? JSON.stringify(v) : v}`);
  return parts.length ? parts.join(' · ') : null;
}

/** Where a history row re-runs: osint runs here; other kinds route to their tab. */
export function rerunTarget(entry) {
  const q = paramsSummary(entry.params);
  switch (entry.kind) {
    case 'osint': return q ? { run: q } : null;
    case 'intel': return { to: intelHistoryRoute(entry.params) };   // all params, the right tab
    case 'entity': return q ? { to: `/entities?q=${encodeURIComponent(q)}` } : { to: '/entities' };
    case 'breach': return q ? { to: `/intel?source=breach&q=${encodeURIComponent(q)}` } : null;
    case 'map': return { to: '/' };
    default: return null;
  }
}

/**
 * Inline recent-searches dropdown (Roadmap 38). `/api/search-history` is
 * WORKSPACE-wide on read (every member's entries, each with `user_id` and
 * `mine`; decided 2026-10-05) and author-only on clear. Every committed search
 * is recorded (decided 2026-10-09): intel, near, semantic, per-source and
 * entity searches by the clients (api/searchHistory.js), OSINT runs and saved-
 * search runs by the server. The same search by the same person inside 10
 * minutes is one entry. The list is paged: the footer shows the server's
 * measured total and loads the next `limit`.
 */
export default function SearchHistoryDropdown({ open, onRun, limit = 20, onClose }) {
  const list = usePagedList(open ? '/api/search-history' : null, { pageSize: limit, enabled: Boolean(open), deps: [open] });
  const { rows, error, loading, reload, meta } = list;
  const [confirmClear, setConfirmClear] = useState(false);
  const [clearing, setClearing] = useState(false);
  // Before the early return: a hook called after it runs only while open, and
  // React throws on the render where the hook count changes (opening). It
  // fetches only while open, like the list above.
  const names = useMemberNames({ enabled: Boolean(open) });
  if (!open) return null;
  const retained = meta?.retained_max;

  const clear = async () => {
    setClearing(true);
    try {
      const r = await api.del('/api/search-history');
      toast(`Cleared ${r?.deleted ?? ''} history ${r?.deleted === 1 ? 'entry' : 'entries'}`.replace('  ', ' '));
      setConfirmClear(false);
      await reload();
    } catch (e) { toast(`Clear failed: ${e.message}`, { tone: 'danger' }); }
    finally { setClearing(false); }
  };

  return (
    <div className="rounded-[10px] border border-osint-border bg-osint-surface shadow-xl overflow-hidden">
      <div className="flex items-start gap-2 px-3 py-2 border-b border-osint-border">
        <LuUsers size={13} className="text-accent mt-0.5 flex-shrink-0" />
        <div className="min-w-0 text-[11px]">
          <div className="text-osint-text font-medium">Shared with your workspace</div>
          <div className="text-osint-muted">Every member of this workspace sees every member's searches here, each with who ran it. Clearing removes only your own entries, and is not logged.</div>
        </div>
        <div className="ml-auto flex items-center gap-1">
          <Link to="/console/saved-searches" className="text-[11px] text-osint-muted hover:text-accent whitespace-nowrap" onClick={onClose}>Manage</Link>
          <Button size="sm" variant="ghost" disabled={!rows.length || clearing} onClick={() => setConfirmClear(true)} title="Clear history"><LuTrash2 size={12} /></Button>
        </div>
      </div>
      {loading && !list.loaded && <LoadingState label="Loading history…" />}
      {error && <div className="p-2"><ErrorNotice error={error} title="Couldn't load history" onRetry={reload} /></div>}
      {!loading && !error && rows.length === 0 && (
        <div className="px-3 py-4 text-xs text-osint-muted text-center">No recorded searches yet. Every search anyone in this workspace runs is recorded here.</div>
      )}
      {rows.length > 0 && (
        <ul className="max-h-72 overflow-auto divide-y divide-osint-border">
          {rows.map((e) => {
            const k = KIND[e.kind] || { label: e.kind, tone: 'neutral' };
            const summary = paramsSummary(e.params);
            const target = rerunTarget(e);
            const body = (
              <>
                <Pill tone={k.tone}>{k.label}</Pill>
                <span className="text-sm text-osint-text truncate flex-1">{summary || '(no query terms)'}</span>
                <span className="text-[10px] text-osint-muted whitespace-nowrap">{authorLabel(e, names)}</span>
                <span className="text-[10px] font-mono text-osint-muted whitespace-nowrap">
                  {relativeTime(e.ts)} · {e.result_count == null ? 'count not recorded' : `${e.result_count} result${e.result_count === 1 ? '' : 's'}`}
                </span>
                {target && <LuRotateCcw size={12} className="text-osint-muted flex-shrink-0" />}
              </>
            );
            const cls = cx('w-full flex items-center gap-2 px-3 py-1.5 text-left', target ? 'hover:bg-white/5' : 'opacity-70');
            if (target?.run) return <li key={e.id}><button type="button" className={cls} onClick={() => { onRun(target.run); onClose?.(); }}>{body}</button></li>;
            if (target?.to) return <li key={e.id}><Link to={target.to} className={cls} onClick={onClose}>{body}</Link></li>;
            return <li key={e.id} title="This kind cannot be re-run from here"><div className={cls}>{body}</div></li>;
          })}
        </ul>
      )}
      <div className="px-3 py-1.5 border-t border-osint-border flex items-center justify-between gap-2">
        <PagedFooter shown={rows.length} total={list.total} hasMore={list.hasMore} busy={list.loadingMore} onMore={list.loadMore} error={list.moreError} noun="entries" />
        <span className="text-[10px] text-osint-muted">{retained ? `server keeps each member's ${retained} most recent` : "the server keeps only each member's most recent entries"}</span>
      </div>
      <ConfirmDialog
        open={confirmClear}
        onClose={() => setConfirmClear(false)}
        onConfirm={clear}
        busy={clearing}
        title="Clear your search history?"
        confirmLabel="Clear history"
        message="Deletes every entry you ran. Your teammates' entries are kept, nothing is recorded about the deletion, and saved searches are kept."
      />
    </div>
  );
}
