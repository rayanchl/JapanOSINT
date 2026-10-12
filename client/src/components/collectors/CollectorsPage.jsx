import React, { useMemo, useState } from 'react';
import { Link } from 'react-router-dom';
import { LuRefreshCw } from 'react-icons/lu';
import { useApi } from '../../hooks/useApi.js';
import { Page, Card, Pill, Button, Input, Segmented, ErrorNotice, EmptyState, LoadingState, BoundNote, cx } from '../ui/kit.jsx';
import { relativeTime } from '../../utils/time.js';
import { fmtCount } from '../../utils/sourceFormat.js';

/* /console/collectors — every collector key the registry carries, with the
 * tallies the server measured (GET /api/collectors). A collector is the beat
 * a source was authored and probed in; every source is in exactly one. Keys
 * with a parent in core/collectors.def nest under it; the rest are top-level,
 * some holding one source, some hundreds. */

export function nestCollectors(rows) {
  const byKey = new Map(rows.map((r) => [r.key, { ...r, children: [] }]));
  const top = [];
  for (const r of byKey.values()) {
    const p = r.parent ? byKey.get(r.parent) : null;
    if (p && p !== r) p.children.push(r); else top.push(r);
  }
  return top;
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

function CollectorCard({ c, depth = 0 }) {
  return (
    <>
      <Card className={cx(depth > 0 && 'ml-6 border-dashed')}>
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
        <div className="flex flex-wrap gap-1 mt-1.5">
          {(c.categories?.values || []).map((v) => <Pill key={v.key}>{v.key} {fmtCount(v.count)}</Pill>)}
          {(c.engines?.values || []).map((v) => <Pill key={v.key} tone="cyan">{v.key}</Pill>)}
        </div>
      </Card>
      {c.children.map((ch) => <CollectorCard key={ch.key} c={ch} depth={depth + 1} />)}
    </>
  );
}

export default function CollectorsPage() {
  const { data, error, loading, reload } = useApi('/api/collectors');
  const rows = Array.isArray(data?.data) ? data.data : [];
  const [q, setQ] = useState('');
  const [sort, setSort] = useState('sources');
  const nested = useMemo(() => {
    const needle = q.trim().toLowerCase();
    const xs = rows.filter((r) => !needle || [r.key, r.label, r.label_ja, r.description].some((s) => s && String(s).toLowerCase().includes(needle)));
    const sorted = [...xs].sort((a, b) => sort === 'items' ? (b.items || 0) - (a.items || 0)
      : sort === 'name' ? String(a.label || a.key).localeCompare(String(b.label || b.key))
      : sort === 'fetched' ? String(b.last_fetched || '').localeCompare(String(a.last_fetched || ''))
      : (b.sources || 0) - (a.sources || 0));
    return nestCollectors(sorted);
  }, [rows, q, sort]);
  const totalSources = data?.meta?.sources;

  return (
    <Page wide title="Collectors"
      subtitle={<span>Every source sits in exactly one collector — the beat it was authored and probed in · <Link to="/console/sources" className="text-accent hover:underline">all sources</Link></span>}
      actions={<Button size="sm" onClick={() => reload()} title="Reload"><LuRefreshCw size={12} /></Button>}
    >
      <div className="flex flex-wrap items-center gap-2">
        <Input className="flex-1 min-w-[200px]" placeholder="filter collectors (key, label, 日本語, description)…" value={q} onChange={(e) => setQ(e.target.value)} />
        <Segmented value={sort} onChange={setSort} options={[{ value: 'sources', label: 'most sources' }, { value: 'items', label: 'most items' }, { value: 'fetched', label: 'last fetched' }, { value: 'name', label: 'name' }]} />
      </div>
      {loading && rows.length === 0 && <LoadingState label="Loading collectors…" />}
      {error && <ErrorNotice error={error} title="Could not list collectors" onRetry={reload} />}
      {data && !error && (
        <div className="flex items-center justify-between">
          <BoundNote shown={nested.reduce((n, c) => n + 1 + c.children.length, 0)} total={rows.length} noun="collectors" />
          <span className="font-mono text-[10px] text-osint-muted">{fmtCount(totalSources)} sources in {fmtCount(data.meta?.total)} collectors · {fmtCount(data.meta?.labelled)} labelled in collectors.def · snapshot {data.meta?.snapshot_at ? relativeTime(data.meta.snapshot_at) : '—'}</span>
        </div>
      )}
      {data && !error && rows.length > 0 && nested.length === 0 && <EmptyState title="No collector matches this filter." />}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-2">
        {nested.map((c) => <CollectorCard key={c.key} c={c} />)}
      </div>
    </Page>
  );
}
