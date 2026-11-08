import React, { useMemo, useState } from 'react';
import { Link, useSearchParams } from 'react-router-dom';
import { useRecordSearch } from '../../api/searchHistory.js';
import { LuPlay, LuRefreshCw } from 'react-icons/lu';
import { api, errorMessage } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { useIntelItems, SINCE_PRESETS, useSinceIso } from '../../hooks/useIntel.js';
import { Page, Card, Pill, Button, Input, Select, Segmented, Toggle, ErrorNotice, EmptyState, LoadingState, BoundNote, PagedFooter, SectionLabel, cx, toast } from '../ui/kit.jsx';
import { usePagedList } from '../../hooks/usePagedList.js';
import { useAuthOptional } from '../../auth/AuthContext.jsx';
import { searchPath } from '../../utils/sourceFormat.js';
import IntelItemRow from './IntelItemRow.jsx';
import { relativeTime } from '../../utils/time.js';

/** Intel tab — Sources · Search · Recent (iOS `IntelTab` + `CrossSourceSearchView`). */
export default function IntelPage() {
  const [params, setParams] = useSearchParams();
  const view = params.get('view') || 'sources';
  const setView = (v) => { const p = new URLSearchParams(params); p.set('view', v); setParams(p, { replace: true }); };
  return (
    <Page title="Intel" subtitle="Everything the collectors stored, per source and across all of them." wide
      actions={<Segmented value={view} onChange={setView} size="md" options={[{ value: 'sources', label: 'Sources' }, { value: 'search', label: 'Search' }, { value: 'recent', label: 'Recent' }]} />}>
      {view === 'sources' && <SourcesView />}
      {view === 'search' && <SearchView />}
      {view === 'recent' && <RecentView />}
    </Page>
  );
}

/* ---------------------------------------------------------------- sources */

function SourcesView() {
  /* Searched and paged on the SERVER (GET /api/sources/search), 2026-11-07.
   * This used to download GET /api/intel/sources — the whole registry with
   * aggregates, tens of MB — and filter it in the browser to show a few
   * cards. The search route takes q / category / has_items / sort and
   * answers its own facets and a measured total. What it does not carry,
   * compared with the old catalogue: `awaiting_geo`, `last_published` and
   * `parent_id` per row, and breach-catalogue entries (they are not
   * registry sources); those cards say less now rather than guess. */
  const auth = useAuthOptional();
  const canRun = auth?.isPlatformAdmin === true || Boolean(auth?.canManageWorkspace);
  const [draft, setDraft] = useState('');
  const [q, setQ] = useState('');
  const [cat, setCat] = useState('');
  const [withItems, setWithItems] = useState(true);
  const [sort, setSort] = useState('items');
  const [running, setRunning] = useState({});
  const path = useMemo(() => searchPath({ q, category: cat, has_items: withItems, sort, desc: sort !== 'name' }), [q, cat, withItems, sort]);
  const list = usePagedList(path, { pageSize: 50 });
  const rows = list.rows;
  const categories = list.meta?.facets?.categories;

  const run = async (id) => {
    setRunning((m) => ({ ...m, [id]: true }));
    try {
      const r = await api.post(`/api/intel/sources/${encodeURIComponent(id)}/run`);
      toast(`${id}: ran, ${r?.ingested ?? '?'} new rows in ${r?.duration_ms ?? '?'} ms`, { tone: 'accent', ttl: 5000 });
      list.reload({ silent: true });
    } catch (e) {
      toast(`${id}: ${e?.status === 409 ? 'a run is already in flight' : errorMessage(e)}`, { tone: 'danger', ttl: 5000 });
    } finally { setRunning((m) => ({ ...m, [id]: false })); }
  };

  return (
    <div className="space-y-3">
      <form onSubmit={(e) => { e.preventDefault(); setQ(draft.trim()); }} className="flex flex-wrap items-center gap-2">
        <Input className="flex-1 min-w-[180px]" placeholder="search sources (id, name, 日本語, description, collector, host)…" value={draft} onChange={(e) => setDraft(e.target.value)} />
        <Button type="submit">Search</Button>
        <Select value={cat} onChange={(e) => setCat(e.target.value)} title="category">
          <option value="">all categories{categories?.total > categories?.shown ? ` (top ${categories.shown} of ${categories.total})` : ''}</option>
          {cat && !(categories?.values || []).some((v) => v.key === cat) && <option value={cat}>{cat}</option>}
          {(categories?.values || []).map((c) => <option key={c.key} value={c.key}>{c.key} ({c.count?.toLocaleString?.() ?? c.count})</option>)}
        </Select>
        <Segmented value={sort} onChange={setSort} options={[{ value: 'items', label: 'most items' }, { value: 'fetched', label: 'last fetched' }, { value: 'name', label: 'name' }]} />
        <Toggle on={withItems} onChange={setWithItems} label="with items only" />
        <Button size="sm" onClick={() => list.reload()} title="Reload"><LuRefreshCw size={12} /></Button>
      </form>
      {list.loading && rows.length === 0 && <LoadingState label="Searching sources…" />}
      {list.error && <ErrorNotice error={list.error} title="Could not search sources" onRetry={list.reload} />}
      {!list.error && list.loaded && (
        <div className="flex items-center justify-between">
          <BoundNote shown={rows.length} total={list.total} more={list.hasMore} noun="sources" />
          <span className="font-mono text-[11px] text-osint-muted">registry {list.meta?.registry_total?.toLocaleString?.() ?? '—'}</span>
        </div>
      )}
      {!list.loading && !list.error && list.loaded && rows.length === 0 && (
        <EmptyState title="No source matches these filters.">{withItems ? 'Sources with zero stored items are hidden — toggle "with items only".' : 'The server searched the whole registry and found nothing.'}</EmptyState>
      )}
      <ul className="grid grid-cols-1 lg:grid-cols-2 gap-2">
        {rows.map((s) => (
          <li key={s.id}>
            <Card padded={false} className="flex items-stretch">
              <Link to={`/intel/sources/${encodeURIComponent(s.id)}`} className="flex-1 min-w-0 p-3 hover:bg-white/5 rounded-l-[10px]">
                <div className="flex items-center gap-2 min-w-0">
                  <span className="text-sm text-osint-text truncate">{s.name || s.id}</span>
                  {s.name_ja && s.name_ja !== s.name && <span className="text-xs text-osint-muted truncate">{s.name_ja}</span>}
                </div>
                <div className="flex flex-wrap items-center gap-1.5 mt-1 text-[10px] font-mono text-osint-muted">
                  <span className="truncate max-w-[200px]">{s.id}</span>
                  {s.category && <Pill>{s.category}</Pill>}
                  {s.category === 'breach' && <Pill tone="danger">operator-gated</Pill>}
                </div>
                <div className="flex flex-wrap gap-x-3 gap-y-0.5 mt-1.5 text-[11px] font-mono">
                  <span className="text-accent">{(s.item_count || 0).toLocaleString()} items</span>
                  {s.geocoded > 0 && <span className="text-osint-muted">{s.geocoded.toLocaleString()} geocoded</span>}
                  <span className="text-osint-muted" title={s.last_fetched || 'never fetched'}>fetched {relativeTime(s.last_fetched)}</span>
                </div>
                {s.description && <div className="text-[11px] text-osint-muted mt-1 line-clamp-2">{s.description}</div>}
              </Link>
              {s.category !== 'breach' && s.kind !== 'internal' && canRun && (
                <button type="button" onClick={() => run(s.id)} disabled={running[s.id]} title="Run this collector now"
                  className="px-3 border-l border-osint-border text-osint-muted hover:text-accent disabled:opacity-40 rounded-r-[10px]">
                  <LuPlay size={14} className={cx(running[s.id] && 'animate-pulse')} />
                </button>
              )}
            </Card>
          </li>
        ))}
      </ul>
      {rows.length > 0 && <PagedFooter shown={rows.length} total={list.total} hasMore={list.hasMore} busy={list.loadingMore} onMore={list.loadMore} error={list.moreError} noun="sources" />}
    </div>
  );
}

/* ----------------------------------------------------------------- search */

function SearchView() {
  const [params, setParams] = useSearchParams();
  const mode = params.get('mode') || 'fts';
  const q = params.get('q') || '';
  const [draft, setDraft] = useState(q);
  const [sort, setSort] = useState('relevance');
  const [semMode, setSemMode] = useState('hybrid');
  const [near, setNear] = useState(params.get('near') || '');
  const [radius, setRadius] = useState(params.get('radius_m') || '2000');
  const [view, setView] = useState('original');
  // Semantic filters (S1): source / record_type / since run INSIDE the KNN on
  // the server, bbox is a post-filter whose drops the server counts.
  const [semSource, setSemSource] = useState('');
  const [semType, setSemType] = useState('');
  const [semSince, setSemSince] = useState('');
  const [semBbox, setSemBbox] = useState('');

  const commit = (patch) => {
    const p = new URLSearchParams(params);
    for (const [k, v] of Object.entries(patch)) { if (v) p.set(k, v); else p.delete(k); }
    setParams(p, { replace: true });
  };

  const submit = (e) => {
    e?.preventDefault();
    if (mode === 'near') commit({ near: near.trim(), radius_m: radius, q: draft.trim() });
    else commit({ q: draft.trim() });
  };

  return (
    <div className="space-y-3">
      <div className="flex flex-wrap items-center gap-2">
        <Segmented value={mode} onChange={(m) => commit({ mode: m })} size="md"
          options={[{ value: 'fts', label: 'Full-text' }, { value: 'semantic', label: 'Semantic' }, { value: 'near', label: 'Nearby' }]} />
        {mode === 'fts' && <Segmented value={sort} onChange={setSort} options={[{ value: 'relevance', label: 'relevance' }, { value: 'trust', label: 'trust' }]} />}
        {mode === 'semantic' && <Segmented value={semMode} onChange={setSemMode} options={[{ value: 'hybrid', label: 'hybrid' }, { value: 'vector', label: 'vector only' }]} />}
        {/* Full-text and Nearby: both routes apply lang_view on the server
          * (httpd.c intel_lang_view), so rows carry their stored translation. */}
        {mode !== 'semantic' && <Segmented value={view} onChange={setView} options={[{ value: 'original', label: '原文' }, { value: 'translated', label: 'EN' }, { value: 'both', label: 'both' }]} />}
      </div>
      {mode === 'semantic' && (
        <div className="flex flex-wrap items-center gap-2">
          <SectionLabel>filters</SectionLabel>
          <Input mono className="w-[220px]" placeholder="source ids, comma-separated" value={semSource} onChange={(e) => setSemSource(e.target.value)} />
          <Input mono className="w-[150px]" placeholder="record_type" value={semType} onChange={(e) => setSemType(e.target.value)} />
          <Segmented value={semSince} onChange={setSemSince} options={SINCE_PRESETS} />
          <Input mono className="w-[200px]" placeholder="bbox w,s,e,n" value={semBbox} onChange={(e) => setSemBbox(e.target.value)} />
        </div>
      )}
      <form onSubmit={submit} className="flex flex-wrap gap-2">
        {mode === 'near' && (
          <>
            <Input mono className="w-[200px]" placeholder="lat,lon" value={near} onChange={(e) => setNear(e.target.value)} />
            <Input mono className="w-[110px]" type="number" min="50" step="50" placeholder="radius m" value={radius} onChange={(e) => setRadius(e.target.value)} />
          </>
        )}
        {/* In Nearby mode q RANKS the radius by similarity when the server has
          * an embedding index (nearapi.c, meta.q_mode "semantic"), and falls
          * back to the full-text filter without one (meta.q_mode "fts"). */}
        <Input className="flex-1 min-w-[200px]" placeholder={mode === 'semantic' ? 'describe what you are looking for…' : mode === 'near' ? 'optional: rank the radius by meaning (kanji or romaji)…' : 'search every stored item (kanji or romaji)…'} value={draft} onChange={(e) => setDraft(e.target.value)} />
        <Button type="submit" variant="primary">Search</Button>
      </form>
      {mode === 'fts' && q && <FtsResults q={q} sort={sort} view={view} />}
      {mode === 'semantic' && q && <SemanticResults q={q} semMode={semMode} filters={{ source: semSource, record_type: semType, since: semSince, bbox: semBbox }} />}
      {mode === 'near' && params.get('near') && <NearResults near={params.get('near')} radius={params.get('radius_m')} q={q} view={view} />}
      {!q && mode !== 'near' && <EmptyState title="Enter a query.">{mode === 'semantic' ? 'Semantic search needs the embedding pod; the server says so if it is not running.' : 'Full-text search runs over titles, summaries and bodies across every source, with Japanese normalisation.'}</EmptyState>}
    </div>
  );
}

function FeedList({ feed, view, emptyTitle = 'No items matched.', noun = 'items' }) {
  const { items, error, loading, loadingMore, hasMore, loadMore, reload, total, totalGte, meta } = feed;
  return (
    <div className="space-y-2">
      {loading && items.length === 0 && <LoadingState />}
      {error && <ErrorNotice error={error} title="Feed request failed" onRetry={reload} />}
      {!loading && !error && (
        <div className="flex flex-wrap items-center gap-3">
          <BoundNote shown={items.length} total={total ?? (totalGte == null && !hasMore ? items.length : null)} atLeast={totalGte} more={hasMore} noun={noun} />
          {meta?.q_applied === false && <Pill tone="danger">query NOT applied by the server</Pill>}
          {meta?.collapsed > 0 && <span className="font-mono text-[11px] text-osint-muted">{meta.collapsed} near-duplicates collapsed</span>}
          {meta?.rerank?.bounded && <span className="font-mono text-[11px] text-accent" title={meta.rerank.formula}>reranked within a window of {meta.rerank.window}</span>}
          {Array.isArray(meta?.notes) && meta.notes.map((n, i) => <Pill key={i} tone="warning">{typeof n === 'string' ? n : JSON.stringify(n)}</Pill>)}
        </div>
      )}
      {!loading && !error && items.length === 0 && <EmptyState title={emptyTitle} />}
      {items.map((it) => <IntelItemRow key={it.uid} item={it} view={view} />)}
      {hasMore && <div className="flex justify-center"><Button busy={loadingMore} onClick={loadMore}>Load more</Button></div>}
    </div>
  );
}

function FtsResults({ q, sort, view }) {
  const feed = useIntelItems('/api/intel/search', { q, sort, total: '1', collapse: '1', limit: 50, lang_view: 'both' });
  // Recorded once the first page is in, with the server's total — the search
  // was committed when q reached the URL (submit, a link, a history re-run).
  useRecordSearch('intel', { view: 'search', mode: 'fts', q }, {
    ready: !feed.loading && !feed.error, resultCount: feed.total ?? feed.items?.length,
  });
  return <FeedList feed={feed} view={view} />;
}

export function nearParams({ near, radius, q }) {
  return { near, radius_m: radius || undefined, q: q || undefined, limit: 200, lang_view: 'both' };
}

function NearResults({ near, radius, q, view }) {
  const feed = useIntelItems('/api/intel/items', nearParams({ near, radius, q }));
  useRecordSearch('intel', { view: 'search', mode: 'near', near, radius_m: radius, q }, {
    ready: !feed.loading && !feed.error, resultCount: feed.total ?? feed.items?.length,
  });
  return (
    <div className="space-y-2">
      {feed.meta?.note && <div className="text-[11px] text-accent font-mono">{feed.meta.note}</div>}
      {q && feed.meta?.q_mode === 'semantic' && (
        <div className="text-[11px] font-mono text-osint-muted" title={feed.meta.semantic?.note}>
          ranked by similarity to “{q}” · {feed.meta.semantic?.ranked ?? 0} ranked
          {feed.meta.semantic?.unembedded > 0 && ` · ${feed.meta.semantic.unembedded} without a vector (listed after, by distance)`}
          {feed.meta.semantic?.beyond_cap > 0 && ` · ${feed.meta.semantic.beyond_cap} past the ${feed.meta.semantic.cap}-row ranking cap`}
        </div>
      )}
      {q && feed.meta?.q_mode === 'fts' && (
        <div className="text-[11px] font-mono text-osint-muted">text filter (no embedding index{feed.meta.semantic_unavailable?.error ? `: ${feed.meta.semantic_unavailable.error}` : ''})</div>
      )}
      <FeedList feed={feed} view={view} emptyTitle={q ? 'Nothing within that radius matched the text filter.' : 'Nothing stored within that radius.'} noun={feed.meta?.q_mode === 'semantic' ? 'items by similarity' : 'items by distance'} />
    </div>
  );
}

/** /api/intel/semantic URL with the S1 filters; empty ones are not sent.
 *  `since` is an ISO instant (already anchored by useSinceIso). */
export function semanticPath({ q, semMode, source, record_type: rt, since, bbox }) {
  const p = new URLSearchParams({ q, mode: semMode || 'hybrid', limit: '50' });
  const src = String(source || '').split(',').map((x) => x.trim()).filter(Boolean).join(',');
  if (src) p.set('source', src);
  if (rt?.trim()) p.set('record_type', rt.trim());
  if (since) p.set('since', since);
  if (bbox?.trim()) p.set('bbox', bbox.replace(/\s+/g, ''));
  return `/api/intel/semantic?${p.toString()}`;
}

function SemanticResults({ q, semMode, filters = {} }) {
  // The since preset is anchored once per preset change (useSinceIso), so the
  // request key does not move every render.
  const sinceAt = useSinceIso(filters.since);
  const path = semanticPath({ q, semMode, ...filters, since: sinceAt });
  const { data, error, loading, reload } = useApi(path, { deps: [path] });
  const rows = Array.isArray(data?.data) ? data.data : [];
  const meta = data?.meta;
  const cov = error?.body?.meta?.coverage || meta?.coverage;
  useRecordSearch('intel', { view: 'search', mode: 'semantic', q }, {
    ready: !loading && !error && Boolean(data), resultCount: rows.length,
  });
  return (
    <div className="space-y-2">
      {loading && <LoadingState label="Embedding the query…" />}
      {error && (
        <ErrorNotice error={error} title={error.status === 503 ? 'Semantic search is unavailable on this server' : 'Semantic search failed'} onRetry={reload}>
          {error.status === 503 && 'The embedding pod is not configured or has not built an index yet. This is not an empty result.'}
          {cov && <div className="font-mono mt-1 text-[10px]">coverage: {JSON.stringify(cov)}</div>}
        </ErrorNotice>
      )}
      {meta && (
        <div className="flex flex-wrap gap-2 items-center">
          <BoundNote shown={meta.shown} total={meta.total} noun="semantic hits" />
          <span className="font-mono text-[11px] text-osint-muted">{meta.mode} · vector {meta.vector_hits}{meta.fts_hits != null ? ` · fts ${meta.fts_hits}` : ''} · {meta.embed_ms} ms embed · {meta.total_ms} ms</span>
          {meta.tenant_withheld > 0 && <Pill tone="warning">{meta.tenant_withheld} withheld (other tenants)</Pill>}
          {meta.filters?.in_knn?.length > 0 && <span className="font-mono text-[11px] text-osint-muted">in KNN: {meta.filters.in_knn.join(', ')}</span>}
          {meta.filters?.dropped_total > 0 && (
            <Pill tone="warning" title={meta.filters.post_note}>
              {meta.filters.dropped_total} of the k nearest dropped by {meta.filters.post?.join(', ') || 'post-filters'}
            </Pill>
          )}
        </div>
      )}
      {!loading && !error && data && rows.length === 0 && <EmptyState title="No semantic match." />}
      {rows.map((it) => <IntelItemRow key={it.uid} item={it} />)}
    </div>
  );
}

/* ----------------------------------------------------------------- recent */

function RecentView() {
  const [since, setSince] = useState('24h');
  const [rt, setRt] = useState('');
  const [geo, setGeo] = useState(false);
  const [lang, setLang] = useState('');
  const [view, setView] = useState('original');
  const sinceAt = useSinceIso(since);
  // has_geom takes yes|no on the server (intelapi.c); '1' matched neither and
  // the "geolocated only" toggle silently filtered nothing.
  const feed = useIntelItems('/api/intel/items', {
    since: sinceAt, record_type: rt.trim() || undefined, has_geom: geo ? 'yes' : undefined,
    lang: lang || undefined, limit: 50, total: '1', lang_view: 'both',
  });
  return (
    <div className="space-y-3">
      <div className="flex flex-wrap items-center gap-2">
        <SectionLabel>since</SectionLabel>
        <Segmented value={since} onChange={setSince} options={SINCE_PRESETS} />
        <Input className="w-[150px]" placeholder="record_type" value={rt} onChange={(e) => setRt(e.target.value)} />
        <Select value={lang} onChange={(e) => setLang(e.target.value)}><option value="">any language</option><option value="ja">ja</option><option value="en">en</option></Select>
        <Toggle on={geo} onChange={setGeo} label="geolocated only" />
        <Segmented value={view} onChange={setView} options={[{ value: 'original', label: '原文' }, { value: 'translated', label: 'EN' }, { value: 'both', label: 'both' }]} />
      </div>
      <FeedList feed={feed} view={view} emptyTitle="Nothing stored in this window." />
    </div>
  );
}
