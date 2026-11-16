import React, { useEffect, useMemo, useState } from 'react';
import { Link, useSearchParams } from 'react-router-dom';
import { LuRefreshCw, LuSearch } from 'react-icons/lu';
import { api } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { usePagedList } from '../../hooks/usePagedList.js';
import { Page, Card, Pill, Button, Input, Select, Segmented, Toggle, ErrorNotice, EmptyState, LoadingState, BoundNote, PagedFooter, cx } from '../ui/kit.jsx';
import SourcesPanel from '../panels/SourcesPanel.jsx';
import Typeahead from '../ui/Typeahead.jsx';
import RunControl, { isPivotKind } from './RunControl.jsx';
import { relativeTime } from '../../utils/time.js';
import CadenceCard from './CadenceCard.jsx';
import { useAuthOptional } from '../../auth/AuthContext.jsx';
import { fmtInterval, fmtCount, statusTone, gradeTone, searchPath, SEARCH_KEYS, NOT_MEASURED } from '../../utils/sourceFormat.js';

/* /console/sources — the registry, searched and paged on the server
 * (GET /api/sources/search). The filters live in the URL so a filtered list is
 * a link. The tiles come from /api/status?summary=1 and /api/collectors, both
 * cheap and cached; nothing on this page is computed from the 18,883-row
 * dump the old dashboard sorted in the browser. */

export function readFilters(sp) {
  const f = {};
  for (const k of SEARCH_KEYS) {
    const v = sp.get(k);
    if (v == null || v === '') continue;
    f[k] = (k === 'has_items' || k === 'desc') ? v === '1' : v;
  }
  return f;
}
export function writeFilters(f) {
  const sp = new URLSearchParams();
  for (const k of SEARCH_KEYS) {
    const v = f[k];
    if (v === undefined || v === null || v === '' || v === false) continue;
    sp.set(k, v === true ? '1' : String(v));
  }
  return sp;
}

function Tile({ label, value, sub, tone }) {
  return (
    <Card className="flex flex-col">
      <span className="text-[10px] uppercase tracking-widest text-osint-muted">{label}</span>
      <span className={cx('text-xl font-mono font-bold', tone || 'text-osint-text')}>{value}</span>
      {sub && <span className="text-[10px] text-osint-muted mt-0.5">{sub}</span>}
    </Card>
  );
}

function FacetSelect({ label, value, onChange, facet, labelled }) {
  const vals = facet?.values || [];
  return (
    <Select value={value || ''} onChange={(e) => onChange(e.target.value)} title={label} aria-label={label}>
      <option value="">{label}{facet?.total > facet?.shown ? ` (top ${facet.shown} of ${facet.total})` : ''}</option>
      {value && !vals.some((v) => v.key === value) && <option value={value}>{value}</option>}
      {vals.map((v) => <option key={v.key} value={v.key}>{labelled && v.label ? `${v.label} · ${v.key}` : v.key} ({fmtCount(v.count)})</option>)}
    </Select>
  );
}

/* Find a source — a pivot above all — by what it does rather than by its id:
 * GET /api/sources/semantic?q=&kind=&k=. The answer says what it searched; a
 * 503 (index not ready) is shown as such, never as "no pivot matches". Each
 * pivot hit carries its own entity form, so the entity goes straight in. */
function PivotFinder() {
  const [draft, setDraft] = useState('');
  const [kind, setKind] = useState('pivot');
  const [state, setState] = useState({ busy: false, error: null, data: null, asked: null });
  const ask = async (e) => {
    e.preventDefault();
    const q = draft.trim();
    if (!q) return;
    setState({ busy: true, error: null, data: null, asked: q });
    try {
      const sp = new URLSearchParams({ q, k: '10' });
      if (kind) sp.set('kind', kind);
      setState({ busy: false, error: null, data: await api.get(`/api/sources/semantic?${sp.toString()}`), asked: q });
    } catch (err) { setState({ busy: false, error: err, data: null, asked: q }); }
  };
  const results = Array.isArray(state.data?.results) ? state.data.results : [];
  return (
    <Card className="space-y-2" data-testid="pivot-finder">
      <form onSubmit={ask} className="flex flex-wrap items-center gap-2">
        <span className="text-[10px] uppercase tracking-widest text-osint-muted">find by meaning</span>
        <Input className="flex-1 min-w-[220px]" aria-label="Describe what you want to look up" placeholder="e.g. company ownership registry, sanctions list, vessel tracking…" value={draft} onChange={(e) => setDraft(e.target.value)} />
        <Select value={kind} onChange={(e) => setKind(e.target.value)} aria-label="Kind of source to find">
          <option value="pivot">entity pivots</option>
          <option value="scheduled">scheduled feeds</option>
          <option value="">any kind</option>
        </Select>
        <Button type="submit" busy={state.busy} disabled={!draft.trim()}>Find</Button>
      </form>
      {state.error && <ErrorNotice error={state.error} title={state.error.status === 503 ? 'The meaning index is not ready' : 'Could not search by meaning'} />}
      {state.data && results.length === 0 && <div className="text-xs text-osint-muted">Nothing in the index matches “{state.asked}”.</div>}
      {results.length > 0 && (
        <>
          <BoundNote shown={results.length} total={null} noun="matches" />
          <ul className="divide-y divide-osint-border/40">
            {results.map((r) => (
              <li key={r.id} className="py-2 flex flex-wrap items-center justify-between gap-2">
                <div className="min-w-0">
                  <Link to={`/console/sources/${encodeURIComponent(r.id)}`} className="text-sm text-osint-text hover:text-accent">{r.name || r.id}</Link>
                  <div className="font-mono text-[10px] text-osint-muted truncate">{r.id} · {r.kind}{r.category ? ` · ${r.category}` : ''}{(r.entity_types || []).length ? ` · for ${r.entity_types.join(', ')}` : ''}</div>
                </div>
                {isPivotKind(r.kind) && <RunControl source={{ id: r.id, kind: r.kind }} compact />}
              </li>
            ))}
          </ul>
        </>
      )}
    </Card>
  );
}

export default function SourcesListPage() {
  const [sp, setSp] = useSearchParams();
  const filters = useMemo(() => readFilters(sp), [sp]);
  const [draft, setDraft] = useState(filters.q || '');
  useEffect(() => { setDraft(filters.q || ''); }, [filters.q]);
  const [showProbe, setShowProbe] = useState(false);

  const set = (patch) => setSp(writeFilters({ ...filters, ...patch }), { replace: true });
  const path = useMemo(() => searchPath(filters), [filters]);
  const list = usePagedList(path, { pageSize: 50 });
  const summary = useApi('/api/status?summary=1', { poll: 30000 });
  const collectors = useApi('/api/collectors');
  const sum = summary.data?.summary;
  const auth = useAuthOptional();
  // The server's PUT /api/sources/cadence is platform-operator only, and
  // AuthContext exports that fact as `isPlatformAdmin` (never `isPlatformOperator`).
  const canEditCadence = auth?.isPlatformAdmin === true;
  const facets = list.meta?.facets;

  return (
    <Page wide title="Sources"
      subtitle={<span>Every registered source, searched on the server · <Link to="/console/collectors" className="text-accent hover:underline">collectors</Link></span>}
      actions={<div className="flex items-center gap-2">
        <Button size="sm" onClick={() => setShowProbe((v) => !v)} className={cx(showProbe && 'border-accent/40 text-accent')} title="Live probe detail (operator tool)">Probe detail</Button>
        <Button size="sm" onClick={() => { list.reload(); summary.reload({ silent: true }); }} title="Reload" aria-label="Reload"><LuRefreshCw size={12} /></Button>
      </div>}
    >
      {showProbe && <div className="absolute top-3 right-3 z-40"><SourcesPanel onClose={() => setShowProbe(false)} /></div>}
      <CadenceCard canEdit={canEditCadence} />

      {summary.error && <ErrorNotice error={summary.error} title="Could not load the fleet summary" onRetry={summary.reload} />}
      <div className="grid grid-cols-2 md:grid-cols-4 lg:grid-cols-7 gap-2">
        <Tile label="Registered" value={sum ? fmtCount(sum.total) : NOT_MEASURED} sub={collectors.data?.meta ? `${fmtCount(collectors.data.meta.total)} collectors` : undefined} />
        <Tile label="Online" value={sum ? fmtCount(sum.online) : NOT_MEASURED} tone="text-neon-green" sub="last run stored records" />
        <Tile label="Degraded" value={sum ? fmtCount(sum.degraded) : NOT_MEASURED} tone="text-accent" sub="last run stored nothing" />
        <Tile label="Offline" value={sum ? fmtCount(sum.offline) : NOT_MEASURED} tone="text-neon-red" sub="last run failed" />
        <Tile label="Pending" value={sum ? fmtCount(sum.pending) : NOT_MEASURED} sub="never run" />
        <Tile label="Needs key" value={sum ? fmtCount(sum.missingKey) : NOT_MEASURED} sub={sum ? `${fmtCount(sum.requiresKey)} require one` : undefined} />
        <Tile label="Summary age" value={summary.loadedAt ? relativeTime(new Date(summary.loadedAt).toISOString()) : NOT_MEASURED} sub="refreshes every 30 s" />
      </div>

      <form onSubmit={(e) => { e.preventDefault(); set({ q: draft.trim() }); }} className="flex flex-wrap items-center gap-2">
        <div className="relative flex-1 min-w-[220px]">
          <LuSearch aria-hidden="true" size={13} className="absolute left-2 top-2.5 text-osint-muted" />
          <Input className="pl-7 w-full" aria-label="Search sources" placeholder="search id, name, 日本語, description, collector, host…" value={draft} onChange={(e) => setDraft(e.target.value)} />
        </div>
        <Button type="submit" variant="primary">Search</Button>
        <FacetSelect label="all groups" value={filters.group} onChange={(v) => set({ group: v })} facet={facets?.groups} labelled />
        {/* ~640 collectors but the facet carries the top N: a typeahead on the
            collectors route reaches every one of them. */}
        <Typeahead label="Collector" value={filters.collector} clearLabel="all collectors" placeholder="all collectors — type to search" className="w-[220px]"
          onChange={(v) => set({ collector: v })}
          pathFor={(t) => `/api/collectors${t ? `?q=${encodeURIComponent(t)}` : ''}`}
          pick={(j) => (Array.isArray(j?.data) ? j.data : []).slice(0, 30).map((c) => ({ value: c.key, label: c.label || c.key, sub: `${c.key}${c.sources != null ? ` · ${fmtCount(c.sources)} sources` : ''}` }))} />
        <FacetSelect label="all categories" value={filters.category} onChange={(v) => set({ category: v })} facet={facets?.categories} />
        <FacetSelect label="all engines" value={filters.engine} onChange={(v) => set({ engine: v })} facet={facets?.engines} />
        <FacetSelect label="all kinds" value={filters.kind} onChange={(v) => set({ kind: v })} facet={facets?.kinds} />
        <FacetSelect label="all statuses" value={filters.status} onChange={(v) => set({ status: v })} facet={facets?.statuses} />
        {/* "broken" is every state that is not ok — one query, so a state added
            later cannot be missed by adding four counts together. */}
        <FacetSelect label="all health" value={filters.health} onChange={(v) => set({ health: v })} facet={facets?.healths} />
        <Button size="sm" type="button" aria-pressed={filters.kind === 'pivot'} className={cx(filters.kind === 'pivot' && 'border-accent/40 text-accent')} title="Entity pivots: sources you run with an entity" onClick={() => set({ kind: filters.kind === 'pivot' ? '' : 'pivot' })}>pivots only</Button>
        <Toggle on={Boolean(filters.has_items)} onChange={(v) => set({ has_items: v })} label="with items only" />
        <Segmented value={filters.sort || 'name'} onChange={(v) => set({ sort: v })} options={[{ value: 'name', label: 'name' }, { value: 'items', label: 'items' }, { value: 'fetched', label: 'fetched' }, { value: 'grade', label: 'grade' }, { value: 'collector', label: 'collector' }]} />
        <Toggle on={Boolean(filters.desc)} onChange={(v) => set({ desc: v })} label="desc" />
        {Object.keys(filters).length > 0 && <Button size="sm" variant="ghost" onClick={() => setSp(new URLSearchParams(), { replace: true })}>clear</Button>}
      </form>

      <PivotFinder />

      {list.loading && list.rows.length === 0 && <LoadingState label="Searching the registry…" />}
      {list.error && <ErrorNotice error={list.error} title="Could not search sources" onRetry={list.reload} />}
      {!list.loading && !list.error && (
        <div className="flex items-center justify-between">
          <BoundNote shown={list.rows.length} total={list.page?.total} more={list.page?.has_more} noun="sources" />
          <span className="font-mono text-[10px] text-osint-muted">registry {fmtCount(list.meta?.registry_total)} · snapshot {list.meta?.snapshot_at ? relativeTime(list.meta.snapshot_at) : '—'}</span>
        </div>
      )}
      {!list.loading && !list.error && list.loaded && list.rows.length === 0 && (
        <EmptyState title="No source matches these filters.">The server searched the whole registry and found nothing for this combination.</EmptyState>
      )}

      {list.rows.length > 0 && (
        <Card padded={false} className="overflow-x-auto">
          <table className="w-full text-xs">
            <caption className="sr-only">Registered sources</caption>
            <thead>
              <tr className="border-b border-osint-border text-osint-muted uppercase tracking-wider text-left">
                <th scope="col" className="px-3 py-2">Status</th><th scope="col" className="px-3 py-2">Source</th><th scope="col" className="px-3 py-2">Collector</th><th scope="col" className="px-3 py-2">Engine · kind</th><th scope="col" className="px-3 py-2">Interval</th><th scope="col" className="px-3 py-2 text-right">Items</th><th scope="col" className="px-3 py-2">Last fetched</th><th scope="col" className="px-3 py-2">Grade</th>
              </tr>
            </thead>
            <tbody>
              {list.rows.map((s) => (
                <tr key={s.id} className="border-b border-osint-border/40 hover:bg-white/5">
                  <td className="px-3 py-2">
                    <Pill tone={statusTone(s.status)}>{s.status || 'pending'}</Pill>
                    {/* WHY it is not working, next to the fact that it is not.
                        The engine records the reason and the remedy; before
                        this they were two clicks away in a notice blob. */}
                    {s.health && !s.health.working && (
                      <div className="mt-1 text-[10px] leading-tight text-osint-muted max-w-[180px]">
                        <span className="font-mono uppercase">{s.health.state}</span>
                        {s.health.http_status ? <span className="font-mono"> · {s.health.http_status}</span> : null}
                        {s.health.credentials_required?.length ? (
                          <div className="font-mono truncate" title={s.health.credentials_required.join(', ')}>
                            needs {s.health.credentials_required.join(', ')}
                          </div>
                        ) : null}
                        {s.health.remedy && (
                          <div className="truncate" title={s.health.remedy}>{s.health.remedy}</div>
                        )}
                      </div>
                    )}
                  </td>
                  <td className="px-3 py-2 min-w-[220px]">
                    <Link to={`/console/sources/${encodeURIComponent(s.id)}`} className="text-osint-text hover:text-accent">{s.name || s.id}</Link>
                    {s.name_ja && s.name_ja !== s.name && <span className="ml-2 text-osint-muted">{s.name_ja}</span>}
                    <div className="font-mono text-[10px] text-osint-muted truncate max-w-[360px]" title={s.id}>{s.id}{s.category ? ` · ${s.category}` : ''}{s.host ? ` · ${s.host}` : ''}</div>
                  </td>
                  <td className="px-3 py-2">
                    <Link to={`/console/collectors/${encodeURIComponent(s.collector)}`} className="font-mono text-[11px] text-osint-muted hover:text-accent" title={s.collector_label || s.collector}>{s.collector}</Link>
                    {s.group && <div><Link to={`/console/collectors/${encodeURIComponent(s.group)}`} className="text-[10px] text-osint-muted hover:text-accent" title={s.group}>{s.group_label || s.group}</Link></div>}
                  </td>
                  <td className="px-3 py-2 font-mono text-[11px] text-osint-muted">{s.engine} · {s.kind}</td>
                  <td className="px-3 py-2 font-mono text-[11px] text-osint-muted">{fmtInterval(s.interval)}{s.sched?.effectiveInterval && s.sched.effectiveInterval !== s.interval ? ` → ${fmtInterval(s.sched.effectiveInterval)}` : ''}</td>
                  <td className="px-3 py-2 text-right font-mono text-neon-green">{fmtCount(s.item_count)}{s.notices > 0 && <span className="text-osint-muted" title="engine notices stored"> +{s.notices}n</span>}</td>
                  <td className="px-3 py-2 font-mono text-[11px] text-osint-muted" title={s.last_fetched || 'never'}>{s.last_fetched ? relativeTime(s.last_fetched) : 'never'}</td>
                  <td className="px-3 py-2">{s.trust?.rated ? <Pill tone={gradeTone(s.trust.grade)}>{s.trust.grade}</Pill> : <span className="text-osint-muted text-[10px]">unrated</span>}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </Card>
      )}
      {list.rows.length > 0 && <PagedFooter shown={list.rows.length} total={list.page?.total} hasMore={list.page?.has_more} busy={list.loadingMore} onMore={list.loadMore} error={list.moreError} noun="sources" />}
    </Page>
  );
}
