import React, { useMemo, useState } from 'react';
import { Link, useParams } from 'react-router-dom';
import { LuPlay } from 'react-icons/lu';
import { api, errorMessage } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { useIntelItems, SINCE_PRESETS, sinceIso } from '../../hooks/useIntel.js';
import { Page, Card, Pill, Button, Input, Segmented, Toggle, ErrorNotice, EmptyState, LoadingState, BoundNote, KV, cx, toast } from '../ui/kit.jsx';
import IntelItemRow from './IntelItemRow.jsx';
import { isSafeUrl } from '../../utils/safeUrl.js';
import { relativeTime, fmtAbs } from '../../utils/time.js';

/** Items of one source — iOS `IntelSourceItemsView`. */
export default function IntelSourceItemsPage() {
  const { id } = useParams();
  const sources = useApi('/api/intel/sources');
  const source = useMemo(() => (Array.isArray(sources.data?.data) ? sources.data.data.find((s) => s.id === id) : null), [sources.data, id]);
  const [q, setQ] = useState('');
  const [qApplied, setQApplied] = useState('');
  const [since, setSince] = useState('');
  const [rt, setRt] = useState('');
  const [geo, setGeo] = useState(false);
  const [sort, setSort] = useState('newest');
  const [view, setView] = useState('original');
  const [running, setRunning] = useState(false);

  const feed = useIntelItems('/api/intel/items', {
    source: id, q: qApplied || undefined, since: sinceIso(since), record_type: rt.trim() || undefined,
    has_geom: geo ? '1' : undefined, sort: qApplied && sort !== 'newest' ? sort : undefined,
    limit: 50, total: '1', lang_view: 'both',
  });
  const { items, error, loading, loadingMore, hasMore, loadMore, reload, total, totalGte, meta } = feed;

  const run = async () => {
    setRunning(true);
    try {
      const r = await api.post(`/api/intel/sources/${encodeURIComponent(id)}/run`);
      toast(`ran: ${r?.ingested ?? '?'} new rows in ${r?.duration_ms ?? '?'} ms`, { tone: 'accent', ttl: 5000 });
      reload(); sources.reload({ silent: true });
    } catch (e) { toast(e?.status === 409 ? 'a run is already in flight' : errorMessage(e), { tone: 'danger', ttl: 5000 }); }
    finally { setRunning(false); }
  };

  const isBreach = source?.category === 'breach';

  return (
    <Page wide
      title={<span className="flex items-center gap-2 flex-wrap">{source?.name || id}{source?.name_ja && source.name_ja !== source.name && <span className="text-sm text-osint-muted font-normal">{source.name_ja}</span>}</span>}
      subtitle={<span><Link to="/intel?view=sources" className="hover:text-accent">← all sources</Link> · <span className="font-mono">{id}</span>{source?.description ? ` — ${source.description}` : ''}</span>}
      actions={!isBreach && <Button variant="primary" busy={running} onClick={run}><LuPlay size={13} /> Run now</Button>}
    >
      {sources.error && <ErrorNotice error={sources.error} title="Could not load source metadata" onRetry={sources.reload} />}
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
          <BoundNote shown={items.length} total={total ?? (totalGte != null ? Math.max(totalGte, items.length) : (hasMore ? items.length + 1 : items.length))} noun={total == null && totalGte != null ? `items (≥${totalGte})` : 'items'} />
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
