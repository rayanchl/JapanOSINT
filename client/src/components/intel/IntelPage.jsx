import React, { useMemo, useState } from 'react';
import { Link, useSearchParams } from 'react-router-dom';
import { LuPlay, LuRefreshCw } from 'react-icons/lu';
import { api, errorMessage } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { useIntelItems, SINCE_PRESETS, sinceIso } from '../../hooks/useIntel.js';
import { Page, Card, Pill, Button, Input, Select, Segmented, Toggle, ErrorNotice, EmptyState, LoadingState, BoundNote, SectionLabel, cx, toast } from '../ui/kit.jsx';
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
  const { data, error, loading, reload } = useApi('/api/intel/sources');
  const rows = Array.isArray(data?.data) ? data.data : [];
  const [q, setQ] = useState('');
  const [cat, setCat] = useState('');
  const [withItems, setWithItems] = useState(true);
  const [sort, setSort] = useState('items');
  const [running, setRunning] = useState({});

  const categories = useMemo(() => Array.from(new Set(rows.map((r) => r.category).filter(Boolean))).sort(), [rows]);
  const filtered = useMemo(() => {
    const needle = q.trim().toLowerCase();
    let xs = rows.filter((r) => (!withItems || r.item_count > 0)
      && (!cat || r.category === cat)
      && (!needle || [r.id, r.name, r.name_ja, r.description, r.category].some((s) => s && String(s).toLowerCase().includes(needle))));
    xs = [...xs].sort((a, b) => sort === 'items' ? (b.item_count || 0) - (a.item_count || 0)
      : sort === 'fetched' ? String(b.last_fetched || '').localeCompare(String(a.last_fetched || ''))
      : String(a.name || a.id).localeCompare(String(b.name || b.id)));
    return xs;
  }, [rows, q, cat, withItems, sort]);

  const run = async (id) => {
    setRunning((m) => ({ ...m, [id]: true }));
    try {
      const r = await api.post(`/api/intel/sources/${encodeURIComponent(id)}/run`);
      toast(`${id}: ran, ${r?.ingested ?? '?'} new rows in ${r?.duration_ms ?? '?'} ms`, { tone: 'accent', ttl: 5000 });
      reload({ silent: true });
    } catch (e) {
      toast(`${id}: ${e?.status === 409 ? 'a run is already in flight' : errorMessage(e)}`, { tone: 'danger', ttl: 5000 });
    } finally { setRunning((m) => ({ ...m, [id]: false })); }
  };

  const totalItems = rows.reduce((s, r) => s + (r.item_count || 0), 0);

  return (
    <div className="space-y-3">
      <div className="flex flex-wrap items-center gap-2">
        <Input className="flex-1 min-w-[180px]" placeholder="filter sources (id, name, 日本語, category)…" value={q} onChange={(e) => setQ(e.target.value)} />
        <Select value={cat} onChange={(e) => setCat(e.target.value)}>
          <option value="">all categories</option>
          {categories.map((c) => <option key={c} value={c}>{c}</option>)}
        </Select>
        <Segmented value={sort} onChange={setSort} options={[{ value: 'items', label: 'most items' }, { value: 'fetched', label: 'last fetched' }, { value: 'name', label: 'name' }]} />
        <Toggle on={withItems} onChange={setWithItems} label="with items only" />
        <Button size="sm" onClick={() => reload()} title="Reload"><LuRefreshCw size={12} /></Button>
      </div>
      {loading && rows.length === 0 && <LoadingState label="Loading sources…" />}
      {error && <ErrorNotice error={error} title="Could not list intel sources" onRetry={reload} />}
      {!error && data && (
        <div className="flex items-center justify-between">
          <BoundNote shown={filtered.length} total={rows.length} noun="sources" />
          <span className="font-mono text-[11px] text-osint-muted">{totalItems.toLocaleString()} items stored · registry {data.meta?.total ?? rows.length}</span>
        </div>
      )}
      {!loading && !error && rows.length > 0 && filtered.length === 0 && (
        <EmptyState title="No source matches these filters.">{withItems ? 'Sources with zero stored items are hidden — toggle "with items only".' : ''}</EmptyState>
      )}
      <ul className="grid grid-cols-1 lg:grid-cols-2 gap-2">
        {filtered.map((s) => (
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
                  {s.parent_id && <Pill title={`sub-source of ${s.parent_id}`}>sub</Pill>}
                </div>
                <div className="flex flex-wrap gap-x-3 gap-y-0.5 mt-1.5 text-[11px] font-mono">
                  <span className="text-accent">{(s.item_count || 0).toLocaleString()} items</span>
                  {s.geocoded > 0 && <span className="text-osint-muted">{s.geocoded.toLocaleString()} geocoded</span>}
                  {s.awaiting_geo > 0 && <span className="text-osint-muted">{s.awaiting_geo.toLocaleString()} awaiting geo</span>}
                  <span className="text-osint-muted" title={s.last_fetched || 'never fetched'}>fetched {relativeTime(s.last_fetched)}</span>
                  {s.last_published && <span className="text-osint-muted" title={s.last_published}>latest {relativeTime(s.last_published)}</span>}
                </div>
                {s.description && <div className="text-[11px] text-osint-muted mt-1 line-clamp-2">{s.description}</div>}
              </Link>
              {s.category !== 'breach' && (
                <button type="button" onClick={() => run(s.id)} disabled={running[s.id]} title="Run this collector now"
                  className="px-3 border-l border-osint-border text-osint-muted hover:text-accent disabled:opacity-40 rounded-r-[10px]">
                  <LuPlay size={14} className={cx(running[s.id] && 'animate-pulse')} />
                </button>
              )}
            </Card>
          </li>
        ))}
      </ul>
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
        {mode !== 'semantic' && <Segmented value={view} onChange={setView} options={[{ value: 'original', label: '原文' }, { value: 'translated', label: 'EN' }, { value: 'both', label: 'both' }]} />}
      </div>
      <form onSubmit={submit} className="flex flex-wrap gap-2">
        {mode === 'near' && (
          <>
            <Input mono className="w-[200px]" placeholder="lat,lon" value={near} onChange={(e) => setNear(e.target.value)} />
            <Input mono className="w-[110px]" type="number" min="50" step="50" placeholder="radius m" value={radius} onChange={(e) => setRadius(e.target.value)} />
          </>
        )}
        <Input className="flex-1 min-w-[200px]" placeholder={mode === 'semantic' ? 'describe what you are looking for…' : mode === 'near' ? 'optional text filter' : 'search every stored item (kanji or romaji)…'} value={draft} onChange={(e) => setDraft(e.target.value)} />
        <Button type="submit" variant="primary">Search</Button>
      </form>
      {mode === 'fts' && q && <FtsResults q={q} sort={sort} view={view} />}
      {mode === 'semantic' && q && <SemanticResults q={q} semMode={semMode} />}
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
          <BoundNote shown={items.length} total={total ?? (totalGte != null ? Math.max(totalGte, items.length) : (hasMore ? items.length + 1 : items.length))} noun={totalGte != null && total == null ? `${noun} (≥${totalGte})` : noun} />
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
  return <FeedList feed={feed} view={view} />;
}

function NearResults({ near, radius, q, view }) {
  const feed = useIntelItems('/api/intel/items', { near, radius_m: radius || undefined, q: q || undefined, limit: 200, lang_view: 'both' });
  return (
    <div className="space-y-2">
      {feed.meta?.note && <div className="text-[11px] text-accent font-mono">{feed.meta.note}</div>}
      <FeedList feed={feed} view={view} emptyTitle="Nothing stored within that radius." noun="items by distance" />
    </div>
  );
}

function SemanticResults({ q, semMode }) {
  const path = `/api/intel/semantic?q=${encodeURIComponent(q)}&mode=${semMode}&limit=50`;
  const { data, error, loading, reload } = useApi(path, { deps: [path] });
  const rows = Array.isArray(data?.data) ? data.data : [];
  const meta = data?.meta;
  const cov = error?.body?.meta?.coverage || meta?.coverage;
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
  const feed = useIntelItems('/api/intel/items', {
    since: sinceIso(since), record_type: rt.trim() || undefined, has_geom: geo ? '1' : undefined,
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
