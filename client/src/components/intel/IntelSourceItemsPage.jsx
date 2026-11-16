import React, { useMemo, useState } from 'react';
import { useSearchParams, Link, useParams } from 'react-router-dom';
import { LuPlay } from 'react-icons/lu';
import { api, errorMessage } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { useIntelItems, SINCE_PRESETS, useSinceIso } from '../../hooks/useIntel.js';
import { useRecordSearch } from '../../api/searchHistory.js';
import { useAuthOptional } from '../../auth/AuthContext.jsx';
import { downloadExport } from '../../hooks/useCases.js';
import { Page, Card, Pill, Button, Input, Segmented, Toggle, ErrorNotice, EmptyState, LoadingState, BoundNote, KV, cx, toast } from '../ui/kit.jsx';
import IntelItemRow from './IntelItemRow.jsx';
import { isSafeUrl } from '../../utils/safeUrl.js';
import { relativeTime, fmtAbs } from '../../utils/time.js';

/** Items of one source — iOS `IntelSourceItemsView`. */
export default function IntelSourceItemsPage() {
  const { id } = useParams();
  /* One source's header from GET /api/sources/:id/detail (2026-11-07). It
   * used to download the whole GET /api/intel/sources catalogue — every
   * registered source with its aggregates — to read one row's name. The
   * detail route answers 404 for an id the registry does not hold (a breach
   * catalogue entry, a retired source whose items remain), which is said as
   * such below rather than as a failure. The catalogue's `ttl_ms` has no
   * counterpart in the detail body and is no longer shown. */
  const detail = useApi(`/api/sources/${encodeURIComponent(id)}/detail`, { deps: [id] });
  const notRegistered = detail.error?.status === 404;
  const source = useMemo(() => {
    const d = detail.data;
    if (!d?.registry) return null;
    const r = d.registry, x = d.data || {};
    return {
      id: r.id, name: r.name, name_ja: r.name_ja, description: r.description, category: r.category, url: r.url, kind: r.kind,
      item_count: x.item_count, geocoded: x.geocoded, awaiting_geo: x.awaiting_geo, last_fetched: x.last_fetched, last_published: x.last_published,
    };
  }, [detail.data]);
  const auth = useAuthOptional();
  /* Same gate as the server's /run route: platform operator, or owner/admin
   * of the active workspace. */
  const canRun = auth?.isPlatformAdmin === true || Boolean(auth?.canManageWorkspace);
  // ?q= reopens a search made here (a history or shared link).
  const [urlParams] = useSearchParams();
  const [q, setQ] = useState(() => urlParams.get('q') || '');
  const [qApplied, setQApplied] = useState(() => (urlParams.get('q') || '').trim());
  const [since, setSince] = useState('');
  const [rt, setRt] = useState('');
  const [geo, setGeo] = useState(false);
  const [sort, setSort] = useState('newest');
  const [view, setView] = useState('original');
  const [running, setRunning] = useState(false);

  // Pinned per preset choice — sinceIso() per render refetched forever.
  const sinceAt = useSinceIso(since);
  const feed = useIntelItems('/api/intel/items', {
    source: id, q: qApplied || undefined, since: sinceAt, record_type: rt.trim() || undefined,
    has_geom: geo ? 'yes' : undefined, sort: qApplied && sort !== 'newest' ? sort : undefined,
    limit: 50, total: '1', lang_view: 'both',
  });
  // A search submitted inside this source goes to the workspace's history once
  // its first page is in. The filters beside it narrow a list; only q is a search.
  useRecordSearch('intel', { view: 'source', source: id, q: qApplied }, {
    enabled: Boolean(qApplied), ready: !feed.loading && !feed.error, resultCount: feed.total ?? feed.items?.length,
  });
  const { items, error, loading, loadingMore, hasMore, loadMore, reload, total, totalGte, meta } = feed;

  /* GET /api/export/intel (or /breach for a breach source, which the server
   * gates on the platform operator and requires ?source= for) with the SAME
   * filters as the list on screen; the export walks every matching row up to
   * the plan's row cap and says in the file when it stopped. No UI reached
   * the export routes other than the case list until 2026-11-07. */
  const [exporting, setExporting] = useState(false);
  const exportItems = async (format) => {
    setExporting(true);
    try {
      const kind = source?.category === 'breach' ? 'breach' : 'intel';
      const query = { source: id, q: qApplied || undefined, since: sinceAt, record_type: rt.trim() || undefined, has_geom: geo ? 'yes' : undefined };
      if (kind === 'intel' && qApplied && sort === 'relevance') query.sort = 'relevance';
      await downloadExport(kind, query, format);
      toast('Export downloaded', { tone: 'accent' });
    } catch (e) { toast(`Export refused — ${errorMessage(e)}`, { tone: 'danger', ttl: 6000 }); }
    finally { setExporting(false); }
  };

  const run = async () => {
    setRunning(true);
    try {
      const r = await api.post(`/api/intel/sources/${encodeURIComponent(id)}/run`);
      toast(`ran: ${r?.ingested ?? '?'} new rows in ${r?.duration_ms ?? '?'} ms`, { tone: 'accent', ttl: 5000 });
      reload(); detail.reload({ silent: true, query: { fresh: 1 } });
    } catch (e) { toast(e?.status === 409 ? 'a run is already in flight' : errorMessage(e), { tone: 'danger', ttl: 5000 }); }
    finally { setRunning(false); }
  };

  const isBreach = source?.category === 'breach';

  return (
    <Page wide
      title={<span className="flex items-center gap-2 flex-wrap">{source?.name || id}{source?.name_ja && source.name_ja !== source.name && <span className="text-sm text-osint-muted font-normal">{source.name_ja}</span>}</span>}
      subtitle={<span><Link to="/intel?view=sources" className="hover:text-accent">← all sources</Link> · <span className="font-mono">{id}</span> · <Link to={`/console/sources/${encodeURIComponent(id)}`} className="text-accent hover:underline">how this source works →</Link>{source?.description ? ` — ${source.description}` : ''}</span>}
      actions={!isBreach && source && source.kind !== 'internal' && canRun && <Button variant="primary" busy={running} onClick={run}><LuPlay size={13} /> Run now</Button>}
    >
      {detail.error && !notRegistered && <ErrorNotice error={detail.error} title="Could not load source metadata" onRetry={detail.reload} />}
      {notRegistered && <div className="text-[11px] text-osint-muted">No registered source has this id (a breach-catalogue entry or a retired source); its stored items are listed below.</div>}
      {source && (
        <Card>
          <KV pairs={[
            ['stored items', (source.item_count || 0).toLocaleString()],
            ['geocoded', source.geocoded], ['awaiting geo', source.awaiting_geo || null],
            ['last fetched', source.last_fetched ? `${fmtAbs(source.last_fetched)} (${relativeTime(source.last_fetched)})` : 'never'],
            ['latest record', source.last_published ? fmtAbs(source.last_published) : null],
            ['category', source.category],
            ['upstream', source.url && isSafeUrl(source.url) ? <a href={source.url} target="_blank" rel="noreferrer" className="text-accent hover:underline">{source.url}</a> : source.url],
          ]} />
          {isBreach && <div className="mt-2 text-[11px] text-accent">Breach corpus: reading records requires the platform-operator role; the server answers 403 otherwise.</div>}
        </Card>
      )}

      <form onSubmit={(e) => { e.preventDefault(); setQApplied(q.trim()); }} className="flex flex-wrap items-center gap-2">
        <Input className="flex-1 min-w-[180px]" placeholder="search within this source…" value={q} onChange={(e) => setQ(e.target.value)} />
        <Button type="submit">Search</Button>
        <Segmented value={since} onChange={setSince} options={SINCE_PRESETS} />
        <Input className="w-[140px]" placeholder="record_type" value={rt} onChange={(e) => setRt(e.target.value)} />
        <Toggle on={geo} onChange={setGeo} label="geolocated" />
        {qApplied && <Segmented value={sort} onChange={setSort} options={[{ value: 'newest', label: 'newest' }, { value: 'relevance', label: 'relevance' }, { value: 'trust', label: 'trust' }, { value: 'hybrid', label: 'hybrid' }]} />}
        <Segmented value={view} onChange={setView} options={[{ value: 'original', label: '原文' }, { value: 'translated', label: 'EN' }, { value: 'both', label: 'both' }]} />
        <Button size="sm" busy={exporting} onClick={() => exportItems('csv')} title="Export every item matching these filters (GET /api/export)">Export CSV</Button>
        <Button size="sm" busy={exporting} onClick={() => exportItems('json')}>JSON</Button>
      </form>

      {loading && items.length === 0 && <LoadingState />}
      {error && <ErrorNotice error={error} title={error.status === 403 ? 'Access to this source is operator-gated' : 'Could not load items'} onRetry={reload} />}
      {!loading && !error && (
        <div className="flex flex-wrap items-center gap-3">
          <BoundNote shown={items.length} total={total ?? (totalGte == null && !hasMore ? items.length : null)} atLeast={totalGte} more={hasMore} noun="items" />
          {meta?.q_applied === false && <Pill tone="danger">query NOT applied by the server</Pill>}
          {/* sort=hybrid (semsearchapi.c): BM25 and the vector KNN fused; its
            * total is the fused top-k of each arm, which the server says. */}
          {sort === 'hybrid' && meta?.hybrid && <span className="font-mono text-[10px] text-accent" title={meta.hybrid.note}>hybrid: fused top-k of full-text and vector arms{meta.hybrid.bounded ? ' (bounded — raise k to widen)' : ''}{meta.hybrid.vector_arm_error ? ' · vector arm failed, full-text only' : ''}</span>}
          {meta?.filters && <span className="font-mono text-[10px] text-osint-muted truncate max-w-full" title={JSON.stringify(meta.filters)}>filters echoed by server</span>}
        </div>
      )}
      {!loading && !error && items.length === 0 && (
        <EmptyState title="No stored items match.">{source && source.item_count > 0 ? 'The source has items outside these filters.' : 'This source has stored nothing yet — run it, or check its schedule under Console → Sources.'}</EmptyState>
      )}
      <div className={cx('space-y-2')}>
        {items.map((it) => <IntelItemRow key={it.uid} item={it} showSource={false} view={view} />)}
      </div>
      {hasMore && <div className="flex justify-center"><Button busy={loadingMore} onClick={loadMore}>Load more</Button></div>}
    </Page>
  );
}
