import React, { useState } from 'react';
import { Link, useLocation, useNavigate, useParams } from 'react-router-dom';
import { LuExternalLink, LuDownload, LuShieldCheck, LuEye } from 'react-icons/lu';
import { api } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { useAuth } from '../../auth/AuthContext.jsx';
import { Page, Section, Pill, Button, ErrorNotice, LoadingState, KV, Segmented, BoundNote, CopyButton, cx, toast } from '../ui/kit.jsx';
import SaveStarButton from '../saved/SaveStarButton.jsx';
import PinToCaseButton from '../cases/CasePickerSheet.jsx';
import AnnotationsSection from './AnnotationsSection.jsx';
import { entityVisual } from '../../utils/entityVisuals.js';
import { isSafeUrl } from '../../utils/safeUrl.js';
import { fmtAbs, relativeTime } from '../../utils/time.js';

/** One record — iOS `IntelDetail` with the Enrichment sections. */
export default function IntelItemPage() {
  const { uid } = useParams();
  const location = useLocation();
  const navigate = useNavigate();
  const enc = encodeURIComponent(uid);
  const { data, error, loading, reload } = useApi(`/api/intel/items/${enc}`, { deps: [uid] });
  // The list carried the translation (fetched with lang_view=both); the
  // detail endpoint does not shape one, so keep it only when it is this uid's.
  const fromList = location.state?.item?.uid === uid ? location.state.item : null;
  const item = data?.data ? { ...(fromList || {}), ...data.data, translation: data.data.translation ?? fromList?.translation ?? null } : fromList;
  const [mode, setMode] = useState(fromList?.translation ? 'both' : 'original');
  const isBreach = uid.startsWith('breach:');

  const title = item ? (mode !== 'original' && item.translation?.title ? item.translation.title : item.title || '(untitled)') : uid;

  return (
    <Page wide
      title={<span className="break-words">{loading && !item ? 'Loading…' : title}</span>}
      subtitle={item && (
        <span className="flex flex-wrap items-center gap-1.5">
          <button type="button" className="hover:text-accent" onClick={() => navigate(-1)}>← back</button>
          <span>·</span>
          <Link to={`/intel/sources/${encodeURIComponent(item.source_id)}`} className="text-accent hover:underline">{item.provenance?.source_name || item.source_id}</Link>
          {item.record_type && <Pill>{item.record_type}</Pill>}
          {item.language && <Pill>{item.language}</Pill>}
          {item.published_at && <span title={item.published_at}>published {relativeTime(item.published_at)}</span>}
          {item.cluster_id && item.cluster_id !== uid && <Link to={`/intel/items/${encodeURIComponent(item.cluster_id)}`}><Pill tone="cyan" title="near-duplicate cluster; links to its earliest member">cluster ↗</Pill></Link>}
          {item.cluster_id === uid && <Pill tone="cyan">cluster root</Pill>}
          {isBreach && <Pill tone="danger">breach record</Pill>}
        </span>
      )}
      actions={item && (
        <>
          {item.translation && <Segmented value={mode} onChange={setMode} options={[{ value: 'original', label: '原文' }, { value: 'translated', label: 'EN' }, { value: 'both', label: 'both' }]} />}
          <SaveStarButton item={{ kind: 'intel_item', refId: uid, displayName: item.title || uid, link: item.link, lat: item.lat ?? undefined, lon: item.lon ?? undefined }} showLabel />
          <PinToCaseButton refType="intel_item" refId={uid} label={item.title || uid} size="md" />
          {item.link && isSafeUrl(item.link) && <Button onClick={() => window.open(item.link, '_blank', 'noreferrer')}><LuExternalLink size={13} /> Open source</Button>}
        </>
      )}
    >
      {error && <ErrorNotice error={error} title={error.status === 403 ? 'This record is operator-gated' : error.status === 404 ? 'Record not found' : 'Could not load the record'} onRetry={reload}>
        {error.status === 404 && 'The uid may have been evicted or never stored.'}
      </ErrorNotice>}
      {loading && !item && <LoadingState />}
      {item && (
        <div className="grid grid-cols-1 lg:grid-cols-[minmax(0,1fr)_320px] gap-4">
          <div className="space-y-4 min-w-0">
            {item.translation && mode !== 'original' && (
              <div className="text-[11px] text-accent font-mono">machine translation · {item.translation.engine || 'engine unknown'} · {item.translation.translated_at ? fmtAbs(item.translation.translated_at) : ''} — not a human translation</div>
            )}
            {mode === 'both' && item.translation?.title && item.title && <div className="text-sm text-osint-muted italic">{item.title}</div>}
            <TextBlock label="Summary" original={item.summary} translated={item.translation?.summary} mode={mode} />
            <TextBlock label="Body" original={item.body} translated={item.translation?.body} mode={mode} pre />
            {!item.summary && !item.body && <div className="text-xs text-osint-muted">This record carries no summary or body text.</div>}

            {Array.isArray(item.tags) && item.tags.length > 0 && (
              <Section label="Tags"><div className="flex flex-wrap gap-1">{item.tags.map((t) => <Pill key={t}>{t}</Pill>)}</div></Section>
            )}
            {Array.isArray(item.keywords) && item.keywords.length > 0 && (
              <Section label="Keywords"><div className="flex flex-wrap gap-1">{item.keywords.map((k, i) => <Pill key={i} tone="cyan">{typeof k === 'string' ? k : k.keyword || k.text || JSON.stringify(k)}</Pill>)}</div></Section>
            )}

            <EntitiesSection uid={uid} />
            <MediaSection uid={uid} />
            <EvidenceSection uid={uid} />
            {isBreach && <RevealSection uid={uid} />}
            <AnnotationsSection refType="intel_item" refId={uid} />
            <RawSection item={item} />
          </div>

          <aside className="space-y-4 min-w-0">
            <Section label="Provenance">
              <KV pairs={[
                ['uid', <span className="flex items-center gap-1 break-all">{uid}<CopyButton text={uid} /></span>],
                ['source', item.provenance?.source_name], ['source id', item.source_id],
                ['source (ja)', item.provenance?.source_name_ja], ['category', item.provenance?.category],
                ['method', item.provenance?.collection_method], ['sub-source', item.sub_source_id],
                ['author', item.author], ['language', item.language],
                ['published', item.published_at ? fmtAbs(item.published_at) : null], ['fetched', item.fetched_at ? fmtAbs(item.fetched_at) : null],
                ['licence', item.provenance?.license], ['confidence', item.provenance?.confidence],
                ['upstream', item.provenance?.source_url && isSafeUrl(item.provenance.source_url) ? <a className="text-accent hover:underline" href={item.provenance.source_url} target="_blank" rel="noreferrer">{item.provenance.source_url}</a> : item.provenance?.source_url],
                ['link', item.link ? (isSafeUrl(item.link) ? <a className="text-accent hover:underline" href={item.link} target="_blank" rel="noreferrer">{item.link}</a> : <span title="not http(s); not linked">{item.link}</span>) : null],
              ]} />
            </Section>
            {(item.lat != null && item.lon != null) ? (
              <Section label="Location">
                <KV pairs={[['lat', item.lat], ['lon', item.lon], ['geometry from', item.geom_source], ['geocoded at', item.geom_at ? fmtAbs(item.geom_at) : null]]} />
                <div className="flex gap-1 mt-2">
                  <Button size="sm" onClick={() => { window.dispatchEvent(new CustomEvent('japanosint:flyto', { detail: { lat: item.lat, lon: item.lon, lng: item.lon, zoom: 14 } })); navigate('/'); }}>Show on map</Button>
                  <Button size="sm" onClick={() => navigate(`/intel?view=search&mode=near&near=${item.lat},${item.lon}&radius_m=1000`)}>Nearby items</Button>
                </div>
              </Section>
            ) : (
              <Section label="Location"><div className="text-xs text-osint-muted">No coordinates on this record{item.geom_source ? ` (geometry source: ${item.geom_source})` : ''}.</div></Section>
            )}
            {item.score && <Section label="Score"><KV pairs={[['score', item.score.score], ['bm25', item.score.bm25], ['trust', item.score.trust], ['decay', item.score.decay]]} /></Section>}
          </aside>
        </div>
      )}
    </Page>
  );
}

function TextBlock({ label, original, translated, mode, pre }) {
  if (!original && !translated) return null;
  const showO = mode === 'original' || mode === 'both' || !translated;
  const showT = translated && (mode === 'translated' || mode === 'both');
  const cls = cx('text-sm text-osint-text leading-relaxed break-words', pre && 'whitespace-pre-wrap');
  return (
    <Section label={label}>
      {showT && <div className={cls}>{translated}</div>}
      {showO && showT && <div className="h-px bg-osint-border my-2" />}
      {showO && original && <div className={cx(cls, showT && 'text-osint-muted')}>{original}</div>}
    </Section>
  );
}

function EntitiesSection({ uid }) {
  const { data, error, loading, reload } = useApi(`/api/intel/items/${encodeURIComponent(uid)}/entities`, { deps: [uid] });
  const rows = Array.isArray(data?.data) ? data.data : [];
  return (
    <Section label="Entities" right={<span className="font-mono text-[10px] text-osint-muted">{rows.length}</span>}>
      {loading && <LoadingState label="Loading entities…" />}
      {error && <ErrorNotice error={error} title="Could not load entities" onRetry={reload} />}
      {!loading && !error && rows.length === 0 && <div className="text-xs text-osint-muted">No entities extracted from this record (yet).</div>}
      <div className="flex flex-wrap gap-1">
        {rows.map((e) => {
          const v = entityVisual(e.type);
          return (
            <Link key={e.entity_id} to={`/entities/${encodeURIComponent(String(e.type).toLowerCase())}/${encodeURIComponent(e.entity_id)}`}
              className={cx('inline-flex items-center gap-1 px-1.5 py-0.5 rounded border text-[11px] hover:brightness-125', v.color)} title={e.value}>
              <span className="opacity-70">{v.label}</span><span className="font-mono">{e.label || e.value}</span>
            </Link>
          );
        })}
      </div>
    </Section>
  );
}

function MediaSection({ uid }) {
  const { data, error, loading, reload } = useApi(`/api/intel/items/${encodeURIComponent(uid)}/media`, { deps: [uid] });
  const rows = Array.isArray(data?.data) ? data.data : [];
  const m = data?.meta;
  if (!loading && !error && rows.length === 0) return null;
  return (
    <Section label="Media" right={m && <span className="font-mono text-[10px] text-osint-muted">{m.analyzed} analysed · {m.pending} pending · {m.failed} failed · {m.geotagged} geotagged</span>}>
      {loading && <LoadingState label="Loading media…" />}
      {error && <ErrorNotice error={error} title="Could not load media" onRetry={reload} />}
      <ul className="grid grid-cols-1 md:grid-cols-2 gap-2">
        {rows.map((a) => (
          <li key={a.id} className="rounded-md border border-osint-border bg-osint-bg/60 p-2 space-y-1">
            {a.url && isSafeUrl(a.url) && (a.content_type || '').startsWith('image/') && (
              <img src={a.url} alt="" loading="lazy" className="max-h-56 rounded object-contain bg-black/30 w-full" />
            )}
            {a.url && isSafeUrl(a.url) && (a.content_type || '').startsWith('video/') && (
              <video src={a.url} controls className="max-h-56 rounded w-full" />
            )}
            <KV pairs={[
              ['url', a.url && isSafeUrl(a.url) ? <a className="text-accent hover:underline break-all" href={a.url} target="_blank" rel="noreferrer">{a.url}</a> : a.url],
              ['type', a.content_type], ['size', a.bytes != null ? `${a.bytes.toLocaleString()} B` : null],
              ['dimensions', a.width && a.height ? `${a.width}×${a.height}` : null],
              ['sha256', a.sha256], ['pHash', a.phash],
              ['gps', a.has_gps ? `${a.lat}, ${a.lon}` : null],
              ['camera', a.exif?.camera ? [a.exif.camera.make, a.exif.camera.model].filter(Boolean).join(' ') : null],
              ['taken', a.exif?.time?.original],
              ['analysed', a.analyzed_at ? fmtAbs(a.analyzed_at) : (a.analyze_failed ? `failed ×${a.analyze_failed}` : 'pending')],
            ]} />
            {a.ocr_text && <details><summary className="text-[11px] text-osint-muted cursor-pointer">OCR text{a.ocr_conf != null ? ` (confidence ${a.ocr_conf})` : ''}</summary><pre className="text-xs whitespace-pre-wrap text-osint-text mt-1">{a.ocr_text}</pre></details>}
          </li>
        ))}
      </ul>
    </Section>
  );
}

function EvidenceSection({ uid }) {
  const auth = useAuth();
  const { data, error, loading, reload } = useApi(`/api/intel/items/${encodeURIComponent(uid)}/evidence`, { deps: [uid] });
  const rows = Array.isArray(data?.data) ? data.data : [];
  const m = data?.meta;
  const [verify, setVerify] = useState(null);
  const [verifying, setVerifying] = useState(false);
  const [dl, setDl] = useState({});

  const download = async (row) => {
    setDl((x) => ({ ...x, [row.id]: true }));
    try {
      const res = await api.raw(`/api/evidence/${encodeURIComponent(row.id)}/raw`);
      if (!res.ok) {
        let body = null; try { body = await res.json(); } catch { /* not json */ }
        throw new Error(`HTTP ${res.status}${body?.error ? `: ${body.error}` : ''}`);
      }
      const blob = await res.blob();
      const sha = res.headers.get('X-JO-Evidence-SHA256');
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a'); a.href = url; a.download = `evidence-${row.id}.bin`; a.click();
      setTimeout(() => URL.revokeObjectURL(url), 5000);
      toast(sha ? `downloaded · sha256 ${sha.slice(0, 12)}…` : 'downloaded', { tone: 'accent' });
    } catch (e) { toast(`raw evidence: ${e.message}`, { tone: 'danger', ttl: 5000 }); }
    finally { setDl((x) => ({ ...x, [row.id]: false })); }
  };

  const runVerify = async () => {
    setVerifying(true);
    try { setVerify(await api.post('/api/evidence/verify')); }
    catch (e) { setVerify({ error: e }); }
    finally { setVerifying(false); }
  };

  if (!loading && !error && rows.length === 0) {
    return <Section label="Evidence"><div className="text-xs text-osint-muted">No captured evidence for this record. Capture is enabled per source under Console → Admin.</div></Section>;
  }
  return (
    <Section label="Evidence (chain of custody)" right={(
      <span className="flex items-center gap-2">
        {m && <span className="font-mono text-[10px] text-osint-muted">{m.present} present · {m.evicted} evicted</span>}
        {auth.isPlatformAdmin && <Button size="sm" busy={verifying} onClick={runVerify}><LuShieldCheck size={12} /> Verify chain</Button>}
      </span>
    )}>
      {loading && <LoadingState label="Loading evidence…" />}
      {error && <ErrorNotice error={error} title="Could not load evidence" onRetry={reload} />}
      {verify?.error && <ErrorNotice error={verify.error} title="Verification failed" />}
      {verify && !verify.error && (
        <div className={cx('rounded-md border px-3 py-2 text-xs font-mono mb-2', verify.ok ? 'border-neon-green/40 text-neon-green bg-neon-green/10' : 'border-neon-red/40 text-neon-red bg-neon-red/10')}>
          {verify.ok ? `chain intact · ${verify.count} rows` : `chain BROKEN at #${verify.brokenAt} (${verify.brokenId || '?'}): ${verify.reason}`}
        </div>
      )}
      <ul className="space-y-2">
        {rows.map((r) => (
          <li key={r.id} className="rounded-md border border-osint-border bg-osint-bg/60 p-2">
            <div className="flex items-center justify-between gap-2 text-[10px] font-mono text-osint-muted">
              <span>#{r.chain_seq} · {fmtAbs(r.captured_at)} · HTTP {r.response_status} · {r.content_bytes?.toLocaleString()} B</span>
              <span className="flex items-center gap-1">
                {r.blob_present ? <Pill tone="success">blob present</Pill> : <Pill tone="warning">blob evicted</Pill>}
                {r.blob_present && <Button size="sm" busy={dl[r.id]} onClick={() => download(r)} title="Raw bytes (operator-gated)"><LuDownload size={12} /></Button>}
              </span>
            </div>
            <KV className="mt-1" pairs={[
              ['request', `${r.request_method || 'GET'} ${r.request_url || ''}`], ['content type', r.content_type],
              ['sha256', r.content_sha256], ['row hash', r.row_hash], ['prev hash', r.prev_hash],
            ]} />
          </li>
        ))}
      </ul>
      {data?.page && rows.length >= data.page.limit && <BoundNote shown={rows.length} total={rows.length + 1} noun="evidence rows (server limit reached)" className="mt-1" />}
    </Section>
  );
}

function RevealSection({ uid }) {
  const [res, setRes] = useState(null);
  const [busy, setBusy] = useState(false);
  const reveal = async () => {
    setBusy(true);
    try { setRes({ data: await api.get(`/api/intel/items/${encodeURIComponent(uid)}/reveal`) }); }
    catch (e) { setRes({ error: e }); }
    finally { setBusy(false); }
  };
  return (
    <Section label="Leaked secret (operator)" right={<Button size="sm" variant="danger" busy={busy} onClick={reveal}><LuEye size={12} /> Reveal</Button>}>
      <div className="text-xs text-osint-muted">Decrypts this breach record's secret server-side. Platform-operator role required; the request is audited.</div>
      {res?.error && <ErrorNotice error={res.error} title={res.error.status === 403 ? 'Reveal refused: platform operator role required' : 'Reveal failed'} className="mt-2" />}
      {res?.data && <div className="mt-2">{typeof res.data === 'object' ? <KV pairs={Object.entries(res.data)} /> : <pre className="text-xs">{String(res.data)}</pre>}</div>}
    </Section>
  );
}

function RawSection({ item }) {
  const props = item.properties && typeof item.properties === 'object' ? Object.entries(item.properties) : [];
  return (
    <Section label="Raw record">
      {props.length > 0 && <KV pairs={props} className="mb-2" />}
      <details>
        <summary className="text-[11px] text-osint-muted cursor-pointer">full JSON</summary>
        <pre className="text-[11px] text-osint-muted bg-black/20 rounded p-2 overflow-auto mt-1 max-h-96">{JSON.stringify(item, null, 2)}</pre>
      </details>
    </Section>
  );
}

