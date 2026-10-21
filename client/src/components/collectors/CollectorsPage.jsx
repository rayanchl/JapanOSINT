import React, { useMemo, useState } from 'react';
import { Link } from 'react-router-dom';
import { LuRefreshCw, LuChevronDown, LuChevronRight } from 'react-icons/lu';
import { useApi } from '../../hooks/useApi.js';
import { Page, Card, Pill, Button, Input, Segmented, ErrorNotice, EmptyState, LoadingState, BoundNote, cx } from '../ui/kit.jsx';
import { relativeTime } from '../../utils/time.js';
import { fmtCount, statusTone } from '../../utils/sourceFormat.js';

/* /console/collectors — the registry's collectors under natural-language GROUP
 * headings (GET /api/collectors: `groups` = every grp_* key with its member
 * collectors and folded tallies; `data` = every collector). A collector is the
 * beat a source was authored and probed in; a group is the heading
 * core/collectors.def gives it. Every source is in exactly one collector and
 * every collector in exactly one group; a collector the server could not
 * place lands in the visible "Unassigned" group, never out of sight. */

/** groups[] + collectors[] → [{...group, members: [collector rows]}], in the
 * server's order, keeping only members that pass `keep`. Groups left with no
 * member are dropped from the view (their count is still in the bound). */
export function groupCollectors(groups, rows, keep = () => true) {
  const byKey = new Map(rows.map((r) => [r.key, r]));
  const out = [];
  for (const g of groups || []) {
    const members = (g.collectors || []).map((k) => byKey.get(k)).filter(Boolean).filter(keep);
    if (members.length) out.push({ ...g, members });
  }
  return out;
}

export function HealthBar({ status, total }) {
  const n = Math.max(1, total || 0);
  const seg = (k, cls) => <div key={k} className={cls} style={{ width: `${(100 * (status?.[k] || 0)) / n}%` }} title={`${k}: ${fmtCount(status?.[k] || 0)}`} />;
  return (
    <div className="flex h-1.5 w-full rounded overflow-hidden bg-osint-border/40">
      {seg('online', 'bg-neon-green/70')}{seg('degraded', 'bg-accent/70')}{seg('offline', 'bg-neon-red/70')}{seg('pending', 'bg-osint-border')}
    </div>
  );
}

function Tallies({ c }) {
  return (
    <div className="flex flex-wrap gap-x-3 gap-y-0.5 mt-1.5 text-[11px] font-mono text-osint-muted">
      <span className="text-accent">{fmtCount(c.items)} items</span>
      {c.scheduled > 0 && <span>{fmtCount(c.scheduled)} scheduled</span>}
      {c.pivots > 0 && <span>{fmtCount(c.pivots)} pivots</span>}
      {c.unreachable > 0 && <span className="text-neon-red" title="fixed URL, no interval: registered but never runs">{fmtCount(c.unreachable)} unreachable</span>}
      {c.internal > 0 && <span>{fmtCount(c.internal)} internal</span>}
      {c.notices > 0 && <span title="engine notices stored">{fmtCount(c.notices)} notices</span>}
      <span title={c.last_fetched || 'never'}>fetched {c.last_fetched ? relativeTime(c.last_fetched) : 'never'}</span>
      <span>{c.status?.online || 0} online · {c.status?.degraded || 0} degraded · {c.status?.offline || 0} offline · {c.status?.pending || 0} pending</span>
    </div>
  );
}

export function CollectorCard({ c }) {
  return (
    <Card>
      <div className="flex items-start justify-between gap-2">
        <div className="min-w-0">
          <Link to={`/console/collectors/${encodeURIComponent(c.key)}`} className="text-sm text-osint-text hover:text-accent">{c.label || c.key}</Link>
          {c.label_ja && <span className="ml-2 text-xs text-osint-muted">{c.label_ja}</span>}
          <div className="font-mono text-[10px] text-osint-muted truncate">{c.key}</div>
          {c.description && <div className="text-[11px] text-osint-muted mt-1">{c.description}</div>}
        </div>
        <div className="text-right whitespace-nowrap">
          <div className="font-mono text-lg text-osint-text">{fmtCount(c.sources)}</div>
          <div className="text-[10px] text-osint-muted">source{c.sources === 1 ? '' : 's'}</div>
        </div>
      </div>
      <div className="mt-2"><HealthBar status={c.status} total={c.sources} /></div>
      <Tallies c={c} />
      <div className="flex flex-wrap gap-1 mt-1.5">
        {(c.categories?.values || []).map((v) => <Pill key={v.key}>{v.key} {fmtCount(v.count)}</Pill>)}
        {(c.engines?.values || []).map((v) => <Pill key={v.key} tone="cyan">{v.key}</Pill>)}
      </div>
    </Card>
  );
}

function GroupSection({ g, open, onToggle }) {
  const unassigned = g.key === 'grp_unassigned';
  return (
    <section className={cx('rounded-[12px] border', unassigned ? 'border-neon-red/40' : 'border-osint-border/60')}>
      <button type="button" onClick={onToggle} className="w-full text-left p-3 hover:bg-white/5 rounded-t-[12px]">
        <div className="flex items-start gap-2">
          {open ? <LuChevronDown size={16} className="mt-1 text-osint-muted" /> : <LuChevronRight size={16} className="mt-1 text-osint-muted" />}
          <div className="min-w-0 flex-1">
            <div className="flex items-center gap-2 flex-wrap">
              <Link to={`/console/collectors/${encodeURIComponent(g.key)}`} onClick={(e) => e.stopPropagation()} className="text-base text-osint-text hover:text-accent">{g.label || g.key}</Link>
              {g.label_ja && <span className="text-xs text-osint-muted">{g.label_ja}</span>}
              <span className="font-mono text-[10px] text-osint-muted">{g.key}</span>
              {unassigned && <Pill tone="danger">needs a collectors.def entry</Pill>}
            </div>
            {g.description && <div className="text-[11px] text-osint-muted mt-0.5">{g.description}</div>}
            <div className="mt-2"><HealthBar status={g.status} total={g.sources} /></div>
            <Tallies c={g} />
          </div>
          <div className="text-right whitespace-nowrap">
            <div className="font-mono text-xl text-osint-text">{fmtCount(g.sources)}</div>
            <div className="text-[10px] text-osint-muted">sources in {fmtCount(g.members.length)}{g.members.length !== g.collector_count ? ` of ${fmtCount(g.collector_count)}` : ''} collector{g.collector_count === 1 ? '' : 's'}</div>
          </div>
        </div>
      </button>
      {open && (
        <div className="p-3 pt-0 space-y-2">
          <div className="grid grid-cols-1 lg:grid-cols-2 gap-2">
            {g.members.filter((c) => c.sources !== 1).map((c) => <CollectorCard key={c.key} c={c} />)}
          </div>
          <SingletonFold items={g.members.filter((c) => c.sources === 1)} groupKey={g.key} />
        </div>
      )}
    </section>
  );
}

/* Collectors that hold exactly ONE source — the per-country hand-written beats
 * (bf_health, bh_company-registry, …) — are folded into one row per group so a
 * group with 26 of them reads as a list, not 26 cards. Nothing is hidden: the
 * fold states its count, opens in place, and every entry links to its page. */
/* a one-source collector's health IS its source's last run */
function singleStatus(c) {
  const st = c.status || {};
  return st.online ? 'online' : st.offline ? 'offline' : st.degraded ? 'degraded' : 'pending';
}

export function SingletonFold({ items, groupKey }) {
  const [open, setOpen] = useState(false);
  if (!items.length) return null;
  return (
    <div className="rounded-[10px] border border-osint-border/40">
      <button type="button" onClick={() => setOpen((o) => !o)} className="w-full text-left px-3 py-2 text-[11px] text-osint-muted hover:bg-white/5 flex items-center gap-2" data-testid={`singletons-${groupKey}`}>
        {open ? <LuChevronDown size={12} /> : <LuChevronRight size={12} />}
        <span>{fmtCount(items.length)} single-source collector{items.length === 1 ? '' : 's'}</span>
      </button>
      {open && (
        <ul className="px-3 pb-2 grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-x-4 gap-y-1 text-xs">
          {items.map((c) => (
            <li key={c.key} className="flex items-baseline gap-2 min-w-0">
              <Pill tone={statusTone(singleStatus(c))} className="shrink-0">{singleStatus(c)}</Pill>
              <Link to={`/console/collectors/${encodeURIComponent(c.key)}`} className="text-osint-text hover:text-accent truncate">{c.label || c.key}</Link>
              <span className="font-mono text-[10px] text-osint-muted truncate">{c.key}</span>
            </li>
          ))}
        </ul>
      )}
    </div>
  );
}

export default function CollectorsPage() {
  const { data, error, loading, reload } = useApi('/api/collectors');
  const rows = Array.isArray(data?.data) ? data.data : [];
  const groups = Array.isArray(data?.groups) ? data.groups : [];
  const [q, setQ] = useState('');
  const [sort, setSort] = useState('sources');
  const [closed, setClosed] = useState(() => new Set());
  const [allOpen, setAllOpen] = useState(true);

  const view = useMemo(() => {
    const needle = q.trim().toLowerCase();
    const keep = (r) => !needle || [r.key, r.label, r.label_ja, r.description].some((s) => s && String(s).toLowerCase().includes(needle));
    const sorted = [...rows].sort((a, b) => sort === 'items' ? (b.items || 0) - (a.items || 0)
      : sort === 'name' ? String(a.label || a.key).localeCompare(String(b.label || b.key))
      : sort === 'fetched' ? String(b.last_fetched || '').localeCompare(String(a.last_fetched || ''))
      : (b.sources || 0) - (a.sources || 0));
    // a group heading matching the needle keeps all its members
    const gs = groupCollectors(groups, sorted, keep).concat(
      groupCollectors(groups.filter((g) => needle && [g.key, g.label, g.label_ja, g.description].some((s) => s && String(s).toLowerCase().includes(needle))), sorted, (r) => !keep(r)),
    );
    // merge duplicates produced by the two passes, keep server order
    const merged = new Map();
    for (const g of gs) { const cur = merged.get(g.key); merged.set(g.key, cur ? { ...cur, members: [...cur.members, ...g.members] } : g); }
    const order = new Map(groups.map((g, i) => [g.key, i]));
    return [...merged.values()].sort((a, b) => (order.get(a.key) ?? 999) - (order.get(b.key) ?? 999));
  }, [rows, groups, q, sort]);
  const shownCollectors = view.reduce((n, g) => n + g.members.length, 0);
  const isOpen = (k) => (allOpen ? !closed.has(k) : closed.has(k));
  const toggle = (k) => setClosed((s) => { const n = new Set(s); if (n.has(k)) n.delete(k); else n.add(k); return n; });

  return (
    <Page wide title="Collectors"
      subtitle={<span>Every source sits in exactly one collector, every collector under one heading · <Link to="/console/sources" className="text-accent hover:underline">all sources</Link></span>}
      actions={<div className="flex gap-2">
        <Button size="sm" onClick={() => { setAllOpen((v) => !v); setClosed(new Set()); }}>{allOpen ? 'collapse all' : 'expand all'}</Button>
        <Button size="sm" onClick={() => reload()} title="Reload"><LuRefreshCw size={12} /></Button>
      </div>}
    >
      <div className="flex flex-wrap items-center gap-2">
        <Input className="flex-1 min-w-[200px]" placeholder="filter groups and collectors (key, label, 日本語, description)…" value={q} onChange={(e) => setQ(e.target.value)} />
        <Segmented value={sort} onChange={setSort} options={[{ value: 'sources', label: 'most sources' }, { value: 'items', label: 'most items' }, { value: 'fetched', label: 'last fetched' }, { value: 'name', label: 'name' }]} />
      </div>
      {loading && rows.length === 0 && <LoadingState label="Loading collectors…" />}
      {error && <ErrorNotice error={error} title="Could not list collectors" onRetry={reload} />}
      {data && !error && (
        <div className="flex items-center justify-between flex-wrap gap-2">
          <BoundNote shown={shownCollectors} total={rows.length} noun="collectors" />
          <span className="font-mono text-[10px] text-osint-muted">{fmtCount(data.meta?.sources)} sources · {fmtCount(data.meta?.total)} collectors · {fmtCount(data.meta?.groups)} groups{data.meta?.unassigned_collectors > 0 ? ` · ${fmtCount(data.meta.unassigned_collectors)} UNASSIGNED` : ''} · snapshot {data.meta?.snapshot_at ? relativeTime(data.meta.snapshot_at) : '—'}</span>
        </div>
      )}
      {data && !error && rows.length > 0 && view.length === 0 && <EmptyState title="No group or collector matches this filter." />}
      <div className="space-y-3">
        {view.map((g) => <GroupSection key={g.key} g={g} open={isOpen(g.key)} onToggle={() => toggle(g.key)} />)}
      </div>
    </Page>
  );
}
