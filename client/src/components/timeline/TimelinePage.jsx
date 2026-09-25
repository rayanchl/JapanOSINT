import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { Link, useNavigate, useSearchParams } from 'react-router-dom';
import { BarChart, Bar, XAxis, YAxis, Tooltip, ResponsiveContainer, Cell } from 'recharts';
import { LuRefreshCw, LuMapPin, LuFileText, LuAtSign, LuBell, LuX } from 'react-icons/lu';
import { api } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { Page, Card, Button, Input, Field, Segmented, ErrorNotice, EmptyState, LoadingState, CopyButton, cx } from '../ui/kit.jsx';
import { fmtAbs } from '../../utils/time.js';

/**
 * Timeline — the iOS `TimelineScreen`. One temporal stream merged from intel
 * items, entity mentions and alert events (GET /api/timeline), a stacked
 * histogram per bucket, and a keyset-paged event list. Tapping a bucket
 * narrows the LIST to that bucket; the histogram deliberately keeps the
 * whole window (the server ignores cursor/limit for buckets).
 */
const WINDOWS = [
  { value: 86_400, label: '24h' },
  { value: 604_800, label: '7d' },
  { value: 2_592_000, label: '30d' },
  { value: 'custom', label: 'Custom' },
];
const MAX_WINDOW_DAYS = 366;
const PAGE = 100;

const KIND = {
  intel: { label: 'INTEL', icon: LuFileText, css: '--accent' },
  mention: { label: 'MENTION', icon: LuAtSign, css: '--accent-alt' },
  alert: { label: 'ALERT', icon: LuBell, css: '--neon-red-rgb' },
};

function cssRgb(varName, alpha) {
  if (typeof window === 'undefined') return '#888';
  const v = getComputedStyle(document.documentElement).getPropertyValue(varName).trim();
  if (!v) return '#888';
  return alpha != null ? `rgb(${v} / ${alpha})` : `rgb(${v})`;
}

function iso(d) { return new Date(d).toISOString().replace(/\.\d{3}Z$/, 'Z'); }
function toLocalInput(d) {
  const x = new Date(d); const p = (n) => String(n).padStart(2, '0');
  return `${x.getFullYear()}-${p(x.getMonth() + 1)}-${p(x.getDate())}T${p(x.getHours())}:${p(x.getMinutes())}`;
}
function bucketLabel(t, size) {
  const d = new Date(t);
  if (Number.isNaN(d.getTime())) return t;
  return size === 'hour'
    ? d.toLocaleString('en-GB', { timeZone: 'Asia/Tokyo', month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit', hour12: false })
    : d.toLocaleDateString('en-GB', { timeZone: 'Asia/Tokyo', month: '2-digit', day: '2-digit' });
}

export default function TimelinePage() {
  const [params, setParams] = useSearchParams();
  const caseId = params.get('case_id') || '';
  const [windowSel, setWindowSel] = useState(604_800);
  const [customSince, setCustomSince] = useState(() => toLocalInput(Date.now() - 7 * 86_400_000));
  const [customUntil, setCustomUntil] = useState(() => toLocalInput(Date.now()));
  const [bucket, setBucket] = useState('day');
  const [source, setSource] = useState('');
  const [focused, setFocused] = useState(null); // { t } — a histogram bucket

  // Resolved request window.
  const win = useMemo(() => {
    if (windowSel === 'custom') {
      const s = new Date(customSince), u = new Date(customUntil);
      if (Number.isNaN(s.getTime()) || Number.isNaN(u.getTime()) || s >= u) return null;
      return { since: iso(s), until: iso(u), seconds: (u - s) / 1000 };
    }
    const u = new Date(); const s = new Date(u.getTime() - windowSel * 1000);
    return { since: iso(s), until: iso(u), seconds: windowSel };
  }, [windowSel, customSince, customUntil]);
  const tooWide = win ? win.seconds > MAX_WINDOW_DAYS * 86_400 : false;

  const [buckets, setBuckets] = useState([]);
  const [events, setEvents] = useState([]);
  const [meta, setMeta] = useState(null);
  const [cursor, setCursor] = useState(null);
  const [loading, setLoading] = useState(false);
  const [loadingMore, setLoadingMore] = useState(false);
  const [error, setError] = useState(null);
  const seq = useRef(0);

  const buildQuery = useCallback((cur) => {
    const q = { bucket, limit: PAGE };
    if (focused) {
      const start = new Date(focused.t);
      const end = new Date(start.getTime() + (bucket === 'hour' ? 3600 : 86_400) * 1000 - 1000);
      q.since = iso(start); q.until = iso(end);
    } else { q.since = win.since; q.until = win.until; }
    if (source) q.source = source;
    if (caseId) q.case_id = caseId;
    if (cur) q.cursor = cur;
    return q;
  }, [bucket, focused, win, source, caseId]);

  const load = useCallback(async () => {
    if (!win || tooWide) return;
    const my = ++seq.current;
    setLoading(true);
    try {
      // The histogram always covers the full window even when the list is
      // narrowed to one bucket, so two requests are needed in that state.
      const listQ = buildQuery(null);
      const [listRes, histRes] = await Promise.all([
        api.get('/api/timeline', { query: listQ }),
        focused ? api.get('/api/timeline', { query: { ...listQ, since: win.since, until: win.until, limit: 1 } }) : null,
      ]);
      if (my !== seq.current) return;
      const hist = histRes || listRes;
      setBuckets(Array.isArray(hist?.data?.buckets) ? hist.data.buckets : []);
      setEvents(Array.isArray(listRes?.data?.events) ? listRes.data.events : []);
      setMeta({ ...(hist?.meta || {}), listNotes: listRes?.meta?.notes || [], listFilters: listRes?.meta?.filters });
      setCursor(listRes?.page?.next_cursor || null);
      setError(null);
    } catch (e) {
      if (my === seq.current) setError(e);
    } finally { if (my === seq.current) setLoading(false); }
  }, [win, tooWide, buildQuery, focused]);

  useEffect(() => { load(); }, [load]);

  const loadMore = async () => {
    if (!cursor || loadingMore) return;
    const my = seq.current;
    setLoadingMore(true);
    try {
      const j = await api.get('/api/timeline', { query: buildQuery(cursor) });
      if (my !== seq.current) return;
      setEvents((xs) => [...xs, ...(Array.isArray(j?.data?.events) ? j.data.events : [])]);
      setCursor(j?.page?.next_cursor || null);
    } catch (e) { if (my === seq.current) setError(e); }
    finally { setLoadingMore(false); }
  };

  // Source catalogue for the filter; falls back to sources seen in events.
  const srcCat = useApi('/api/intel/sources');
  const sourceOptions = useMemo(() => {
    const list = Array.isArray(srcCat.data?.data) ? srcCat.data.data : [];
    if (list.length) return list.map((s) => ({ id: s.id, name: s.name || s.id }));
    const seen = new Map();
    for (const e of events) if (e.source_id && !seen.has(e.source_id)) seen.set(e.source_id, e.source_id);
    return [...seen.entries()].map(([id, name]) => ({ id, name }));
  }, [srcCat.data, events]);

  const totals = meta?.totals || buckets.reduce((a, b) => ({ intel: a.intel + (b.intel || 0), mentions: a.mentions + (b.mentions || 0), alerts: a.alerts + (b.alerts || 0) }), { intel: 0, mentions: 0, alerts: 0 });
  const notes = [...(meta?.notes || []), ...(meta?.listNotes || [])].filter((v, i, a) => a.indexOf(v) === i);
  const caseFilterDropped = caseId && notes.some((n) => n === 'case_items_table_missing' || n === 'cases_table_missing');

  const colors = useMemo(() => ({ intel: cssRgb('--accent'), mention: cssRgb('--accent-alt'), alert: cssRgb('--neon-red-rgb'), grid: cssRgb('--osint-border'), text: cssRgb('--osint-text-muted') }), []);

  const chartData = buckets.map((b) => ({ ...b, total: (b.intel || 0) + (b.mentions || 0) + (b.alerts || 0), label: bucketLabel(b.t, meta?.bucket || bucket) }));
  const focusedBucket = focused ? chartData.find((b) => b.t === focused.t) : null;

  return (
    <Page
      wide
      title="Timeline"
      subtitle="Intel items, entity mentions and alert events merged into one temporal stream. Times shown in JST."
      actions={<Button onClick={load} title="Reload timeline"><LuRefreshCw size={13} /></Button>}
    >
      <Card className="space-y-3">
        <div className="flex flex-wrap items-end gap-3">
          <Field label="Window"><Segmented value={windowSel} onChange={(v) => { setWindowSel(v); setFocused(null); }} options={WINDOWS} /></Field>
          <Field label="Histogram"><Segmented value={bucket} onChange={(v) => { setBucket(v); setFocused(null); }} options={[{ value: 'hour', label: 'Hourly' }, { value: 'day', label: 'Daily' }]} /></Field>
          <Field label="Source">
            <select value={source} onChange={(e) => setSource(e.target.value)} className="px-2 py-1 rounded-md bg-osint-bg border border-osint-border text-xs text-osint-text max-w-[240px]">
              <option value="">All sources</option>
              {sourceOptions.map((s) => <option key={s.id} value={s.id}>{s.name}</option>)}
            </select>
          </Field>
          {caseId && (
            <Field label="Case scope">
              <span className="inline-flex items-center gap-1">
                <Link to={`/cases/${encodeURIComponent(caseId)}`} className="font-mono text-xs text-accent hover:underline">{caseId}</Link>
                <button type="button" className="text-osint-muted hover:text-osint-text" title="Clear case scope" onClick={() => { params.delete('case_id'); setParams(params); }}><LuX size={12} /></button>
              </span>
            </Field>
          )}
        </div>
        {windowSel === 'custom' && (
          <div className="flex flex-wrap items-end gap-3">
            <Field label="Since (local time)"><Input type="datetime-local" value={customSince} onChange={(e) => { setCustomSince(e.target.value); setFocused(null); }} mono /></Field>
            <span className="text-osint-muted pb-2">→</span>
            <Field label="Until (local time)"><Input type="datetime-local" value={customUntil} onChange={(e) => { setCustomUntil(e.target.value); setFocused(null); }} mono /></Field>
            {!win && <span className="text-xs text-neon-red pb-2">Since must be before until.</span>}
          </div>
        )}
        {srcCat.error && <div className="text-[11px] text-osint-muted">Source catalogue unavailable ({srcCat.error.message}); the filter lists only sources seen in loaded events.</div>}
        {win && (
          <div className="text-[11px] text-osint-muted font-mono">
            {fmtAbs(win.since)} → {fmtAbs(win.until)} JST
            {meta?.since && meta?.until && !focused && <> · server resolved {meta.since} → {meta.until}</>}
          </div>
        )}
      </Card>

      {tooWide && (
        <ErrorNotice title="Window too wide">
          That range is wider than the {MAX_WINDOW_DAYS} days this server will scan.
          <div className="mt-1"><Button size="sm" variant="danger" onClick={() => { setWindowSel(2_592_000); setFocused(null); }}>Narrow to 30 days</Button></div>
        </ErrorNotice>
      )}
      {error && <ErrorNotice error={error} title="Timeline request failed" onRetry={load} />}
      {caseFilterDropped && (
        <ErrorNotice title="Case scope not applied">
          The server could not apply the case filter on this deployment ({notes.join(', ')}), so it answered with the unscoped window. This is not the same as “the case is empty”.
        </ErrorNotice>
      )}
      {notes.filter((n) => n === 'cursor_ignored').length > 0 && <div className="text-[11px] text-accent">Note from server: a malformed cursor was ignored and page 1 returned.</div>}

      {/* Histogram */}
      <Card>
        <div className="flex flex-wrap items-center gap-3 mb-2">
          {[['intel', totals.intel], ['mention', totals.mentions], ['alert', totals.alerts]].map(([k, n]) => (
            <span key={k} className="inline-flex items-center gap-1.5 text-[11px] font-mono" style={{ color: colors[k] }}>
              <span className="w-2 h-2 rounded-sm" style={{ background: colors[k] }} />{KIND[k].label} <span className="text-osint-text">{n}</span>
            </span>
          ))}
          <span className="ml-auto text-[11px] text-osint-muted font-mono">{buckets.length} buckets · {meta?.bucket || bucket}</span>
        </div>
        {loading && buckets.length === 0 && <LoadingState label="Loading histogram…" />}
        {!loading && !error && buckets.length === 0 && <div className="text-xs text-osint-muted text-center py-6">No histogram for this window.</div>}
        {chartData.length > 0 && (
          <div className="h-40">
            <ResponsiveContainer width="100%" height="100%">
              <BarChart data={chartData} margin={{ top: 4, right: 4, left: -20, bottom: 0 }} onClick={(s) => { const p = s?.activePayload?.[0]?.payload; if (p) setFocused(focused?.t === p.t ? null : { t: p.t }); }}>
                <XAxis dataKey="label" tick={{ fontSize: 10, fill: colors.text, fontFamily: 'JetBrains Mono, monospace' }} interval="preserveStartEnd" axisLine={{ stroke: colors.grid }} tickLine={false} />
                <YAxis tick={{ fontSize: 10, fill: colors.text, fontFamily: 'JetBrains Mono, monospace' }} axisLine={false} tickLine={false} allowDecimals={false} />
                <Tooltip cursor={{ fill: cssRgb('--accent', 0.08) }} contentStyle={{ background: cssRgb('--osint-surface'), border: `1px solid ${colors.grid}`, borderRadius: 6, fontSize: 11, fontFamily: 'JetBrains Mono, monospace', color: cssRgb('--osint-text') }} labelStyle={{ color: cssRgb('--osint-text') }} />
                <Bar dataKey="intel" stackId="a" fill={colors.intel} name="intel" isAnimationActive={false}>
                  {chartData.map((b) => <Cell key={b.t} fillOpacity={focused && focused.t !== b.t ? 0.3 : 1} />)}
                </Bar>
                <Bar dataKey="mentions" stackId="a" fill={colors.mention} name="mentions" isAnimationActive={false}>
                  {chartData.map((b) => <Cell key={b.t} fillOpacity={focused && focused.t !== b.t ? 0.3 : 1} />)}
                </Bar>
                <Bar dataKey="alerts" stackId="a" fill={colors.alert} name="alerts" isAnimationActive={false} radius={[2, 2, 0, 0]}>
                  {chartData.map((b) => <Cell key={b.t} fillOpacity={focused && focused.t !== b.t ? 0.3 : 1} />)}
                </Bar>
              </BarChart>
            </ResponsiveContainer>
          </div>
        )}
        <div className="text-[11px] text-osint-muted mt-1">Click a bar to narrow the event list to that bucket; click it again to widen.</div>
        {focusedBucket && (
          <div className="mt-2 flex items-center gap-2 text-xs text-accent">
            Showing {bucketLabel(focusedBucket.t, meta?.bucket || bucket)} only · {focusedBucket.total} events in that bucket
            <Button size="sm" onClick={() => setFocused(null)}>Show whole window</Button>
          </div>
        )}
      </Card>

      {/* Events */}
      {loading && events.length === 0 && buckets.length > 0 && <LoadingState label="Loading events…" />}
      {!loading && !error && events.length === 0 && (
        <EmptyState title={focused ? 'That histogram bucket has no events at the current filters.' : 'No events in this window.'}
          action={!focused && windowSel !== 2_592_000 && <Button size="sm" onClick={() => setWindowSel(2_592_000)}>Widen to 30 days</Button>} />
      )}
      {events.length > 0 && (
        <Card padded={false}>
          <ul className="divide-y divide-osint-border">
            {events.map((ev) => <EventRow key={ev.key || `${ev.kind}:${ev.uid}:${ev.ts}`} ev={ev} colors={colors} />)}
          </ul>
        </Card>
      )}
      {events.length > 0 && (
        <div className="flex items-center justify-between">
          <span className={cx('text-[11px] font-mono', cursor ? 'text-accent' : 'text-osint-muted')}>
            {cursor ? `showing ${events.length} loaded · older events on the server (keyset paging, no total)` : `${events.length} events · end of window`}
          </span>
          {cursor && <Button busy={loadingMore} onClick={loadMore}>Load older events</Button>}
        </div>
      )}
    </Page>
  );
}

function EventRow({ ev, colors }) {
  const navigate = useNavigate();
  const k = KIND[ev.kind] || { label: (ev.kind || 'EVENT').toUpperCase(), icon: LuFileText, css: '--osint-text-muted' };
  const Icon = k.icon;
  const color = colors[ev.kind] || cssRgb('--osint-text-muted');
  const hasCoord = Number.isFinite(Number(ev.lat)) && Number.isFinite(Number(ev.lon));
  const itemLink = ev.uid ? `/intel/items/${encodeURIComponent(ev.uid)}` : null;
  return (
    <li className="px-3 py-2 flex items-start gap-3">
      <span className="flex items-center justify-center w-7 h-7 rounded-md flex-shrink-0 mt-0.5" style={{ color, background: `color-mix(in srgb, ${color} 14%, transparent)` }} title={ev.kind}>
        <Icon size={14} />
      </span>
      <div className="min-w-0 flex-1">
        {itemLink
          ? <Link to={itemLink} className="text-sm text-osint-text hover:text-accent block truncate">{ev.title || ev.uid}</Link>
          : <div className="text-sm text-osint-text truncate">{ev.title || '(untitled)'}</div>}
        <div className="flex flex-wrap items-center gap-x-2 gap-y-0.5 text-[11px] font-mono mt-0.5">
          <span style={{ color }}>{k.label}</span>
          {ev.source_id && <span className="text-osint-muted">· {ev.source_id}</span>}
          <span className="text-osint-muted">· {fmtAbs(ev.ts)}</span>
          {ev.kind === 'mention' && ev.ref && <span className="text-osint-muted">· entity {ev.ref}</span>}
          {ev.kind === 'alert' && ev.ref && <Link to="/console/alerts" className="text-osint-muted hover:text-accent">· rule {ev.ref}</Link>}
        </div>
      </div>
      <div className="flex items-center gap-1 flex-shrink-0">
        {hasCoord && (
          <Button size="sm" title="Show on map" onClick={() => { navigate('/'); setTimeout(() => window.dispatchEvent(new CustomEvent('japanosint:flyto', { detail: { lat: Number(ev.lat), lon: Number(ev.lon), zoom: 14 } })), 150); }}>
            <LuMapPin size={12} />
          </Button>
        )}
        {ev.uid && <CopyButton text={ev.uid} label="uid" />}
      </div>
    </li>
  );
}

