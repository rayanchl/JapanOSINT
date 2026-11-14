import React, { useMemo, useState } from 'react';
import { useSearchParams, Link, useParams } from 'react-router-dom';
import RunControl from '../sources/RunControl.jsx';
import { useApi } from '../../hooks/useApi.js';
import { useIntelItems, SINCE_PRESETS, useSinceIso } from '../../hooks/useIntel.js';
import { useRecordSearch } from '../../api/searchHistory.js';
import { Page, Card, Pill, Button, Input, Segmented, Toggle, ErrorNotice, EmptyState, LoadingState, BoundNote, KV, cx, toast } from '../ui/kit.jsx';
import IntelItemRow from './IntelItemRow.jsx';
import { isSafeUrl } from '../../utils/safeUrl.js';
import { relativeTime, fmtAbs } from '../../utils/time.js';

/** Items of one source — iOS `IntelSourceItemsView`. */
export default function IntelSourceItemsPage() {
  const { id } = useParams();
  // ONE source's metadata: the detail route, not the whole registry joined to
  // its aggregates (which this page used to download to find() a single row).
  const sources = useApi(`/api/sources/${encodeURIComponent(id)}/detail`);
  const source = useMemo(() => {
    const d = sources.data;
    if (!d?.registry) return null;
    return {
      id: d.registry.id, name: d.registry.name, name_ja: d.registry.name_ja, description: d.registry.description,
      category: d.registry.category, kind: d.registry.kind, url: d.registry.url,
      item_count: d.data?.item_count, geocoded: d.data?.geocoded, awaiting_geo: d.data?.awaiting_geo,
      last_fetched: d.data?.last_fetched, last_published: d.data?.last_published,
    };
  }, [sources.data]);
  // ?q= reopens a search made here (a history or shared link).
  const [urlParams] = useSearchParams();
  const [q, setQ] = useState(() => urlParams.get('q') || '');
  const [qApplied, setQApplied] = useState(() => (urlParams.get('q') || '').trim());
  const [since, setSince] = useState('');
  const [rt, setRt] = useState('');
  const [geo, setGeo] = useState(false);
  const [sort, setSort] = useState('newest');
  const [view, setView] = useState('original');

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

  const isBreach = source?.category === 'breach';

  return (
    <Page wide
      title={<span className="flex items-center gap-2 flex-wrap">{source?.name || id}{source?.name_ja && source.name_ja !== source.name && <span className="text-sm text-osint-muted font-normal">{source.name_ja}</span>}</span>}
      subtitle={<span><Link to="/intel?view=sources" className="hover:text-accent">← all sources</Link> · <span className="font-mono">{id}</span> · <Link to={`/console/sources/${encodeURIComponent(id)}`} className="text-accent hover:underline">how this source works →</Link>{source?.description ? ` — ${source.description}` : ''}</span>}
      actions={!isBreach && source && <RunControl source={source} onDone={() => { reload(); sources.reload({ silent: true }); }} />}
    >
      {sources.error && sources.error.status !== 404 && <ErrorNotice error={sources.error} title="Could not load source metadata" onRetry={sources.reload} />}
      {source && (
        <Card>
          <KV pairs={[
            ['stored items', (source.item_count || 0).toLocaleString()],
            ['geocoded', source.geocoded], ['awaiting geo', source.awaiting_geo || null],
            ['last fetched', source.last_fetched ? `${fmtAbs(source.last_fetched)} (${relativeTime(source.last_fetched)})` : 'never'],
            ['latest record', source.last_published ? fmtAbs(source.last_published) : null],
            ['category', source.category], ['ttl', source.ttl_ms ? `${Math.round(source.ttl_ms / 60000)} min` : null],
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
        {qApplied && <Segmented value={sort} onChange={setSort} options={[{ value: 'newest', label: 'newest' }, { value: 'relevance', label: 'relevance' }, { value: 'trust', label: 'trust' }]} />}
        <Segmented value={view} onChange={setView} options={[{ value: 'original', label: '原文' }, { value: 'translated', label: 'EN' }, { value: 'both', label: 'both' }]} />
      </form>

      {loading && items.length === 0 && <LoadingState />}
      {error && <ErrorNotice error={error} title={error.status === 403 ? 'Access to this source is operator-gated' : 'Could not load items'} onRetry={reload} />}
      {!loading && !error && (
        <div className="flex flex-wrap items-center gap-3">
          <BoundNote shown={items.length} total={total ?? (totalGte == null && !hasMore ? items.length : null)} atLeast={totalGte} more={hasMore} noun="items" />
          {meta?.q_applied === false && <Pill tone="danger">query NOT applied by the server</Pill>}
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
