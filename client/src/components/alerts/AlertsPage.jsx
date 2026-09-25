import React, { useEffect, useMemo, useState } from 'react';
import { Link, useSearchParams } from 'react-router-dom';
import { LuBellPlus, LuRefreshCw, LuPencil, LuTrash2, LuFlaskConical, LuHistory, LuBellOff, LuBell } from 'react-icons/lu';
import { api, errorMessage } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { relativeTime, fmtAbs } from '../../utils/time.js';
import {
  Page, Section, Card, Pill, Button, Input, TextArea, Field, Select, Toggle, Segmented, Sheet,
  ConfirmDialog, ErrorNotice, EmptyState, LoadingState, BoundNote, KV, toast, cx,
} from '../ui/kit.jsx';
import {
  ChannelsEditor, ChannelPills, validateChannels, MASKED, isMuted, muteLabel, MUTE_OPTIONS,
  splitList, joinList, predicateSummary, emptyChannel,
} from './alertShared.jsx';

/**
 * Alert rules — port of iOS `AlertsTab` + `AlertEditor` + `RulePreviewSection`
 * + `AlertEventsView`. Endpoints: GET/POST /api/alerts, PATCH/DELETE
 * /api/alerts/:id, POST /:id/mute|unmute|test, GET /:id/events, POST
 * /api/alerts/preview.
 */
export default function AlertsPage() {
  const { data, error, loading, reload } = useApi('/api/alerts');
  const rules = Array.isArray(data?.data) ? data.data : [];
  const [editing, setEditing] = useState(null);   // null | 'new' | rule
  const [confirmDel, setConfirmDel] = useState(null);
  const [busyId, setBusyId] = useState(null);
  const [historyFor, setHistoryFor] = useState(null);
  const [testResult, setTestResult] = useState(null);
  // Deep link from Areas of interest: /console/alerts?aoi=<id> opens a new rule on that area.
  // Also from Workspace › Queries: /console/alerts?new=1&mode=fts|llm&q=…&nl_query=…
  const [params] = useSearchParams();
  const presetAoi = params.get('aoi');
  const presetNew = params.get('new');
  const preset = useMemo(() => {
    if (!presetAoi && !presetNew) return null;
    const p = {};
    if (presetAoi) p.aoi_id = presetAoi;
    if (params.get('mode') === 'llm' || params.get('mode') === 'fts') p.mode = params.get('mode');
    if (params.get('q')) p.q = params.get('q');
    if (params.get('nl_query')) p.nl_query = params.get('nl_query');
    if (params.get('name')) p.name = params.get('name');
    return p;
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [presetAoi, presetNew, params.toString()]);
  useEffect(() => { if (preset) setEditing('new'); }, [preset]);

  const act = async (id, fn, okMsg) => {
    setBusyId(id);
    try { await fn(); if (okMsg) toast(okMsg, { tone: 'accent' }); await reload({ silent: true }); }
    catch (e) { toast(errorMessage(e), { tone: 'danger', ttl: 6000 }); }
    finally { setBusyId(null); }
  };

  const toggleEnabled = (r) => act(r.id, () => api.patch(`/api/alerts/${encodeURIComponent(r.id)}`, { enabled: !r.enabled }));
  const mute = (r, dur) => act(r.id, () => api.post(`/api/alerts/${encodeURIComponent(r.id)}/mute`, { duration_sec: dur }), 'Rule muted');
  const unmute = (r) => act(r.id, () => api.post(`/api/alerts/${encodeURIComponent(r.id)}/unmute`), 'Rule unmuted');
  const test = (r) => act(r.id, async () => {
    const res = await api.post(`/api/alerts/${encodeURIComponent(r.id)}/test`);
    setTestResult({ rule: r, res });
  });
  const del = () => {
    const r = confirmDel; setConfirmDel(null);
    return act(r.id, () => api.del(`/api/alerts/${encodeURIComponent(r.id)}`), 'Rule deleted');
  };

  return (
    <Page
      title="Alerts"
      subtitle="Rules that watch new intel and deliver matches to email or webhook channels."
      actions={(
        <>
          <Button onClick={() => reload()} title="Reload alert rules"><LuRefreshCw size={13} /></Button>
          <Button variant="primary" onClick={() => setEditing('new')}><LuBellPlus size={14} /> New alert</Button>
        </>
      )}
    >
      {error && <ErrorNotice error={error} title="Could not load alert rules" onRetry={reload} />}
      {loading && !data && <LoadingState label="Loading rules…" />}

      {!loading && !error && rules.length === 0 && (
        <EmptyState title="No alert rules yet" action={<Button variant="primary" onClick={() => setEditing('new')}>Create your first rule</Button>}>
          A rule is an FTS query plus optional source, tag, entity and spatial filters. Every new item that matches is delivered once per dedup window.
        </EmptyState>
      )}

      {rules.length > 0 && (
        <div className="space-y-2">
          {rules.map((r) => (
            <RuleCard
              key={r.id}
              rule={r}
              busy={busyId === r.id}
              onEdit={() => setEditing(r)}
              onToggle={() => toggleEnabled(r)}
              onMute={(d) => mute(r, d)}
              onUnmute={() => unmute(r)}
              onTest={() => test(r)}
              onDelete={() => setConfirmDel(r)}
              onHistory={() => setHistoryFor(historyFor?.id === r.id ? null : r)}
              historyOpen={historyFor?.id === r.id}
            />
          ))}
          <BoundNote shown={rules.length} total={rules.length} noun="rules" />
        </div>
      )}

      <RuleEditorSheet
        open={editing != null}
        rule={editing === 'new' ? null : editing}
        preset={editing === 'new' ? preset : null}
        onClose={() => setEditing(null)}
        onSaved={() => { setEditing(null); reload({ silent: true }); }}
      />

      <ConfirmDialog
        open={confirmDel != null}
        onClose={() => setConfirmDel(null)}
        onConfirm={del}
        title={`Delete “${confirmDel?.name || ''}”?`}
        confirmLabel="Delete"
        message="Deletes the rule and its firing history. Inbox events already delivered are kept."
      />

      <TestResultSheet result={testResult} onClose={() => setTestResult(null)} />
    </Page>
  );
}

function RuleCard({ rule: r, busy, onEdit, onToggle, onMute, onUnmute, onTest, onDelete, onHistory, historyOpen }) {
  const muted = isMuted(r.muted_until);
  return (
    <Card padded={false} className={cx(!r.enabled && 'opacity-70')}>
      <div className="p-3 flex items-start gap-3 flex-wrap">
        <div className="min-w-0 flex-1">
          <div className="flex items-center gap-2 flex-wrap">
            <span className="text-sm font-medium text-osint-text truncate">{r.name}</span>
            {!r.enabled && <Pill>disabled</Pill>}
            {muted && <Pill tone="warning">{muteLabel(r.muted_until)}</Pill>}
            {r.predicate?.mode === 'llm' && <Pill tone="purple">LLM</Pill>}
          </div>
          <div className="text-[11px] text-osint-muted font-mono mt-0.5 break-words">{predicateSummary(r.predicate)}</div>
          <div className="flex items-center gap-1 flex-wrap mt-1.5">
            <ChannelPills channels={r.channels} />
            <Pill title="dedup window">dedup {r.dedup_window_sec ?? 3600}s</Pill>
            <Pill title="storm cap">≤ {r.storm_cap_per_hour ?? 100}/h</Pill>
            {r.updated_at && <span className="text-[10px] text-osint-muted font-mono ml-1">updated {relativeTime(r.updated_at)}</span>}
          </div>
        </div>
        <div className="flex items-center gap-1 flex-wrap">
          <Toggle on={Boolean(r.enabled)} onChange={onToggle} disabled={busy} />
          <Button size="sm" onClick={onEdit} title="Edit"><LuPencil size={12} /></Button>
          <Button size="sm" onClick={onTest} busy={busy} title="Test fire through the real channels"><LuFlaskConical size={12} /> Test</Button>
          {muted ? (
            <Button size="sm" onClick={onUnmute} title="Unmute"><LuBell size={12} /> Unmute</Button>
          ) : (
            <Select className="text-[11px] py-1" value="" onChange={(e) => { if (e.target.value) onMute(e.target.value === 'forever' ? 'forever' : Number(e.target.value)); }} title="Mute this rule for…">
              <option value="">Mute…</option>
              {MUTE_OPTIONS.map((o) => <option key={String(o.value)} value={String(o.value)}>{o.label}</option>)}
            </Select>
          )}
          <Button size="sm" onClick={onHistory} className={cx(historyOpen && 'border-accent/50 text-accent')} title="Firing history"><LuHistory size={12} /> History</Button>
          <Button size="sm" variant="ghost" onClick={onDelete} title="Delete"><LuTrash2 size={12} /></Button>
        </div>
      </div>
      {historyOpen && <RuleEvents rule={r} />}
    </Card>
  );
}

/** GET /api/alerts/:id/events → {data:[…], page:{next_cursor,limit,total}} */
function RuleEvents({ rule }) {
  const [rows, setRows] = useState([]);
  const [page, setPage] = useState(null);
  const [error, setError] = useState(null);
  const [loading, setLoading] = useState(true);

  const load = async (cursor) => {
    setLoading(true); setError(null);
    try {
      const j = await api.get(`/api/alerts/${encodeURIComponent(rule.id)}/events`, { query: { limit: 100, cursor } });
      const d = Array.isArray(j?.data) ? j.data : [];
      setRows((xs) => (cursor ? [...xs, ...d] : d));
      setPage(j?.page || null);
    } catch (e) { setError(e); }
    finally { setLoading(false); }
  };
  useEffect(() => { load(); /* eslint-disable-line react-hooks/exhaustive-deps */ }, [rule.id]);

  return (
    <div className="border-t border-osint-border p-3 space-y-2 bg-osint-bg/40">
      <div className="font-mono text-[10px] uppercase tracking-[0.12em] text-osint-muted">Firing history</div>
      {error && <ErrorNotice error={error} title="Could not load history" onRetry={() => load()} />}
      {loading && rows.length === 0 && <LoadingState label="Loading events…" />}
      {!loading && !error && rows.length === 0 && <div className="text-xs text-osint-muted">This rule has not fired yet.</div>}
      {rows.length > 0 && (
        <ul className="divide-y divide-osint-border">
          {rows.map((ev) => (
            <li key={ev.id} className="py-1.5 flex items-start gap-2 text-xs">
              <span className="font-mono text-osint-muted whitespace-nowrap" title={fmtAbs(ev.matched_at)}>{relativeTime(ev.matched_at)}</span>
              <span className="min-w-0 flex-1">
                <Link to={`/intel/items/${encodeURIComponent(ev.item_uid)}`} className="text-osint-text hover:text-accent">{ev.item_title || ev.item_uid}</Link>
                {ev.item_source_id && <span className="text-osint-muted"> · {ev.item_source_id}</span>}
                {ev.suppressed ? <Pill tone="warning" className="ml-1">suppressed{ev.reason ? `: ${ev.reason}` : ''}</Pill> : null}
              </span>
              <span className="font-mono text-[10px] text-osint-muted">{(ev.delivered_channels || []).join(', ') || '—'}</span>
            </li>
          ))}
        </ul>
      )}
      <div className="flex items-center justify-between">
        <BoundNote shown={rows.length} total={page?.total ?? rows.length} noun="events" />
        {page?.next_cursor && <Button size="sm" busy={loading} onClick={() => load(page.next_cursor)}>Load more</Button>}
      </div>
    </div>
  );
}

function TestResultSheet({ result, onClose }) {
  if (!result) return null;
  const { rule, res } = result;
  const results = res?.data?.results || [];
  const bad = results.filter((x) => x.status !== 'ok');
  return (
    <Sheet open onClose={onClose} title={`Test fire — ${rule.name}`} width="max-w-md" footer={<Button onClick={onClose}>OK</Button>}>
      <div className="space-y-2 text-xs">
        <div className={cx('font-medium', bad.length ? 'text-neon-red' : 'text-neon-green')}>
          {bad.length === 0 ? (results.length === 1 ? 'Channel delivered' : `All ${results.length} channels delivered`) : `${bad.length} of ${results.length} channel(s) failed`}
        </div>
        <ul className="divide-y divide-osint-border rounded-md border border-osint-border">
          {results.map((c) => (
            <li key={c.channel_idx} className="px-2 py-1.5 flex items-center gap-2">
              <Pill tone={c.status === 'ok' ? 'success' : c.status === 'skipped' ? 'neutral' : 'danger'}>{c.status}</Pill>
              <span className="font-mono truncate flex-1">{c.type} {c.target}</span>
              {c.http_code != null && <span className="font-mono text-osint-muted">HTTP {c.http_code}</span>}
              {c.error && <span className="text-neon-red truncate max-w-[40%]" title={c.error}>{c.error}</span>}
            </li>
          ))}
        </ul>
        {res?.data?.event_id && <div className="text-osint-muted font-mono">event {res.data.event_id}</div>}
      </div>
    </Sheet>
  );
}

/* ───────────────────────────── editor ───────────────────────────── */

function draftFrom(rule) {
  const p = rule?.predicate || {};
  const spatial = p.aoi_id ? 'aoi' : p.bbox ? 'bbox' : p.circle ? 'circle' : p.polygon ? 'polygon' : 'none';
  return {
    name: rule?.name || '',
    enabled: rule ? Boolean(rule.enabled) : true,
    mode: p.mode === 'llm' ? 'llm' : 'fts',
    q: p.q || '',
    nl_query: p.nl_query || '',
    source_ids: joinList(p.source_ids),
    tags_any: joinList(p.tags_any),
    tags_all: joinList(p.tags_all),
    entity_types: joinList(p.entity_types),
    entity_ids: joinList(p.entity_ids),
    spatial,
    aoi_id: p.aoi_id || '',
    bbox: Array.isArray(p.bbox) ? p.bbox.join(', ') : '',
    circle: p.circle ? { lat: String(p.circle.lat), lon: String(p.circle.lon), radius_m: String(p.circle.radius_m) } : { lat: '', lon: '', radius_m: '1000' },
    polygon: p.polygon ? JSON.stringify(p.polygon) : '',
    record_types: p.record_types,           // not enforced by the server; round-trips untouched
    channels: Array.isArray(rule?.channels) && rule.channels.length ? rule.channels.map((c) => ({ ...c })) : [emptyChannel('email')],
    dedup_window_sec: String(rule?.dedup_window_sec ?? 3600),
    storm_cap_per_hour: String(rule?.storm_cap_per_hour ?? 100),
  };
}

/** Build the predicate object the server validates. Returns [predicate, error]. */
function buildPredicate(d) {
  const p = {};
  if (d.mode === 'llm') {
    p.mode = 'llm';
    if (d.nl_query.trim()) p.nl_query = d.nl_query.trim();
    if (d.q.trim()) p.q = d.q.trim();
  } else if (d.q.trim()) p.q = d.q.trim();
  const si = splitList(d.source_ids); if (si.length) p.source_ids = si;
  const ta = splitList(d.tags_any); if (ta.length) p.tags_any = ta;
  const tl = splitList(d.tags_all); if (tl.length) p.tags_all = tl;
  const et = splitList(d.entity_types); if (et.length) p.entity_types = et;
  const ei = splitList(d.entity_ids); if (ei.length) p.entity_ids = ei;
  if (d.record_types) p.record_types = d.record_types;
  if (d.spatial === 'aoi') {
    if (!d.aoi_id) return [null, 'Pick an area of interest.'];
    p.aoi_id = d.aoi_id;
  } else if (d.spatial === 'bbox') {
    const nums = splitList(d.bbox).map(Number);
    if (nums.length !== 4 || nums.some((n) => !Number.isFinite(n))) return [null, 'bbox must be four numbers: w, s, e, n'];
    p.bbox = nums;
  } else if (d.spatial === 'circle') {
    const lat = Number(d.circle.lat), lon = Number(d.circle.lon), r = Number(d.circle.radius_m);
    if (![lat, lon, r].every(Number.isFinite) || r <= 0) return [null, 'circle needs lat, lon and a positive radius (m)'];
    p.circle = { lat, lon, radius_m: r };
  } else if (d.spatial === 'polygon') {
    try {
      const ring = JSON.parse(d.polygon);
      if (!Array.isArray(ring) || ring.length < 3) return [null, 'polygon must be a JSON ring of at least 3 [lon, lat] points'];
      p.polygon = ring;
    } catch { return [null, 'polygon is not valid JSON'] ; }
  }
  return [p, null];
}

function channelsDirty(draft, original) {
  const a = JSON.stringify(draft.map((c) => ({ type: c.type, target: c.target, secret: c.secret })));
  const b = JSON.stringify((original || []).map((c) => ({ type: c.type, target: c.target, secret: c.secret })));
  return a !== b;
}

function RuleEditorSheet({ open, rule, onClose, onSaved, preset }) {
  const [d, setD] = useState(() => draftFrom(rule));
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState(null);
  const aois = useApi(open ? '/api/aoi?limit=200' : null, { deps: [open] });
  const aoiRows = Array.isArray(aois.data?.data) ? aois.data.data : [];

  useEffect(() => {
    if (!open) return;
    const base = draftFrom(rule);
    if (preset && !rule) {
      const next = { ...base };
      if (preset.aoi_id) { next.spatial = 'aoi'; next.aoi_id = preset.aoi_id; }
      if (preset.mode) next.mode = preset.mode;
      if (preset.q) next.q = preset.q;
      if (preset.nl_query) next.nl_query = preset.nl_query;
      if (preset.name) next.name = preset.name;
      setD(next);
    } else {
      setD(base);
    }
    setError(null);
  }, [open, rule, preset]);

  const set = (patch) => setD((x) => ({ ...x, ...patch }));
  const [predicate, predErr] = useMemo(() => buildPredicate(d), [d]);

  const save = async () => {
    setError(null);
    if (!d.name.trim()) { setError(new Error('Name is required.')); return; }
    if (predErr) { setError(new Error(predErr)); return; }
    const dirty = !rule || channelsDirty(d.channels, rule.channels);
    if (dirty) {
      const ce = validateChannels(d.channels);
      if (ce) { setError(new Error(ce)); return; }
    }
    const body = {
      name: d.name.trim(),
      enabled: d.enabled,
      predicate,
      dedup_window_sec: Number(d.dedup_window_sec) || 0,
      storm_cap_per_hour: Number(d.storm_cap_per_hour) || 1,
    };
    // The server re-validates the whole channel array on PATCH and masks
    // stored webhook secrets on read, so an untouched list must be OMITTED.
    if (dirty) body.channels = d.channels.map((c) => (c.type === 'webhook' ? c : { type: 'email', target: c.target }));
    setSaving(true);
    try {
      if (rule) await api.patch(`/api/alerts/${encodeURIComponent(rule.id)}`, body);
      else await api.post('/api/alerts', body);
      toast(rule ? 'Rule updated' : 'Rule created', { tone: 'accent' });
      onSaved();
    } catch (e) { setError(e); }
    finally { setSaving(false); }
  };

  return (
    <Sheet
      open={open}
      onClose={onClose}
      title={rule ? 'Edit alert rule' : 'New alert rule'}
      width="max-w-2xl"
      footer={(
        <>
          <Button onClick={onClose}>Cancel</Button>
          <Button variant="primary" busy={saving} onClick={save}>{rule ? 'Save' : 'Create'}</Button>
        </>
      )}
    >
      <div className="space-y-4">
        <Section label="Rule">
          <div className="space-y-3">
            <Field label="Name"><Input value={d.name} onChange={(e) => set({ name: e.target.value })} placeholder="Name" maxLength={200} /></Field>
            <Toggle label="Enabled" on={d.enabled} onChange={(v) => set({ enabled: v })} />
          </div>
        </Section>

        <Section label="Match when…">
          <div className="space-y-3">
            <div className="flex items-center justify-between gap-2 flex-wrap">
              <span className="text-xs text-osint-muted">Query type</span>
              <Segmented value={d.mode} onChange={(v) => set({ mode: v })} options={[{ value: 'fts', label: 'FTS' }, { value: 'llm', label: 'LLM pipeline' }]} />
            </div>
            {d.mode === 'llm' ? (
              <>
                <Field label="Natural-language query" hint="Drives the agentic search pipeline. LLM-mode rules cannot be previewed and do not fire on new items until the pipeline produces an FTS query.">
                  <Input value={d.nl_query} onChange={(e) => set({ nl_query: e.target.value })} placeholder="Natural-language query (e.g. phishing targeting JP banks)" />
                </Field>
                <Field label="FTS query (chosen by the pipeline, editable)">
                  <Input mono value={d.q} onChange={(e) => set({ q: e.target.value })} placeholder="FTS query" />
                </Field>
              </>
            ) : (
              <Field label="FTS query">
                <Input mono value={d.q} onChange={(e) => set({ q: e.target.value })} placeholder="FTS query (e.g. phishing AND tld:.jp)" />
              </Field>
            )}
          </div>
        </Section>

        <Section label="Also matching">
          <div className="grid md:grid-cols-2 gap-3">
            <Field label="Source IDs"><Input mono value={d.source_ids} onChange={(e) => set({ source_ids: e.target.value })} placeholder="Source IDs (comma-separated)" /></Field>
            <Field label="Tags any-of"><Input mono value={d.tags_any} onChange={(e) => set({ tags_any: e.target.value })} placeholder="Tags any-of (comma-separated)" /></Field>
            <Field label="Tags all-of"><Input mono value={d.tags_all} onChange={(e) => set({ tags_all: e.target.value })} placeholder="Tags all-of (comma-separated)" /></Field>
            <Field label="Entity types"><Input mono value={d.entity_types} onChange={(e) => set({ entity_types: e.target.value })} placeholder="person, org, domain… (comma-separated)" /></Field>
            <Field label="Entity IDs" hint="Watchlists manage this for you — see Console → Watchlists." className="md:col-span-2">
              <Input mono value={d.entity_ids} onChange={(e) => set({ entity_ids: e.target.value })} placeholder="entity ids (comma-separated)" />
            </Field>
          </div>
        </Section>

        <Section label="Where" right={<Segmented size="sm" value={d.spatial} onChange={(v) => set({ spatial: v })} options={[{ value: 'none', label: 'Anywhere' }, { value: 'aoi', label: 'Area' }, { value: 'bbox', label: 'bbox' }, { value: 'circle', label: 'Circle' }, { value: 'polygon', label: 'Polygon' }]} />}>
          {d.spatial === 'none' && <div className="text-xs text-osint-muted">No spatial filter. At most one spatial term is allowed per rule.</div>}
          {d.spatial === 'aoi' && (
            <div className="space-y-2">
              {aois.error && <ErrorNotice error={aois.error} title="Could not load areas" onRetry={aois.reload} />}
              <Select className="w-full" value={d.aoi_id} onChange={(e) => set({ aoi_id: e.target.value })}>
                <option value="">— pick a saved area —</option>
                {aoiRows.map((a) => <option key={a.id} value={a.id}>{a.name} ({a.kind})</option>)}
              </Select>
              <div className="text-[11px] text-osint-muted">Manage areas under <Link className="text-accent" to="/console/aoi">Console → Areas of interest</Link>.</div>
            </div>
          )}
          {d.spatial === 'bbox' && <Field label="bbox [w, s, e, n]"><Input mono value={d.bbox} onChange={(e) => set({ bbox: e.target.value })} placeholder="139.5, 35.5, 140.0, 35.9" /></Field>}
          {d.spatial === 'circle' && (
            <div className="grid grid-cols-3 gap-2">
              <Field label="lat"><Input mono value={d.circle.lat} onChange={(e) => set({ circle: { ...d.circle, lat: e.target.value } })} /></Field>
              <Field label="lon"><Input mono value={d.circle.lon} onChange={(e) => set({ circle: { ...d.circle, lon: e.target.value } })} /></Field>
              <Field label="radius (m)"><Input mono value={d.circle.radius_m} onChange={(e) => set({ circle: { ...d.circle, radius_m: e.target.value } })} /></Field>
            </div>
          )}
          {d.spatial === 'polygon' && <Field label="Ring of [lon, lat] points (JSON)"><TextArea mono value={d.polygon} onChange={(e) => set({ polygon: e.target.value })} placeholder="[[139.6,35.6],[139.9,35.6],[139.9,35.8],[139.6,35.6]]" /></Field>}
          {predErr && <div className="text-[11px] text-neon-red mt-1">{predErr}</div>}
        </Section>

        <PreviewSection predicate={predicate} mode={d.mode} enabled={open} />

        <Section label="Deliver to">
          <ChannelsEditor channels={d.channels} onChange={(channels) => set({ channels })} />
          {rule && !channelsDirty(d.channels, rule.channels) && (
            <div className="text-[11px] text-osint-muted mt-2">Channels unchanged — they will be left as stored.</div>
          )}
        </Section>

        <Section label="Throttle">
          <div className="grid grid-cols-2 gap-3">
            <Field label="Dedup window (seconds)" hint="An item that matches again inside this window is not re-delivered.">
              <Input mono type="number" min={0} value={d.dedup_window_sec} onChange={(e) => set({ dedup_window_sec: e.target.value })} />
            </Field>
            <Field label="Max fires / hour" hint="Storm cap. Extra matches are recorded as suppressed.">
              <Input mono type="number" min={1} value={d.storm_cap_per_hour} onChange={(e) => set({ storm_cap_per_hour: e.target.value })} />
            </Field>
          </div>
        </Section>

        {error && <ErrorNotice error={error} title="Could not save rule" />}
      </div>
    </Sheet>
  );
}

/**
 * Backtest — POST /api/alerts/preview {predicate, since?} →
 * {data:[{uid,title,source_id,published_at}], meta:{match_count,scanned,truncated,since,skipped}}
 */
export function PreviewSection({ predicate, mode, enabled = true }) {
  const [state, setState] = useState({ status: 'idle' });
  const key = JSON.stringify(predicate || {});
  const isLlm = mode === 'llm';
  const empty = !predicate || Object.keys(predicate).filter((k) => k !== 'mode').length === 0;

  useEffect(() => {
    if (!enabled || isLlm || empty) { setState({ status: 'idle' }); return undefined; }
    let alive = true;
    setState({ status: 'loading' });
    const t = setTimeout(async () => {
      try {
        const j = await api.post('/api/alerts/preview', { predicate: JSON.parse(key) });
        if (alive) setState({ status: 'ok', data: Array.isArray(j?.data) ? j.data : [], meta: j?.meta || null });
      } catch (e) { if (alive) setState({ status: 'error', error: e }); }
    }, 600);
    return () => { alive = false; clearTimeout(t); };
  }, [key, enabled, isLlm, empty]);

  return (
    <Section label="Backtest" right={state.status === 'loading' ? <span className="text-[11px] text-osint-muted">Backtesting…</span> : null}>
      {isLlm && <div className="text-xs text-osint-muted">LLM-mode rules can't be previewed — and they don't fire until the pipeline has produced an FTS query.</div>}
      {!isLlm && empty && <div className="text-xs text-osint-muted">Edit the query above to see what it would have matched.</div>}
      {state.status === 'error' && <ErrorNotice error={state.error} title="Preview failed" />}
      {state.status === 'ok' && (
        <div className="space-y-2">
          {state.meta?.skipped && <div className="text-xs text-accent">Preview skipped: {String(state.meta.skipped)}</div>}
          {state.meta?.match_count == null && !state.meta?.skipped && <div className="text-xs text-osint-muted">The server answered, but did not report a match count.</div>}
          {state.meta?.match_count != null && (
            <div className="text-xs text-osint-text">
              <span className="font-mono text-accent text-base">{state.meta.match_count}</span> match{state.meta.match_count === 1 ? '' : 'es'}
              {state.meta.scanned != null && <span className="text-osint-muted"> · scanned {state.meta.scanned} item{state.meta.scanned === 1 ? '' : 's'}</span>}
              {state.meta.since && <span className="text-osint-muted"> since {state.meta.since}</span>}
              {state.meta.truncated && <Pill tone="warning" className="ml-1">scan truncated</Pill>}
            </div>
          )}
          {state.data.length > 0 && (
            <div>
              <div className="text-[11px] text-osint-muted mb-1">Sample matches ({state.data.length})</div>
              <ul className="divide-y divide-osint-border rounded-md border border-osint-border">
                {state.data.map((m) => (
                  <li key={m.uid} className="px-2 py-1 text-xs flex items-center gap-2">
                    <Link to={`/intel/items/${encodeURIComponent(m.uid)}`} className="text-osint-text hover:text-accent truncate flex-1">{m.title || m.uid}</Link>
                    {m.source_id && <span className="font-mono text-osint-muted">{m.source_id}</span>}
                    {m.published_at && <span className="font-mono text-osint-muted">{relativeTime(m.published_at)}</span>}
                  </li>
                ))}
              </ul>
            </div>
          )}
        </div>
      )}
    </Section>
  );
}
