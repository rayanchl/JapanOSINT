import React, { useMemo, useState } from 'react';
import { Link, useParams } from 'react-router-dom';
import { LuPlay, LuRefreshCw, LuExternalLink } from 'react-icons/lu';
import { api, errorMessage } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { Page, Card, Section, Pill, Button, KV, ErrorNotice, EmptyState, LoadingState, BoundNote, Mono, cx, toast } from '../ui/kit.jsx';
import { isSafeUrl } from '../../utils/safeUrl.js';
import { relativeTime, fmtAbs } from '../../utils/time.js';
import {
  measured, fmtInterval, fmtBytes, fmtCount, fmtPct, kindLabel, engineLabel,
  statusTone, gradeTone, splitTemplate, NOT_MEASURED,
} from '../../utils/sourceFormat.js';
import IntervalOverrideCard from './IntervalOverrideCard.jsx';

/* /console/sources/:id — one source, in the order the audit asked for: what
 * it is (small-model summary), engine and mechanism, parsing, frequency,
 * address, obtained data and its form, testing, status, size. Everything on
 * the page is one GET /api/sources/:id/detail plus the summary route; a value
 * the server did not measure says so. */

function when(iso) {
  if (!iso) return NOT_MEASURED;
  return `${fmtAbs(iso)} (${relativeTime(iso)})`;
}

/** A URL/body template with its {tokens} highlighted. */
export function Template({ value }) {
  if (!value) return <span className="text-osint-muted">{NOT_MEASURED}</span>;
  return (
    <code className="font-mono text-[11px] break-all">
      {splitTemplate(value).map((p, i) => (
        <span key={i} className={cx(p.token && 'text-accent bg-accent/10 rounded px-0.5')}>{p.text}</span>
      ))}
    </code>
  );
}

/** Rendering of the summary route's two shapes: a generated summary, or the
 * llm status plus the facts it would have been given. */
export function SummaryCard({ summary, loading, error, onRegenerate, busy }) {
  if (loading) return <Card><LoadingState label="Asking the summary model…" /></Card>;
  if (error) return <ErrorNotice error={error} title="Could not reach the summary route" />;
  if (!summary) return null;
  if (summary.error) {
    return (
      <Card className="border-accent/30">
        <div className="flex items-start justify-between gap-2">
          <div className="text-xs">
            <span className="text-accent font-medium">No summary: model unreachable</span>
            <span className="text-osint-muted"> · {summary.error}{summary.http_status ? ` (HTTP ${summary.http_status})` : ''} · {summary.llm_base_url}</span>
            <p className="text-osint-muted mt-1">Nothing was generated, so nothing is shown as prose. These are the {summary.facts_used} facts the model would have been given:</p>
          </div>
          <Button size="sm" busy={busy} onClick={onRegenerate}><LuRefreshCw size={12} /> Retry</Button>
        </div>
        <pre className="mt-2 text-[10px] font-mono text-osint-muted whitespace-pre-wrap break-all max-h-56 overflow-auto">{JSON.stringify(summary.facts, null, 1)}</pre>
      </Card>
    );
  }
  return (
    <Card>
      <div className="flex items-start justify-between gap-2">
        <div className="min-w-0">
          {summary.result_type && <Pill tone="accent" mono={false}>yields: {summary.result_type}</Pill>}
          <p className="text-sm text-osint-text mt-2">{summary.summary}</p>
          {Array.isArray(summary.caveats) && summary.caveats.length > 0 && (
            <ul className="mt-2 text-xs text-osint-muted list-disc pl-4 space-y-0.5">
              {summary.caveats.map((c, i) => <li key={i}>{c}</li>)}
            </ul>
          )}
        </div>
        <Button size="sm" busy={busy} onClick={onRegenerate} title="Ask the model again, ignoring the cache"><LuRefreshCw size={12} /></Button>
      </div>
      <div className="mt-2 text-[10px] font-mono text-osint-muted">
        model {summary.model} · {summary.facts_used} facts · {summary.cached ? 'cached' : 'generated'} {summary.generated_at ? relativeTime(summary.generated_at) : ''} · written from measured facts only
      </div>
    </Card>
  );
}

function Hist({ values, keyName, shown, total, noun }) {
  if (!values || values.length === 0) return <span className="text-xs text-osint-muted">none stored</span>;
  const max = Math.max(...values.map((v) => v.count || 0), 1);
  return (
    <div className="space-y-1">
      {values.map((v) => (
        <div key={v[keyName]} className="flex items-center gap-2 text-[11px]">
          <span className="font-mono w-56 truncate text-osint-text" title={v[keyName]}>{v[keyName]}</span>
          <div className="flex-1 h-2 bg-osint-border/40 rounded overflow-hidden"><div className="h-2 bg-accent/60" style={{ width: `${Math.max(2, (100 * (v.count || 0)) / max)}%` }} /></div>
          <span className="font-mono text-osint-muted w-16 text-right">{fmtCount(v.count)}</span>
        </div>
      ))}
      {total != null && <BoundNote shown={shown ?? values.length} total={total} noun={noun} />}
    </div>
  );
}

export default function SourceDetailPage() {
  const { id } = useParams();
  const detail = useApi(`/api/sources/${encodeURIComponent(id)}/detail`);
  const summary = useApi(`/api/sources/${encodeURIComponent(id)}/summary`);
  const samples = useApi(`/api/intel/items?source=${encodeURIComponent(id)}&limit=3`);
  const [running, setRunning] = useState(false);
  const [regen, setRegen] = useState(false);
  const d = detail.data;

  const run = async () => {
    setRunning(true);
    try {
      const r = await api.post(`/api/intel/sources/${encodeURIComponent(id)}/run`);
      toast(`ran: ${r?.ingested ?? '?'} new rows in ${r?.duration_ms ?? '?'} ms`, { tone: 'accent', ttl: 5000 });
      detail.reload({ silent: true }); samples.reload({ silent: true });
    } catch (e) { toast(e?.status === 409 ? 'a run is already in flight' : errorMessage(e), { tone: 'danger', ttl: 5000 }); }
    finally { setRunning(false); }
  };
  const regenerate = async () => {
    setRegen(true);
    try { summary.setData(await api.get(`/api/sources/${encodeURIComponent(id)}/summary?refresh=1`)); }
    catch (e) { toast(errorMessage(e), { tone: 'danger', ttl: 5000 }); }
    finally { setRegen(false); }
  };

  const reg = d?.registry, eng = d?.engine, sch = d?.schedule, st = d?.status, data = d?.data;
  const isHp = eng?.engine === 'hpengine';
  const sampleItems = useMemo(() => (Array.isArray(samples.data?.data) ? samples.data.data : Array.isArray(samples.data?.items) ? samples.data.items : []), [samples.data]);

  if (detail.loading && !d) return <Page wide title={id}><LoadingState label="Loading source…" /></Page>;
  if (detail.error) {
    return (
      <Page wide title={id} subtitle={<Link to="/console/sources" className="hover:text-accent">← all sources</Link>}>
        <ErrorNotice error={detail.error} title={detail.error.status === 404 ? 'No registered source has this id' : 'Could not load the source'} onRetry={detail.reload} />
      </Page>
    );
  }
  if (!d) return null;

  return (
    <Page wide
      title={<span className="flex items-center gap-2 flex-wrap">{reg.name || reg.id}{reg.name_ja && reg.name_ja !== reg.name && <span className="text-sm text-osint-muted font-normal">{reg.name_ja}</span>}
        <Pill tone={statusTone(st.status)}>{st.status || 'never run'}</Pill>
        {st.trust?.rated && <Pill tone={gradeTone(st.trust.grade)} title={`reliability ${fmtPct(st.trust.reliability)}`}>grade {st.trust.grade}</Pill>}
      </span>}
      subtitle={<span className="flex flex-wrap items-center gap-x-2 gap-y-1">
        <Link to="/console/sources" className="hover:text-accent">← all sources</Link>
        <span>·</span><Mono>{reg.id}</Mono>
        <span>·</span>{reg.collector_meta?.parent && <><Link to={`/console/collectors/${encodeURIComponent(reg.collector_meta.parent)}`} className="hover:text-accent" title="group">{reg.collector_meta.parent}</Link><span>›</span></>}<Link to={`/console/collectors/${encodeURIComponent(reg.collector)}`} className="hover:text-accent" title="collector">{reg.collector_meta?.label || reg.collector}</Link>
        {reg.category && <Pill>{reg.category}</Pill>}
        {reg.type && <Pill>{reg.type}</Pill>}
        <Pill tone="cyan" mono={false}>{engineLabel(reg.engine)}</Pill>
      </span>}
      actions={<div className="flex gap-2">
        {reg.url && isSafeUrl(reg.url) && <a href={reg.url} target="_blank" rel="noreferrer" className="inline-flex items-center gap-1 text-xs text-accent hover:underline"><LuExternalLink size={12} /> upstream</a>}
        <Link to={`/intel/sources/${encodeURIComponent(reg.id)}`} className="inline-flex items-center text-xs text-osint-muted hover:text-accent">stored items →</Link>
        {reg.kind !== 'internal' && <Button variant="primary" busy={running} onClick={run}><LuPlay size={13} /> Run now</Button>}
      </div>}
    >
      {reg.description && <p className="text-xs text-osint-muted -mt-2">{reg.description}</p>}

      <Section label="What it is">
        <SummaryCard summary={summary.data} loading={summary.loading && !summary.data} error={summary.error} onRegenerate={regenerate} busy={regen} />
      </Section>

      <Section label="Engine and mechanism" right={<span className="text-[10px] font-mono text-osint-muted">{kindLabel(reg.kind)}</span>}>
        <Card>
          <KV pairs={[
            ['engine', engineLabel(reg.engine)],
            ['kind', kindLabel(reg.kind)],
            ['entity pivot', reg.is_entity_pivot ? 'yes — routable by the OSINT dispatcher' : 'no'],
            isHp && ['mode', eng.mode],
            isHp && ['entity shape wanted', eng.want],
            isHp && ['entity tokens', (eng.entity_tokens || []).length ? eng.entity_tokens.join(' ') : 'none (fixed endpoint)'],
            isHp && ['request', eng.request?.method],
            isHp && ['url template', <Template value={eng.request?.url} />],
            isHp && eng.request?.post_body && ['POST body', <Template value={eng.request.post_body} />],
            isHp && eng.request?.content_type && ['content type', eng.request.content_type],
            isHp && (eng.request?.headers || []).length > 0 && ['headers (templates)', eng.request.headers.join(' | ')],
            isHp && ['credential', eng.request?.key_env ? `env ${eng.request.key_env}` : 'none required'],
            isHp && eng.request?.index_url && ['index page followed each run', <Template value={eng.request.index_url} />],
            isHp && eng.request?.index_href_must && ['index link must contain', eng.request.index_href_must],
            isHp && eng.request?.charset && ['declared charset', eng.request.charset],
            isHp && eng.request?.timeout_ms && ['timeout', `${eng.request.timeout_ms} ms`],
            isHp && eng.uses_date_tokens && ['date tokens', `rendered each run (tz +${eng.request?.tz_minutes || 0} min${eng.request?.date_back ? `, steps back up to ${eng.request.date_back} ${eng.request.date_step || 'd'}` : ''})`],
            isHp && eng.request?.page_delay_ms && ['pacing between pages', `${eng.request.page_delay_ms} ms`],
            isHp && eng.request?.filter_query && ['keeps only records mentioning the query', 'yes'],
            !isHp && ['registered url', reg.url || NOT_MEASURED],
            !isHp && eng.note && ['note', eng.note],
            ['host', reg.host || NOT_MEASURED],
            ['host pacing', d.pacing?.min_gap_ms != null ? `≥ ${d.pacing.min_gap_ms} ms between requests` : NOT_MEASURED],
          ].filter(Boolean)} />
        </Card>
      </Section>

      {isHp && (
        <Section label="Parsing">
          <Card>
            <KV pairs={[
              ['array path', eng.parsing?.array_path || 'auto-detected (densest array of objects)'],
              ['title keys', eng.parsing?.title_keys || 'default list (name / title / …)'],
              ['id keys', eng.parsing?.id_keys ? <span>{eng.parsing.id_keys} <span className="text-osint-muted">(+ composes, , chooses)</span></span> : 'default list (id / uid / guid / …)'],
              eng.parsing?.link_keys && ['link keys', eng.parsing.link_keys],
              eng.parsing?.link_tmpl && ['link template', <Template value={eng.parsing.link_tmpl} />],
              eng.parsing?.date_keys && ['date keys', eng.parsing.date_keys],
              eng.parsing?.body_keys && ['body keys', eng.parsing.body_keys],
              (eng.parsing?.lat_key || eng.parsing?.latlon_key || eng.parsing?.lonlat_key) && ['coordinates', eng.parsing.latlon_key ? `${eng.parsing.latlon_key} (lat,lon)` : eng.parsing.lonlat_key ? `${eng.parsing.lonlat_key} (lon,lat)` : `${eng.parsing.lat_key} / ${eng.parsing.lon_key}`],
              eng.mode === 'csv' && ['csv', `${eng.parsing?.csv_delim ? `delimiter ${eng.parsing.csv_delim}` : 'comma'}${eng.parsing?.csv_no_header ? ', no header row' : ''}${eng.parsing?.csv_comment ? `, comments ${eng.parsing.csv_comment}` : ''}${eng.parsing?.csv_skip_lines ? `, skips ${eng.parsing.csv_skip_lines} lines` : ''}`],
              eng.mode === 'xlsx' && ['worksheet', eng.parsing?.xlsx_sheet || `index ${eng.parsing?.xlsx_sheet_index || 0}`],
              eng.mode === 'html' && ['html', eng.parsing?.table_must ? `table containing "${eng.parsing.table_must}"${eng.parsing.table_skip_rows ? `, skips ${eng.parsing.table_skip_rows} rows` : ''}` : `anchors${eng.parsing?.href_must ? ` whose href contains "${eng.parsing.href_must}"` : ''}`],
              eng.parsing?.base && ['relative links resolved against', eng.parsing.base],
              ['record type stamped', eng.parsing?.record_type || NOT_MEASURED],
              ['language stamped', eng.parsing?.lang || 'en (default)'],
              eng.parsing?.max_items && ['max items', eng.parsing.max_items],
              ['detail hop', eng.detail_hop?.declared ? <span><Template value={eng.detail_hop.detail_url} /> <span className="text-osint-muted">via {eng.detail_hop.detail_key}{eng.detail_hop.detail_path ? ` → ${eng.detail_hop.detail_path}` : ''}{eng.detail_hop.detail_max ? `, at most ${eng.detail_hop.detail_max}` : ', every record (bounded by JO_HP_DETAIL_MAX)'}</span></span> : 'none'],
              ['paging', eng.paging?.strategy === 'single_request' ? 'single request' : `${eng.paging.strategy}${eng.paging.next_path ? ` ${eng.paging.next_path}` : ''}${eng.paging.page_param ? ` ${eng.paging.page_param}` : ''}${eng.paging.page_size ? `, ${eng.paging.page_size}/page` : ''}${eng.paging.page_zero_based ? ', 0-based' : ''}${eng.paging.page_max ? `, up to ${eng.paging.page_max} pages` : ''}`],
              eng.paging?.next_tmpl && ['next page template', <Template value={eng.paging.next_tmpl} />],
            ].filter(Boolean)} />
          </Card>
        </Section>
      )}

      <Section label="Frequency">
        <Card>
          <KV pairs={[
            ['declared interval', fmtInterval(sch.declaredInterval)],
            ['measured interval', sch.adaptive ? (sch.measuredInterval != null ? `${fmtInterval(sch.measuredInterval)} (from the newest stored item)` : 'adaptive — no dated item yet') : 'not adaptive'],
            ['policy interval', sch.policyInterval != null ? `${fmtInterval(sch.policyInterval)} (${sch.policy || 'declared'})` : 'not reported'],
            ['set for this source', sch.overrideInterval != null ? `${fmtInterval(sch.overrideInterval)} (outranks the policy)` : 'no'],
            ['effective interval', sch.effectiveInterval != null ? `${fmtInterval(sch.effectiveInterval)}${sch.cadenceBasis ? ` (${sch.cadenceBasis})` : ''}` : 'as declared'],
            ['next due', sch.next_due ? `${when(sch.next_due)}${sch.overdue ? ' — overdue' : ''}` : (sch.declaredInterval > 0 ? 'not yet run' : 'on demand')],
            ['schedule mode', sch.schedule_mode || 'default'],
            ['backed off', sch.backedOff ? `until ${when(sch.backoffUntil)}` : 'no'],
            ['quarantined (health)', sch.quarantined ? 'yes' : 'no'],
            ['consecutive failures / empties', `${sch.consecutiveFailures} / ${sch.consecutiveEmpties}`],
            ['runs in 30 days', sch.stats_30d ? `${fmtCount(sch.stats_30d.runs_30d)} (${fmtPct(sch.stats_30d.success_rate_30d)} ok)` : NOT_MEASURED],
            ['average duration (30 d)', sch.stats_30d?.avg_duration_ms != null ? `${Math.round(sch.stats_30d.avg_duration_ms)} ms` : NOT_MEASURED],
            ['last run', when(sch.stats_30d?.last_run_at)],
          ]} />
          <IntervalOverrideCard id={reg.id} />
        </Card>
      </Section>

      <Section label="Address and location">
        <Card>
          <KV pairs={[
            ['portal', reg.url && isSafeUrl(reg.url) ? <a href={reg.url} target="_blank" rel="noreferrer" className="text-accent hover:underline">{reg.url}</a> : (reg.url || NOT_MEASURED)],
            ['fetch host', reg.host || NOT_MEASURED],
            ['license', reg.license || NOT_MEASURED],
            ['map layer', reg.layer || 'none'],
            ['geocoded records', `${fmtCount(data.geocoded)} of ${fmtCount(data.item_count)}`],
            ['bounding box', data.bbox ? `${data.bbox.min_lat.toFixed(3)},${data.bbox.min_lon.toFixed(3)} → ${data.bbox.max_lat.toFixed(3)},${data.bbox.max_lon.toFixed(3)}` : 'no geocoded records'],
          ]} />
        </Card>
      </Section>

      <Section label="Obtained data and its form" right={<Link to={`/intel/sources/${encodeURIComponent(reg.id)}`} className="text-[10px] text-accent hover:underline">browse and search all items →</Link>}>
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-3">
          <Card>
            <KV pairs={[
              ['records stored', fmtCount(data.item_count)],
              ['of which notices', fmtCount(data.notices)],
              ['geocoded / awaiting geo', `${fmtCount(data.geocoded)} / ${measured(data.awaiting_geo, fmtCount)}`],
              ['first / last fetched', `${when(data.first_fetched)} · ${when(data.last_fetched)}`],
              ['published range', data.first_published || data.last_published ? `${data.first_published || '?'} → ${data.last_published || '?'}` : NOT_MEASURED],
              ['languages', (data.languages || []).map((l) => `${l.language} (${fmtCount(l.count)})`).join(', ') || 'none stored'],
              ['DB text size', `${fmtBytes(data.db_text_bytes)} (title + body + properties; not transfer size)`],
            ]} />
          </Card>
          <Card>
            <div className="text-[10px] uppercase tracking-wider text-osint-muted mb-1">record types</div>
            <Hist values={data.record_types?.values} keyName="record_type" shown={data.record_types?.shown} total={data.record_types?.total} noun="record types" />
          </Card>
          <Card>
            <div className="text-[10px] uppercase tracking-wider text-osint-muted mb-1">fields present (sample of {data.field_inventory?.sample_size ?? 0} most recent records)</div>
            <Hist values={data.field_inventory?.fields?.values} keyName="key" shown={data.field_inventory?.fields?.shown} total={data.field_inventory?.fields?.total} noun="fields" />
          </Card>
          <Card>
            <div className="text-[10px] uppercase tracking-wider text-osint-muted mb-1">sample records (3 most recent)</div>
            {samples.loading && <LoadingState label="Loading sample…" />}
            {samples.error && <ErrorNotice error={samples.error} title="Could not load sample records" onRetry={samples.reload} />}
            {!samples.loading && !samples.error && sampleItems.length === 0 && <span className="text-xs text-osint-muted">nothing stored yet</span>}
            <ul className="space-y-1">
              {sampleItems.map((it) => (
                <li key={it.uid} className="text-[11px]">
                  <Link to={`/intel/items/${encodeURIComponent(it.uid)}`} className="text-osint-text hover:text-accent">{it.title || it.uid}</Link>
                  <span className="text-osint-muted font-mono ml-2">{it.record_type}{it.published_at ? ` · ${it.published_at}` : ''}</span>
                </li>
              ))}
            </ul>
          </Card>
        </div>
      </Section>

      <Section label="Testing" right={<Link to={`/api/sources/${encodeURIComponent(reg.id)}/logs`} className="text-[10px] text-osint-muted hover:text-accent" target="_blank" rel="noreferrer">all runs (JSON) →</Link>}>
        <Card>
          <div className="text-[10px] uppercase tracking-wider text-osint-muted mb-1">recent runs</div>
          {(d.runs?.recent || []).length === 0 ? <span className="text-xs text-osint-muted">this source has never run</span> : (
            <table className="w-full text-[11px]">
              <thead><tr className="text-osint-muted text-left"><th className="py-1 pr-2">when</th><th className="pr-2">status</th><th className="pr-2 text-right">fetched</th><th className="pr-2 text-right">stored</th><th className="pr-2 text-right">ms</th><th className="pr-2 text-right">bytes</th><th>error</th></tr></thead>
              <tbody>
                {d.runs.recent.map((r) => (
                  <tr key={r.id} className="border-t border-osint-border/40">
                    <td className="py-1 pr-2 font-mono whitespace-nowrap" title={r.timestamp}>{relativeTime(r.timestamp)}</td>
                    <td className="pr-2"><Pill tone={r.status === 'ok' ? 'success' : 'danger'}>{r.status}</Pill></td>
                    <td className="pr-2 text-right font-mono">{fmtCount(r.records_fetched)}</td>
                    <td className="pr-2 text-right font-mono">{r.stored == null ? NOT_MEASURED : fmtCount(r.stored)}</td>
                    <td className="pr-2 text-right font-mono">{r.duration_ms == null ? '—' : r.duration_ms}</td>
                    <td className="pr-2 text-right font-mono" title={r.bytes_fetched == null ? 'run predates byte accounting' : `${r.bytes_fetched} bytes`}>{r.bytes_fetched == null ? NOT_MEASURED : fmtBytes(r.bytes_fetched)}</td>
                    <td className="text-osint-muted break-all">{r.error || ''}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
          <BoundNote shown={d.runs?.shown ?? 0} total={d.runs?.total} noun="runs" className="mt-1" />
        </Card>
        <Card className="mt-3">
          <div className="text-[10px] uppercase tracking-wider text-osint-muted mb-1">engine notices (latest of each kind)</div>
          {(d.notices?.latest || []).length === 0 ? <span className="text-xs text-osint-muted">no notices stored — no fetch failure, shape surprise or truncation has been recorded for this source</span> : (
            <ul className="space-y-2">
              {d.notices.latest.map((n) => (
                <li key={n.uid || n.kind} className="text-[11px]">
                  <Pill tone={n.kind === 'collector-fetch-failure' ? 'danger' : 'warning'}>{n.kind}</Pill>
                  <span className="ml-2 text-osint-text">{n.title}</span>
                  <span className="ml-2 text-osint-muted font-mono">{relativeTime(n.fetched_at)}</span>
                  {n.properties && <pre className="mt-1 text-[10px] font-mono text-osint-muted whitespace-pre-wrap break-all max-h-32 overflow-auto">{JSON.stringify(n.properties, null, 1)}</pre>}
                </li>
              ))}
            </ul>
          )}
          {(d.notices?.counts || []).length > 0 && <div className="mt-2 text-[10px] font-mono text-osint-muted">{d.notices.counts.map((c) => `${c.kind}: ${fmtCount(c.count)}`).join(' · ')}</div>}
        </Card>
        <Card className="mt-3">
          <div className="text-[10px] uppercase tracking-wider text-osint-muted mb-1">provenance — rows in docs/ naming this id</div>
          {d.provenance?.note && <div className="text-xs text-osint-muted">{d.provenance.note}</div>}
          {(d.provenance?.rows || []).length === 0 && !d.provenance?.note && <span className="text-xs text-osint-muted">none — this id does not appear in any manifest, probe or verification table ({fmtCount(d.provenance?.files_scanned)} files scanned)</span>}
          <ul className="space-y-1">
            {(d.provenance?.rows || []).map((r, i) => (
              <li key={i} className="text-[10px] font-mono break-all"><span className="text-accent">{r.file}:{r.line}</span> <span className="text-osint-muted">{r.text}</span></li>
            ))}
          </ul>
          {d.provenance?.total > 0 && <BoundNote shown={d.provenance.shown} total={d.provenance.total} noun="rows" className="mt-1" />}
        </Card>
      </Section>

      <Section label="Status">
        {/* WHY it is not working, and what to do — stated before the run
            numbers rather than left in a notice's properties blob below. The
            same object the Sources list shows, from the same builder, so the
            reason a reader arrived with is the reason this page confirms. */}
        {st.health && !st.health.working && (
          <Card>
            <KV pairs={[
              ['not working', <Pill tone="danger">{st.health.state}</Pill>],
              st.health.http_status && ['upstream answered', `HTTP ${st.health.http_status}${st.health.body_parsed === false ? ' · the body did not parse' : ''}`],
              st.health.outcome && ['the engine read it as', st.health.outcome],
              st.health.credentials_required?.length && ['needs a credential', st.health.credentials_required.join(', ')],
              st.health.note && ['what happened', st.health.note],
              st.health.remedy && ['remedy', st.health.remedy],
              st.health.since && ['recorded', when(st.health.since)],
            ].filter(Boolean)} />
          </Card>
        )}
        <Card>
          <KV pairs={[
            ['status', <Pill tone={statusTone(st.status)}>{st.status || 'pending — never run'}</Pill>],
            ['last check / last success', `${when(st.last_check)} · ${when(st.last_success)}`],
            ['last run', st.records_last_run != null ? `${fmtCount(st.records_last_run)} records in ${st.response_time_ms ?? '?'} ms` : NOT_MEASURED],
            st.error_message && ['last error', st.error_message],
            ['trust', st.trust?.rated ? `grade ${st.trust.grade} · reliability ${fmtPct(st.trust.reliability)} · success ${fmtPct(st.trust.success_rate)} · freshness ${fmtPct(st.trust.freshness)} · anomalies ${fmtCount(st.trust.anomalies_30d)} in 30 d` : 'unrated — no runs in the trust window'],
            ['credential', d.credentials?.requiresKey == null ? 'no credential table entry' : d.credentials.requiresKey ? (d.credentials.configured ? `configured (${(d.credentials.envVars || []).map((v) => v.name).join(', ')})` : `MISSING: ${(d.credentials.missingVars || []).join(', ')}`) : 'none required'],
            ['quarantined', sch.quarantined ? 'yes (health)' : 'no'],
          ].filter(Boolean)} />
        </Card>
      </Section>

      <Section label="Size">
        <Card>
          <KV pairs={[
            ['records stored', fmtCount(data.item_count)],
            ['records last run (distinct uids)', measured(st.records_last_run, fmtCount)],
            ['records in 30 days of runs', measured(sch.stats_30d?.records_30d, fmtCount)],
            ['DB text size', fmtBytes(data.db_text_bytes)],
            ['transfer, last run', (() => { const r = (d.runs?.recent || [])[0]; return r ? (r.bytes_fetched == null ? `${NOT_MEASURED} (run predates byte accounting)` : `${fmtBytes(r.bytes_fetched)} received`) : 'never run'; })()],
            ['transfer, 30 days', sch.stats_30d?.bytes_30d != null ? `${fmtBytes(sch.stats_30d.bytes_30d)} over ${fmtCount(sch.stats_30d.runs_with_bytes_30d)} run${sch.stats_30d.runs_with_bytes_30d === 1 ? '' : 's'}` : (sch.stats_30d?.runs_30d > 0 ? `${NOT_MEASURED} (runs predate byte accounting)` : 'no runs in 30 days')],
          ]} />
        </Card>
      </Section>
      <div className="text-[10px] font-mono text-osint-muted">detail fetched {relativeTime(d.meta?.fetched_at)}</div>
    </Page>
  );
}
