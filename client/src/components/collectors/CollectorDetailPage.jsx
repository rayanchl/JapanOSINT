import React, { useEffect, useMemo, useState } from 'react';
import { Link, useParams, useSearchParams } from 'react-router-dom';
import { LuRefreshCw } from 'react-icons/lu';
import { api } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { Page, Card, Pill, Button, Input, Segmented, ErrorNotice, EmptyState, LoadingState, BoundNote, PagedFooter, KV } from '../ui/kit.jsx';
import { relativeTime } from '../../utils/time.js';
import { fmtCount, fmtInterval, statusTone, gradeTone } from '../../utils/sourceFormat.js';
import { HealthBar } from './CollectorsPage.jsx';

/* /console/collectors/:key — one collector: its measured header, the id
 * prefixes its sources share (a sub-grouping hint the server computed, not a
 * rename), its children in collectors.def, and its sources paged from the
 * same search the Sources page uses, restricted to this key. */

export default function CollectorDetailPage() {
  const { key } = useParams();
  const [sp, setSp] = useSearchParams();
  const q = sp.get('q') || '';
  const sort = sp.get('sort') || 'name';
  const [draft, setDraft] = useState(q);
  useEffect(() => { setDraft(q); }, [q]);
  const base = `/api/collectors/${encodeURIComponent(key)}?limit=50${q ? `&q=${encodeURIComponent(q)}` : ''}&sort=${encodeURIComponent(sort)}`;
  const head = useApi(base);
  const [rows, setRows] = useState([]);
  const [page, setPage] = useState(null);
  const [more, setMore] = useState({ busy: false, error: null });
  useEffect(() => {
    const src = head.data?.sources;
    setRows(Array.isArray(src?.data) ? src.data : []);
    setPage(src?.page || null);
  }, [head.data]);
  const loadMore = async () => {
    if (!page?.has_more) return;
    setMore({ busy: true, error: null });
    try {
      const j = await api.get(`${base}&offset=${page.offset + page.count}`);
      const next = Array.isArray(j?.sources?.data) ? j.sources.data : [];
      setRows((xs) => { const seen = new Set(xs.map((r) => r.id)); return [...xs, ...next.filter((r) => !seen.has(r.id))]; });
      setPage(j?.sources?.page || null);
      setMore({ busy: false, error: null });
    } catch (e) { setMore({ busy: false, error: e }); }
  };
  const c = head.data?.collector;
  const groups = head.data?.groups;
  const children = head.data?.children || [];
  const set = (patch) => { const n = new URLSearchParams(sp); for (const [k, v] of Object.entries(patch)) { if (v) n.set(k, v); else n.delete(k); } setSp(n, { replace: true }); };
  const prefixOf = (id) => { const m = /^[^_-]+[_-]/.exec(id); return m ? m[0] : null; };
  const grouped = useMemo(() => {
    if (!groups?.values?.length) return null;
    const keys = new Set(groups.values.map((g) => g.key));
    const buckets = new Map(); const rest = [];
    for (const r of rows) { const p = prefixOf(r.id); if (p && keys.has(p)) { if (!buckets.has(p)) buckets.set(p, []); buckets.get(p).push(r); } else rest.push(r); }
    return { buckets, rest };
  }, [rows, groups]);

  if (head.loading && !head.data) return <Page wide title={key}><LoadingState label="Loading collector…" /></Page>;
  if (head.error) {
    return <Page wide title={key} subtitle={<Link to="/console/collectors" className="hover:text-accent">← all collectors</Link>}>
      <ErrorNotice error={head.error} title={head.error.status === 404 ? 'No registered source has this collector' : 'Could not load the collector'} onRetry={head.reload} />
    </Page>;
  }
  if (!c) return null;

  const Row = ({ s }) => (
    <tr key={s.id} className="border-b border-osint-border/40 hover:bg-white/5">
      <td className="px-3 py-2"><Pill tone={statusTone(s.status)}>{s.status || 'pending'}</Pill></td>
      <td className="px-3 py-2 min-w-[220px]">
        <Link to={`/console/sources/${encodeURIComponent(s.id)}`} className="text-osint-text hover:text-accent">{s.name || s.id}</Link>
        {s.name_ja && s.name_ja !== s.name && <span className="ml-2 text-osint-muted">{s.name_ja}</span>}
        <div className="font-mono text-[10px] text-osint-muted truncate max-w-[360px]" title={s.id}>{s.id}{s.category ? ` · ${s.category}` : ''}{s.host ? ` · ${s.host}` : ''}</div>
      </td>
      <td className="px-3 py-2 font-mono text-[11px] text-osint-muted">{s.engine} · {s.kind}</td>
      <td className="px-3 py-2 font-mono text-[11px] text-osint-muted">{fmtInterval(s.interval)}</td>
      <td className="px-3 py-2 text-right font-mono text-neon-green">{fmtCount(s.item_count)}</td>
      <td className="px-3 py-2 font-mono text-[11px] text-osint-muted" title={s.last_fetched || 'never'}>{s.last_fetched ? relativeTime(s.last_fetched) : 'never'}</td>
      <td className="px-3 py-2">{s.trust?.rated ? <Pill tone={gradeTone(s.trust.grade)}>{s.trust.grade}</Pill> : <span className="text-osint-muted text-[10px]">unrated</span>}</td>
    </tr>
  );
  const Table = ({ items }) => (
    <Card padded={false} className="overflow-x-auto">
      <table className="w-full text-xs">
        <thead><tr className="border-b border-osint-border text-osint-muted uppercase tracking-wider text-left">
          <th className="px-3 py-2">Status</th><th className="px-3 py-2">Source</th><th className="px-3 py-2">Engine · kind</th><th className="px-3 py-2">Interval</th><th className="px-3 py-2 text-right">Items</th><th className="px-3 py-2">Last fetched</th><th className="px-3 py-2">Grade</th>
        </tr></thead>
        <tbody>{items.map((s) => <Row key={s.id} s={s} />)}</tbody>
      </table>
    </Card>
  );

  return (
    <Page wide
      title={<span className="flex items-center gap-2 flex-wrap">{c.label || c.key}{c.label_ja && <span className="text-sm text-osint-muted font-normal">{c.label_ja}</span>}</span>}
      subtitle={<span className="flex flex-wrap items-center gap-x-2"><Link to="/console/collectors" className="hover:text-accent">← all collectors</Link><span>·</span><span className="font-mono">{c.key}</span>{c.parent && <><span>·</span><span>under <Link to={`/console/collectors/${encodeURIComponent(c.parent)}`} className="text-accent hover:underline">{c.parent}</Link></span></>}</span>}
      actions={<Button size="sm" onClick={() => head.reload()} title="Reload"><LuRefreshCw size={12} /></Button>}
    >
      {c.description && <p className="text-xs text-osint-muted -mt-2">{c.description}</p>}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-3">
        <Card>
          <KV pairs={[
            ['sources', fmtCount(c.sources)],
            ['scheduled / pivots', `${fmtCount(c.scheduled)} / ${fmtCount(c.pivots)}`],
            c.unreachable > 0 && ['unreachable (rule 3)', fmtCount(c.unreachable)],
            c.internal > 0 && ['internal pods', fmtCount(c.internal)],
            ['items stored', fmtCount(c.items)],
            ['notices stored', fmtCount(c.notices)],
            ['last fetched', c.last_fetched ? relativeTime(c.last_fetched) : 'never'],
            ['categories', (c.categories?.values || []).map((v) => `${v.key} (${fmtCount(v.count)})`).join(', ') || '—'],
            ['engines', (c.engines?.values || []).map((v) => `${v.key} (${fmtCount(v.count)})`).join(', ') || '—'],
          ].filter(Boolean)} />
        </Card>
        <Card>
          <div className="text-[10px] uppercase tracking-wider text-osint-muted mb-1">health (last run of each source)</div>
          <HealthBar status={c.status} total={c.sources} />
          <div className="mt-1 text-[11px] font-mono text-osint-muted">{c.status?.online || 0} online · {c.status?.degraded || 0} degraded · {c.status?.offline || 0} offline · {c.status?.pending || 0} pending (never run)</div>
          {children.length > 0 && <div className="mt-3 text-[11px]"><span className="text-osint-muted">child collectors: </span>{children.map((k) => <Link key={k} to={`/console/collectors/${encodeURIComponent(k)}`} className="font-mono text-accent hover:underline mr-2">{k}</Link>)}</div>}
          {groups?.values?.length > 0 && <div className="mt-3 text-[11px]"><span className="text-osint-muted">shared id prefixes: </span>{groups.values.map((g) => <Pill key={g.key} className="mr-1">{g.key} {fmtCount(g.count)}</Pill>)}<BoundNote shown={groups.shown} total={groups.total} noun="prefix groups" className="ml-2" /></div>}
        </Card>
      </div>

      <form onSubmit={(e) => { e.preventDefault(); set({ q: draft.trim() }); }} className="flex flex-wrap items-center gap-2">
        <Input className="flex-1 min-w-[200px]" placeholder="search within this collector…" value={draft} onChange={(e) => setDraft(e.target.value)} />
        <Button type="submit">Search</Button>
        <Segmented value={sort} onChange={(v) => set({ sort: v })} options={[{ value: 'name', label: 'name' }, { value: 'id', label: 'id' }, { value: 'items', label: 'items' }, { value: 'fetched', label: 'fetched' }, { value: 'grade', label: 'grade' }]} />
      </form>
      <BoundNote shown={rows.length} total={page?.total} more={page?.has_more} noun="sources" />
      {rows.length === 0 && <EmptyState title="No source in this collector matches." />}
      {grouped ? (
        <div className="space-y-3">
          {[...grouped.buckets.entries()].map(([p, items]) => (
            <div key={p}>
              <div className="text-[10px] uppercase tracking-wider text-osint-muted mb-1">prefix <span className="font-mono text-osint-text">{p}</span> · {fmtCount(items.length)} shown</div>
              <Table items={items} />
            </div>
          ))}
          {grouped.rest.length > 0 && <div><div className="text-[10px] uppercase tracking-wider text-osint-muted mb-1">other · {fmtCount(grouped.rest.length)} shown</div><Table items={grouped.rest} /></div>}
        </div>
      ) : rows.length > 0 && <Table items={rows} />}
      {rows.length > 0 && <PagedFooter shown={rows.length} total={page?.total} hasMore={page?.has_more} busy={more.busy} onMore={loadMore} error={more.error} noun="sources" />}
    </Page>
  );
}
