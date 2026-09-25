import React, { useEffect, useMemo, useRef, useState } from 'react';
import { Link, useSearchParams } from 'react-router-dom';
import { LuDatabase, LuClock, LuScrollText, LuKey, LuRefreshCw, LuPlay, LuLockOpen, LuWrench } from 'react-icons/lu';
import { api, ApiError } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { useAuth } from '../../auth/AuthContext.jsx';
import {
  Page, Section, Row, Card, Pill, Button, Input, Field, Segmented, Sheet, Toggle,
  ConfirmDialog, ErrorNotice, EmptyState, LoadingState, BoundNote, KV, Spinner, cx, toast,
} from '../ui/kit.jsx';

/**
 * Admin panel — port of the iOS `AdminPanel` + `RepairWorkerView` +
 * `SourcePipelineView` + `BreachCorpusView`. Every route here sits behind the
 * server's platform-operator gate; a 403 is rendered as the server's refusal.
 */

const TABS = [
  { value: 'maintenance', label: 'Maintenance' },
  { value: 'breach', label: 'Breach corpus' },
  { value: 'pagination', label: 'Pagination' },
  { value: 'server', label: 'Server' },
];

function shortTime(s) {
  if (!s) return '—';
  const t = String(s).replace('T', ' ');
  const [d, tm] = t.split(' ');
  if (!tm) return t;
  const parts = d.split('-');
  return parts.length === 3 ? `${parts[1]}-${parts[2]} ${tm.slice(0, 5)}` : `${d} ${tm.slice(0, 5)}`;
}

function opError(e) {
  if (e instanceof ApiError && e.status === 403) return 'The server refused this account (platform-operator allowlist)';
  if (e instanceof ApiError && e.status === 409) return 'A job is already running — wait for it to finish';
  return null;
}

export default function AdminPage() {
  const auth = useAuth();
  const [params, setParams] = useSearchParams();
  const tab = TABS.some((t) => t.value === params.get('tab')) ? params.get('tab') : 'maintenance';
  const setTab = (t) => setParams((p) => { const n = new URLSearchParams(p); n.set('tab', t); return n; }, { replace: true });
  const [pipelineId, setPipelineId] = useState(params.get('source') || null);

  return (
    <Page
      title="Admin panel"
      subtitle="Operator tools that affect every workspace on this host: the self-healing pipeline, the breach corpus, the pagination survey and the server itself."
      actions={<Segmented value={tab} onChange={setTab} options={TABS} />}
      wide
    >
      <OperatorLinks />
      {tab === 'maintenance' && <MaintenanceTab onOpenSource={setPipelineId} />}
      {tab === 'breach' && <BreachTab />}
      {tab === 'pagination' && <PaginationTab />}
      {tab === 'server' && <ServerTab />}
      <SourcePipelineSheet sourceId={pipelineId} onClose={() => setPipelineId(null)} />
      {auth.isPlatformAdmin === false && (
        <ErrorNotice title="The server answered 403 for this account on an operator route">Everything on this page will be refused until the account is listed in PLATFORM_OPERATOR_EMAILS / PLATFORM_OPERATOR_IDS.</ErrorNotice>
      )}
    </Page>
  );
}

function OperatorLinks() {
  const links = [
    { to: '/console/database', icon: LuDatabase, label: 'Database', sub: 'Browse collected tables' },
    { to: '/console/scheduler', icon: LuClock, label: 'Scheduler', sub: 'Collection cadence' },
    { to: '/console/settings', icon: LuWrench, label: 'Source scheduling', sub: 'Per-source map-cron vs search-only' },
    { to: '/console/follow', icon: LuScrollText, label: 'Follow log', sub: 'Live activity stream' },
    { to: '/console/api-keys?scope=platform', icon: LuKey, label: 'Platform API keys', sub: 'Server-wide default credentials' },
  ];
  return (
    <div className="grid grid-cols-2 md:grid-cols-5 gap-2">
      {links.map((l) => (
        <Link key={l.to} to={l.to} className="flex items-center gap-2 rounded-[10px] border border-osint-border bg-osint-surface px-3 py-2 hover:border-accent/50">
          <span className="flex items-center justify-center w-7 h-7 rounded-md bg-accent/10 text-accent flex-shrink-0"><l.icon size={14} /></span>
          <span className="min-w-0">
            <span className="block text-xs font-medium text-osint-text truncate">{l.label}</span>
            <span className="block text-[10px] text-osint-muted truncate">{l.sub}</span>
          </span>
        </Link>
      ))}
    </div>
  );
}

/* ============================================================ maintenance */

const WINDOWS = [{ value: 24, label: '24h' }, { value: 72, label: '3d' }, { value: 168, label: '7d' }];
const STATUS_TONE = { merged: 'success', verified: 'warning', rejected: 'neutral', needs_human: 'cyan', error: 'danger' };

function parsePatch(patch) {
  if (!patch) return null;
  if (typeof patch === 'object') return patch;
  try { return JSON.parse(patch); } catch { return null; }
}

function StatCard({ label, value, tone }) {
  const color = { success: 'text-neon-green', warning: 'text-accent', cyan: 'text-neon-cyan', danger: 'text-neon-red', neutral: 'text-osint-muted' }[tone] || 'text-osint-text';
  return (
    <Card className="text-center py-3">
      <div className={cx('font-mono text-xl font-bold', color)}>{value ?? 0}</div>
      <div className="text-[10px] text-osint-muted">{label}</div>
    </Card>
  );
}

function UrlDiff({ oldUrl, newUrl }) {
  if (!oldUrl && !newUrl) return null;
  return (
    <div className="rounded-md bg-osint-bg border border-osint-border p-2 font-mono text-[11px] space-y-0.5 break-all">
      {oldUrl && <div className="text-neon-red">− {oldUrl}</div>}
      {newUrl && <div className="text-neon-green">+ {newUrl}</div>}
    </div>
  );
}

export function RepairCard({ row, busy, onApprove, onReject, onRevert, onOpenSource }) {
  const [confirm, setConfirm] = useState(false);
  const swap = parsePatch(row.patch);
  const approvable = row.status === 'verified' && row.action === 'url_swap';
  return (
    <Card className="space-y-2">
      <div className="flex items-center gap-2 flex-wrap">
        <Pill tone={STATUS_TONE[row.status] || 'neutral'}>{String(row.status || '?').toUpperCase()}</Pill>
        {row.action && <span className="text-[11px] text-osint-muted">{row.action}</span>}
        {row.gate && <Pill>{row.gate}</Pill>}
        <span className="ml-auto text-[10px] font-mono text-osint-muted">{shortTime(row.created_at)}</span>
      </div>
      <button type="button" onClick={() => onOpenSource?.(row.source_id)} className="font-mono text-xs font-semibold text-osint-text hover:text-accent text-left break-all">{row.source_id}</button>
      <div className="text-[10px] text-osint-muted font-mono">
        #{row.id}{row.anomaly_id != null ? ` · anomaly ${row.anomaly_id}` : ''}{row.triage_class ? ` · class: ${row.triage_class}` : ''}{row.model ? ` · ${row.model}` : ''}
      </div>
      {swap && <UrlDiff oldUrl={swap.old_url} newUrl={swap.new_url} />}
      {swap && swap.runtime_override === false && (
        <div className="text-[11px] text-accent">old_url isn't matchable by a runtime override — applying records the swap but may not rewrite live traffic.</div>
      )}
      {row.pr_url && <a href={row.pr_url} target="_blank" rel="noreferrer" className="text-xs text-accent hover:underline">Open PR ↗</a>}
      {(approvable && (onApprove || onReject)) || (row.status === 'merged' && onRevert) ? (
        <div className="flex items-center gap-2 pt-1">
          {approvable && onApprove && <Button size="sm" variant="primary" disabled={busy} onClick={() => setConfirm(true)}>Approve</Button>}
          {approvable && onReject && <Button size="sm" variant="danger" disabled={busy} onClick={() => onReject(row)}>Reject</Button>}
          {row.status === 'merged' && onRevert && <Button size="sm" variant="danger" disabled={busy} onClick={() => onRevert(row)}>Revert override</Button>}
          {busy && <Spinner size={12} />}
        </div>
      ) : null}
      <ConfirmDialog
        open={confirm}
        onClose={() => setConfirm(false)}
        onConfirm={() => { setConfirm(false); onApprove(row); }}
        title="Apply this fix?"
        confirmLabel="Approve & apply"
        danger={false}
        message={`Writes a live runtime URL override for ${row.source_id} and marks the repair merged.`}
      />
    </Card>
  );
}

function QuarantineCard({ row, busy, onClear, onOpenSource }) {
  return (
    <Card className={cx('space-y-1', row.active && 'border-neon-red/40')}>
      <div className="flex items-center gap-2">
        <button type="button" onClick={() => onOpenSource?.(row.source_id)} className="font-mono text-xs font-semibold text-osint-text hover:text-accent truncate text-left">{row.name || row.source_id}</button>
        <span className="ml-auto"><Pill tone={row.active ? 'danger' : 'neutral'}>{row.active ? 'ACTIVE' : 'EXPIRED'}</Pill></span>
      </div>
      {row.name && <div className="text-[10px] font-mono text-osint-muted">{row.source_id}{row.category ? ` · ${row.category}` : ''}</div>}
      {row.reason && <div className="text-xs text-osint-muted">{row.reason}</div>}
      <div className="flex items-center gap-3 text-[10px] font-mono text-osint-muted pt-1">
        {row.since && <span>since {shortTime(row.since)}</span>}
        {row.until && <span>until {shortTime(row.until)}</span>}
        <span className="ml-auto flex items-center gap-1.5">
          <Button size="sm" variant="ghost" onClick={() => onOpenSource?.(row.source_id)}>Pipeline</Button>
          <Button size="sm" busy={busy} onClick={() => onClear(row.source_id)}><LuLockOpen size={12} /> Clear</Button>
        </span>
      </div>
    </Card>
  );
}

function MaintenanceTab({ onOpenSource }) {
  const [hours, setHours] = useState(24);
  const { data: d, error, loading, reload } = useApi(`/api/admin/maintenance?hours=${hours}`, { deps: [hours] });
  const [busy, setBusy] = useState(new Set());
  const [actError, setActError] = useState(null);
  const [showOverrides, setShowOverrides] = useState(false);

  const act = async (key, fn, okMsg) => {
    setBusy((s) => new Set(s).add(key)); setActError(null);
    try { const r = await fn(); toast(okMsg + (r?.new_url ? ` → ${r.new_url}` : ''), { tone: 'success' }); await reload({ silent: true }); }
    catch (e) { setActError(e); }
    finally { setBusy((s) => { const n = new Set(s); n.delete(key); return n; }); }
  };
  const approve = (r) => act(`r${r.id}`, () => api.post(`/api/admin/repairs/${r.id}/approve`), `Repair #${r.id} applied`);
  const reject = (r) => act(`r${r.id}`, () => api.post(`/api/admin/repairs/${r.id}/reject`), `Repair #${r.id} rejected`);
  const revert = (r) => act(`r${r.id}`, () => api.del(`/api/admin/repairs/${r.id}`), `Repair #${r.id} reverted`);
  const clearQ = (id) => act(`q${id}`, () => api.post(`/api/admin/sources/${encodeURIComponent(id)}/unquarantine`), `Quarantine cleared for ${id}`);

  const awaiting = useMemo(() => [...(d?.awaiting_review?.awaiting_apply || []), ...(d?.awaiting_review?.awaiting_pr || [])], [d]);
  const t = d?.totals || {};

  return (
    <div className="space-y-4">
      <div className="flex items-center justify-between gap-2 flex-wrap">
        <Segmented value={hours} onChange={setHours} options={WINDOWS} />
        <div className="flex items-center gap-2 text-[10px] font-mono text-osint-muted">
          {d?.generated_at && <span>generated {shortTime(d.generated_at)}</span>}
          <Button size="sm" variant="ghost" onClick={() => reload()}><LuRefreshCw size={12} /></Button>
        </div>
      </div>
      {loading && !d && <LoadingState label="Loading the maintenance digest…" />}
      {error && <ErrorNotice error={error} title={opError(error) || 'Could not load the digest'} onRetry={reload} />}
      {actError && <ErrorNotice error={actError} title={opError(actError) || 'Action failed'} />}

      {d && (
        <>
          <Section label={`Last ${d.window_hours ?? hours}h`}>
            <div className="grid grid-cols-3 md:grid-cols-6 gap-2">
              <StatCard label="Auto-fixed" value={t.merged} tone="success" />
              <StatCard label="Awaiting" value={awaiting.length} tone="warning" />
              <StatCard label="Needs human" value={t.needs_human} tone="cyan" />
              <StatCard label="Verified" value={t.verified} tone="warning" />
              <StatCard label="Rejected" value={t.rejected} tone="neutral" />
              <StatCard label="Errors" value={t.error} tone="danger" />
            </div>
          </Section>

          <Section label={`Awaiting your review · ${awaiting.length}`} padded={false}>
            {awaiting.length === 0 ? <EmptyState title="No staged fixes waiting." /> : (
              <div className="p-2 grid gap-2 md:grid-cols-2">
                {awaiting.map((r) => <RepairCard key={r.id} row={r} busy={busy.has(`r${r.id}`)} onApprove={approve} onReject={reject} onOpenSource={onOpenSource} />)}
              </div>
            )}
          </Section>

          {(d.needs_human || []).length > 0 && (
            <Section label={`Needs human · ${d.needs_human.length}`} padded={false}>
              <div className="p-2 grid gap-2 md:grid-cols-2">
                {d.needs_human.map((r) => <RepairCard key={r.id} row={r} onOpenSource={onOpenSource} />)}
              </div>
            </Section>
          )}

          {(d.quarantined || []).length > 0 && (
            <Section label={`Quarantined · ${d.quarantined.length}`} padded={false}>
              <div className="p-2 grid gap-2 md:grid-cols-2">
                {d.quarantined.map((q) => <QuarantineCard key={q.source_id} row={q} busy={busy.has(`q${q.source_id}`)} onClear={clearQ} onOpenSource={onOpenSource} />)}
              </div>
            </Section>
          )}

          {(d.auto_fixed || []).length > 0 && (
            <Section label={`Recently auto-fixed · ${d.auto_fixed.length}`} padded={false}>
              <div className="p-2 grid gap-2 md:grid-cols-2">
                {d.auto_fixed.map((r) => <RepairCard key={r.id} row={r} busy={busy.has(`r${r.id}`)} onRevert={revert} onOpenSource={onOpenSource} />)}
              </div>
              {(d.url_overrides || []).length > 0 && (
                <div className="px-3 pb-3">
                  <button type="button" className="text-xs text-accent hover:underline" onClick={() => setShowOverrides((v) => !v)}>
                    Active URL overrides ({d.url_overrides.length}) {showOverrides ? '▴' : '▾'}
                  </button>
                  {showOverrides && (
                    <div className="space-y-2 mt-2">
                      {d.url_overrides.map((o, i) => (
                        <div key={`${o.source_id}${o.created_at || i}`}>
                          <div className="font-mono text-[11px] font-semibold text-osint-text">{o.source_id} <span className="text-osint-muted font-normal">{shortTime(o.created_at)}{o.anomaly_id != null ? ` · anomaly ${o.anomaly_id}` : ''}</span></div>
                          <UrlDiff oldUrl={o.old_url} newUrl={o.new_url} />
                        </div>
                      ))}
                    </div>
                  )}
                </div>
              )}
            </Section>
          )}

          {(d.auto_dismissed || []).length > 0 && (
            <Section label={`Auto-dismissed · ${d.auto_dismissed.length}`} padded={false}>
              <div className="p-2 grid gap-2 md:grid-cols-2">
                {d.auto_dismissed.map((r) => <RepairCard key={r.id} row={r} onOpenSource={onOpenSource} />)}
              </div>
            </Section>
          )}

          <div className="grid gap-4 md:grid-cols-2">
            {(d.success_by_class || []).length > 0 && (
              <Section label="Success by triage class">
                {d.success_by_class.map((c) => (
                  <Row key={c.class} label={<span className="font-mono">{c.class}</span>}>
                    {c.success_rate != null && <span className={c.success_rate >= 0.5 ? 'text-neon-green' : 'text-accent'}>{Math.round(c.success_rate * 100)}%</span>}
                    <span>{c.success ?? 0}✓ / {c.fail ?? 0}✗{c.needs_human ? ` / ${c.needs_human}?` : ''}</span>
                  </Row>
                ))}
              </Section>
            )}
            {(d.worst_sources || []).length > 0 && (
              <Section label="Worst sources">
                {d.worst_sources.map((s) => (
                  <Row key={s.source_id} label={<span className="font-mono text-xs">{s.source_id}</span>} hint={`${s.success ?? 0} fixed · ${s.fail ?? 0} failed`} onClick={() => onOpenSource(s.source_id)}>
                    {s.success_rate != null && <span>{Math.round(s.success_rate * 100)}%</span>}<span>›</span>
                  </Row>
                ))}
              </Section>
            )}
          </div>

          <div className="text-[11px] text-osint-muted">
            Auto-repair runs only while the worker (LLM) is enabled on the host. Approving a fix writes a live runtime URL override; rejecting resolves the anomaly so the worker stops re-deriving it.
            {d.concurrency?.heavy?.limit != null && d.concurrency?.mid?.limit != null && (
              <> Configured LLM concurrency — heavy {d.concurrency.heavy.limit}, mid {d.concurrency.mid.limit}
                {d.concurrency.heavy.inflight != null ? ` (in flight ${d.concurrency.heavy.inflight}/${d.concurrency.mid.inflight ?? 0}, waiting ${d.concurrency.heavy.waiting ?? 0}/${d.concurrency.mid.waiting ?? 0})` : ' (the C runtime reports no live in-flight gauge)'}.
              </>
            )}
          </div>
        </>
      )}

      <Section label="Open a source pipeline">
        <SourcePicker onPick={onOpenSource} />
      </Section>
    </div>
  );
}

function SourcePicker({ onPick }) {
  const [id, setId] = useState('');
  return (
    <form className="flex gap-2" onSubmit={(e) => { e.preventDefault(); if (id.trim()) onPick(id.trim()); }}>
      <Input mono placeholder="source id (e.g. jma-earthquake)" value={id} onChange={(e) => setId(e.target.value)} />
      <Button type="submit" variant="primary" disabled={!id.trim()}>Open</Button>
    </form>
  );
}

/* --------------------------------------------------------- source pipeline */

function AnomalyCard({ a, busy, onRequeue }) {
  const open = !a.resolved_at;
  return (
    <Card className="space-y-1">
      <div className="flex items-center gap-2 flex-wrap">
        {a.verdict && <Pill tone="danger">{String(a.verdict).toUpperCase()}</Pill>}
        <Pill tone={open ? 'warning' : 'neutral'}>{open ? 'OPEN' : 'RESOLVED'}</Pill>
        {a.triage_class && <Pill tone="cyan">{a.triage_class}{a.triage_confidence != null ? ` ${Math.round(a.triage_confidence * 100)}%` : ''}</Pill>}
        {a.escalation_level != null && <Pill>esc {a.escalation_level}</Pill>}
        <span className="ml-auto text-[10px] font-mono text-osint-muted">#{a.id} · {shortTime(a.created_at)}</span>
      </div>
      {a.reason && <div className="text-xs text-osint-text">{a.reason}</div>}
      {a.evidence && <pre className="text-[10px] font-mono text-osint-muted whitespace-pre-wrap break-all max-h-24 overflow-auto bg-osint-bg rounded p-1.5">{a.evidence}</pre>}
      {a.triage_suggested_fix && <div className="text-[11px]"><span className="text-osint-muted">suggested fix: </span><span className="text-osint-text">{a.triage_suggested_fix}</span></div>}
      {a.triage_evidence && <div className="text-[11px] text-osint-muted">{a.triage_evidence}</div>}
      <div className="flex items-center gap-2 text-[10px] font-mono text-osint-muted">
        {a.triaged_at && <span>triaged {shortTime(a.triaged_at)}{a.triage_model ? ` by ${a.triage_model}` : ''}</span>}
        {a.resolved_at && <span>resolved {shortTime(a.resolved_at)}{a.resolution ? ` (${a.resolution})` : ''}</span>}
        {open && <span className="ml-auto"><Button size="sm" busy={busy} onClick={() => onRequeue(a)}>Re-queue triage</Button></span>}
      </div>
    </Card>
  );
}

function SourcePipelineSheet({ sourceId, onClose }) {
  const { data: p, error, loading, reload } = useApi(sourceId ? `/api/admin/maintenance/source/${encodeURIComponent(sourceId)}` : null, { deps: [sourceId] });
  const src = useApi(sourceId ? `/api/sources/${encodeURIComponent(sourceId)}` : null, { deps: [sourceId] });
  const [busy, setBusy] = useState(new Set());
  const [actError, setActError] = useState(null);
  const [confirmCapture, setConfirmCapture] = useState(false);

  const act = async (key, fn, okMsg) => {
    setBusy((s) => new Set(s).add(key)); setActError(null);
    try { const r = await fn(); toast(okMsg + (r?.new_url ? ` → ${r.new_url}` : ''), { tone: 'success' }); await reload({ silent: true }); src.reload({ silent: true }); }
    catch (e) { setActError(e); }
    finally { setBusy((s) => { const n = new Set(s); n.delete(key); return n; }); }
  };
  if (!sourceId) return null;
  const s = p?.source;
  const runs = p?.fetch_log || [];
  const captureOn = Boolean(src.data?.capture_evidence);

  return (
    <Sheet open onClose={onClose} title={sourceId} width="max-w-3xl" footer={<Button onClick={onClose}>Close</Button>}>
      <div className="space-y-4">
        <div className="flex flex-wrap gap-2">
          <Button size="sm" busy={busy.has('run')} onClick={() => act('run', () => api.post(`/api/intel/sources/${encodeURIComponent(sourceId)}/run`), 'Collector run finished')}><LuPlay size={12} /> Re-run collector</Button>
          {s?.quarantine?.active && <Button size="sm" busy={busy.has('q')} onClick={() => act('q', () => api.post(`/api/admin/sources/${encodeURIComponent(sourceId)}/unquarantine`), 'Quarantine cleared')}><LuLockOpen size={12} /> Clear quarantine</Button>}
          <Button size="sm" variant="ghost" onClick={() => { reload(); src.reload(); }}><LuRefreshCw size={12} /> Refresh</Button>
        </div>
        {loading && !p && <LoadingState label="Loading pipeline…" />}
        {error && <ErrorNotice error={error} title={opError(error) || 'Could not load the pipeline'} onRetry={reload} />}
        {actError && <ErrorNotice error={actError} title={opError(actError) || 'Action failed'} />}

        {s && (
          <Card className="space-y-2">
            <div className="flex items-center gap-2 flex-wrap">
              <Pill tone={{ online: 'success', degraded: 'warning', offline: 'danger' }[s.status] || 'neutral'}>{String(s.status || 'unknown').toUpperCase()}</Pill>
              {s.category && <span className="text-[11px] text-osint-muted">{s.category}</span>}
              {s.type && <span className="ml-auto text-[11px] text-osint-muted">{s.type}</span>}
            </div>
            {s.name && <div className="font-mono text-sm font-semibold text-osint-text">{s.name}</div>}
            {s.quarantine?.active && <div className="text-xs text-neon-red">Quarantined{s.quarantine.reason ? ` — ${s.quarantine.reason}` : ''}{s.quarantine.until ? ` · until ${shortTime(s.quarantine.until)}` : ''}</div>}
            <div className="grid grid-cols-3 gap-2 text-center">
              {[['records', s.records_count], ['latency', s.response_time_ms != null ? `${s.response_time_ms}ms` : null], ["ok'd", s.last_success ? shortTime(s.last_success) : null]].map(([l, v]) => (
                <div key={l}><div className="font-mono text-xs text-osint-text">{v ?? '—'}</div><div className="text-[10px] text-osint-muted">{l}</div></div>
              ))}
            </div>
            {s.error_message && <div className="font-mono text-[11px] text-neon-red break-all">{s.error_message}</div>}
            {s.url && <div className="font-mono text-[10px] text-osint-muted break-all">{s.url}</div>}
          </Card>
        )}

        <Section label="Evidence capture">
          {src.error ? <ErrorNotice error={src.error} title="Could not read the source row" onRetry={src.reload} /> : (
            <Row label="Capture evidence for this source" hint="Writes third-party content fetched by this source to the server's disk (audited). POST turns it on, DELETE turns it off.">
              {src.loading && !src.data ? <Spinner size={12} /> : (
                <Toggle on={captureOn} disabled={busy.has('cap')} onChange={(on) => { if (on) setConfirmCapture(true); else act('cap', () => api.del(`/api/admin/sources/${encodeURIComponent(sourceId)}/capture-evidence`), 'Evidence capture off'); }} />
              )}
            </Row>
          )}
        </Section>

        {p && (
          <>
            <Section label={`Fetch runs · ${runs.length}`}>
              {runs.length === 0 ? <div className="text-xs text-osint-muted">No recorded runs yet.</div> : (
                <>
                  <div className="flex gap-0.5 mb-2">{[...runs].reverse().map((r, i) => <span key={r.id ?? i} className={cx('w-2 h-4 rounded-sm', (r.status || '').toLowerCase() === 'ok' ? 'bg-neon-green' : 'bg-neon-red')} />)}</div>
                  <div className="max-h-56 overflow-auto">
                    {runs.map((r, i) => (
                      <div key={r.id ?? i} className="flex items-center gap-2 py-1 border-b border-osint-border last:border-0 text-[11px]">
                        <span className={cx('w-1.5 h-1.5 rounded-full', (r.status || '').toLowerCase() === 'ok' ? 'bg-neon-green' : 'bg-neon-red')} />
                        <span className="font-mono text-osint-muted">{shortTime(r.timestamp)}</span>
                        <span className="font-mono text-osint-muted">{r.status}</span>
                        {r.error && <span className="text-neon-red truncate flex-1" title={r.error}>{r.error}</span>}
                        <span className="ml-auto font-mono text-osint-muted">{r.records_fetched ?? 0} rec{r.duration_ms != null ? ` · ${r.duration_ms}ms` : ''}</span>
                      </div>
                    ))}
                  </div>
                </>
              )}
            </Section>
            <Section label={`Anomalies & triage · ${(p.anomalies || []).length}`} padded={false}>
              {(p.anomalies || []).length === 0 ? <EmptyState title="No anomalies — pipeline healthy." /> : (
                <div className="p-2 space-y-2">
                  {p.anomalies.map((a) => <AnomalyCard key={a.id} a={a} busy={busy.has(`a${a.id}`)} onRequeue={(x) => act(`a${x.id}`, () => api.post(`/api/admin/anomalies/${x.id}/requeue`), `Anomaly #${x.id} re-queued`)} />)}
                </div>
              )}
            </Section>
            {(p.repairs || []).length > 0 && (
              <Section label={`Repair propositions · ${p.repairs.length}`} padded={false}>
                <div className="p-2 space-y-2">
                  {p.repairs.map((r) => (
                    <RepairCard
                      key={r.id} row={r} busy={busy.has(`r${r.id}`)}
                      onApprove={(x) => act(`r${x.id}`, () => api.post(`/api/admin/repairs/${x.id}/approve`), `Repair #${x.id} applied`)}
                      onReject={(x) => act(`r${x.id}`, () => api.post(`/api/admin/repairs/${x.id}/reject`), `Repair #${x.id} rejected`)}
                      onRevert={(x) => act(`r${x.id}`, () => api.del(`/api/admin/repairs/${x.id}`), `Repair #${x.id} reverted`)}
                    />
                  ))}
                </div>
              </Section>
            )}
          </>
        )}
      </div>
      <ConfirmDialog
        open={confirmCapture}
        onClose={() => setConfirmCapture(false)}
        onConfirm={() => { setConfirmCapture(false); act('cap', () => api.post(`/api/admin/sources/${encodeURIComponent(sourceId)}/capture-evidence`), 'Evidence capture on'); }}
        title="Start capturing evidence?"
        confirmLabel="Turn on"
        message="Every fetch by this source will start writing third-party content to the server's disk. The action is audited."
      />
    </Sheet>
  );
}

/* ================================================================= breach */

function jobTone(state) { return { done: 'success', error: 'danger', running: 'accent', started: 'accent' }[state] || 'neutral'; }
function isRunning(j) { return ['running', 'started', 'queued'].includes(j.state); }
function stagedPath(j) {
  if (j.kind !== 'fetch' || !j.message) return null;
  const i = j.message.indexOf(' -> ');
  if (i < 0) return null;
  const tail = j.message.slice(i + 4);
  const p = (tail.split(' (status')[0] || tail).trim();
  return p || null;
}
function elapsed(j) {
  if (!j.started) return '—';
  const end = j.ended ? j.ended * 1000 : Date.now();
  const secs = Math.max(0, Math.floor((end - j.started * 1000) / 1000));
  return secs < 60 ? `${secs}s` : `${Math.floor(secs / 60)}m ${secs % 60}s`;
}

function StepCard({ n, title, hint, children }) {
  return (
    <Card className="space-y-2">
      <div className="flex items-center gap-2">
        <span className="w-5 h-5 rounded-full bg-accent/10 text-accent font-mono text-[11px] font-semibold flex items-center justify-center">{n}</span>
        <span className="font-mono text-[10px] font-semibold uppercase tracking-[0.12em] text-osint-muted">{title}</span>
      </div>
      <div className="text-[11px] text-osint-muted">{hint}</div>
      {children}
    </Card>
  );
}

function BreachTab() {
  const status = useApi('/api/status');
  const jobsQ = useApi('/api/admin/breach/jobs');
  const jobs = jobsQ.data?.jobs || [];
  const busyAny = jobs.some(isRunning);
  const timer = useRef(null);
  useEffect(() => {
    clearInterval(timer.current);
    timer.current = setInterval(() => jobsQ.reload({ silent: true }), busyAny ? 2000 : 15000);
    return () => clearInterval(timer.current);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [busyAny]);

  const [notice, setNotice] = useState(null);
  const [err, setErr] = useState(null);
  const [runningCollector, setRunningCollector] = useState(false);
  const [seedPath, setSeedPath] = useState('');
  const [preview, setPreview] = useState(null);
  const [previewing, setPreviewing] = useState(false);
  const [loadResult, setLoadResult] = useState(null);
  const [confirmLoad, setConfirmLoad] = useState(false);
  const [saving, setSaving] = useState(false);
  const [sourceId, setSourceId] = useState('');
  const [fetchURL, setFetchURL] = useState('');
  const [fetching, setFetching] = useState(false);
  const [ingestPath, setIngestPath] = useState('');
  const [ingestType, setIngestType] = useState('auto');
  const [materialize, setMaterialize] = useState(false);
  const [dryRun, setDryRun] = useState(true);
  const [ingesting, setIngesting] = useState(false);
  const [confirmIngest, setConfirmIngest] = useState(false);

  const summary = status.data?.summary;
  const breachSources = useMemo(() => (status.data?.apis || []).filter((s) => s.category === 'breach'), [status.data]);
  const suggestions = useMemo(() => {
    const q = sourceId.toLowerCase();
    const pool = q ? breachSources.filter((s) => s.id.toLowerCase().includes(q) || (s.name || '').toLowerCase().includes(q)) : breachSources;
    return pool.slice(0, 25);
  }, [breachSources, sourceId]);
  const fetchBusy = jobs.some((j) => j.kind === 'fetch' && isRunning(j));
  const ingestBusy = jobs.some((j) => j.kind === 'ingest' && isRunning(j));

  const wrap = async (setBusy, fn) => {
    setBusy(true); setErr(null); setNotice(null);
    try { await fn(); } catch (e) { setErr(e); }
    finally { setBusy(false); }
  };
  const runCollector = () => wrap(setRunningCollector, async () => {
    await api.post('/api/intel/sources/breach-corpus/run');
    await status.reload({ silent: true });
    setNotice('Collector run complete — the catalog count above is refreshed.');
  });
  const runPreview = () => wrap(setPreviewing, async () => {
    setPreview(await api.get('/api/admin/breach/catalog/preview', { query: { path: seedPath.trim() || undefined, limit: 50 } }));
  });
  const runLoad = () => wrap(setSaving, async () => {
    const body = { format: 'auto' };
    if (seedPath.trim()) body.path = seedPath.trim();
    const r = await api.post('/api/admin/breach/catalog/load', body);
    setLoadResult(r);
    await status.reload({ silent: true });
    setNotice(`Loaded ${r?.loaded ?? '?'} catalog entries.`);
  });
  const runFetch = () => wrap(setFetching, async () => {
    await api.post('/api/admin/breach/fetch', { source_id: sourceId.trim(), url: fetchURL.trim() });
    await jobsQ.reload({ silent: true });
    setNotice('Fetch started.');
  });
  const runIngest = () => wrap(setIngesting, async () => {
    const body = { source_id: sourceId.trim(), path: ingestPath.trim(), materialize, dry_run: dryRun };
    if (ingestType !== 'auto') body.type = ingestType;
    await api.post('/api/admin/breach/ingest', body);
    await jobsQ.reload({ silent: true });
    setNotice(dryRun ? 'Dry-run ingest started — nothing will be written.' : 'Ingest started.');
  });

  return (
    <div className="space-y-4">
      {err && <ErrorNotice error={err} title={opError(err) || 'Breach console request failed'} />}
      {notice && <div className="text-xs text-neon-green border border-neon-green/40 bg-neon-green/10 rounded px-3 py-2">{notice}</div>}
      {status.error && <ErrorNotice error={status.error} title="Source status unavailable (catalog counts and breach sources unknown)" onRetry={status.reload} />}

      <div className="grid gap-4 md:grid-cols-2">
        <StepCard n={1} title="Catalog" hint="Breaches known to the server, from the local seed file.">
          <div className="flex gap-6">
            <div><div className="font-mono text-xl font-bold text-osint-text">{summary ? (summary.breachTotal ?? 0).toLocaleString() : '—'}</div><div className="text-[10px] text-osint-muted">catalogued</div></div>
            <div><div className="font-mono text-xl font-bold text-neon-red">{summary ? (summary.breachMaterialized ?? 0).toLocaleString() : '—'}</div><div className="text-[10px] text-osint-muted">with records</div></div>
          </div>
          <Button busy={runningCollector} onClick={runCollector}><LuPlay size={12} /> Run collector</Button>
          <div className="text-[11px] text-osint-muted">Runs the breach-corpus collector, which loads the seed straight into the catalog — the same thing step 3 does, on the server's daily schedule.</div>
        </StepCard>

        <StepCard n={2} title="Parse" hint="Reads and normalizes the seed. Writes nothing.">
          <Input mono placeholder="Seed path (blank = server default)" value={seedPath} onChange={(e) => setSeedPath(e.target.value)} />
          <Button variant="primary" busy={previewing} onClick={runPreview}>Preview parse</Button>
          {preview && (
            <div className="space-y-2">
              <div className="flex flex-wrap gap-1">
                <Pill>{preview.rows_read} read</Pill><Pill tone="accent">{preview.rows_parsed} parsed</Pill><Pill tone="warning">{preview.rows_skipped} skipped</Pill>
                <Pill tone="success">{preview.rows_new} new</Pill><Pill>{preview.rows_existing} existing</Pill><Pill>{preview.format}</Pill>
              </div>
              <div className="font-mono text-[10px] text-osint-muted break-all">{preview.path}</div>
              <div className="overflow-x-auto rounded-md border border-osint-border bg-osint-bg">
                <table className="w-full text-[11px]">
                  <thead><tr className="text-left font-mono text-[9px] uppercase tracking-wider text-osint-muted"><th className="px-2 py-1">ID</th><th className="px-2 py-1 text-right">Accounts</th><th className="px-2 py-1 text-right">Breached</th></tr></thead>
                  <tbody>
                    {(preview.sample || []).map((r) => (
                      <tr key={r.breach_id} className="border-t border-osint-border">
                        <td className="px-2 py-1"><div className="font-mono text-osint-text truncate max-w-[220px]">{r.breach_id}</div><div className="text-[10px] text-osint-muted truncate max-w-[220px]">{r.name}</div></td>
                        <td className="px-2 py-1 text-right font-mono text-osint-muted">{r.pwn_count != null ? r.pwn_count.toLocaleString() : '—'}</td>
                        <td className="px-2 py-1 text-right font-mono text-osint-muted">{r.breach_date || '—'}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
              <BoundNote shown={(preview.sample || []).length} total={preview.rows_parsed} noun="parsed rows (sample)" />
              {preview.rows_skipped > 0 && <div className="text-[11px] text-osint-muted">Skipped rows are the header plus names that normalize to an empty id (non-Latin catalog names have no usable slug).</div>}
            </div>
          )}
        </StepCard>

        <StepCard n={3} title="Save" hint="Commits the parsed catalog into breach_meta.">
          <Button variant="primary" busy={saving} disabled={!preview} onClick={() => setConfirmLoad(true)}>Load into catalog</Button>
          {!preview && <div className="text-[11px] text-osint-muted">Run a preview first so you can see what will be written.</div>}
          {loadResult && (
            <div className="flex flex-wrap gap-1">
              <Pill tone="success">{loadResult.loaded} loaded</Pill>
              {loadResult.rows_new != null && <Pill tone="accent">{loadResult.rows_new} new</Pill>}
              {loadResult.rows_existing != null && <Pill>{loadResult.rows_existing} existing</Pill>}
              {loadResult.format && <Pill>{loadResult.format}</Pill>}
            </div>
          )}
        </StepCard>

        <StepCard n={4} title="Ingest records" hint="Stage a leaked-record dataset, then index it. Catalog entries alone carry no records.">
          <Field label="Source (breach id)">
            <Input mono list="breach-source-ids" placeholder="breach id" value={sourceId} onChange={(e) => setSourceId(e.target.value)} />
            <datalist id="breach-source-ids">{suggestions.map((s) => <option key={s.id} value={s.id}>{s.name || s.id}</option>)}</datalist>
          </Field>
          <Field label="Stage from URL">
            <div className="flex gap-2">
              <Input mono placeholder="https://…" value={fetchURL} onChange={(e) => setFetchURL(e.target.value)} />
              <Button busy={fetching} disabled={fetchBusy || !fetchURL.trim() || !sourceId.trim()} onClick={runFetch}>Stage</Button>
            </div>
          </Field>
          {fetchBusy && <div className="text-[11px] text-accent">A fetch is already running.</div>}
          <Field label="File (path on the server)" hint="Filled in from a completed fetch when you click “Use file”. The server truncates that message at 120 characters, so check the path before ingesting.">
            <Input mono placeholder="/path/on/server" value={ingestPath} onChange={(e) => setIngestPath(e.target.value)} />
          </Field>
          <Segmented value={ingestType} onChange={setIngestType} options={['auto', 'email', 'username', 'phone', 'password'].map((t) => ({ value: t, label: t }))} />
          <Toggle label="Dry run" on={dryRun} onChange={setDryRun} />
          <Toggle label="Materialize records" on={materialize} onChange={setMaterialize} />
          <div className="text-[11px] text-osint-muted">Materializing writes one searchable row per identity — roughly 4 rows per record, and tens of gigabytes on a 100M-record corpus. Leave it off to build only the hashed index.</div>
          <Button variant="primary" busy={ingesting} disabled={ingestBusy || !sourceId.trim() || !ingestPath.trim()} onClick={() => { if (materialize && !dryRun) setConfirmIngest(true); else runIngest(); }}>Start ingest</Button>
          {ingestBusy && <div className="text-[11px] text-accent">An ingest is already running.</div>}
        </StepCard>
      </div>

      <Section label={`Jobs · ${jobs.length}`} right={<span className="text-[10px] font-mono text-osint-muted">in-memory on the server — resets on restart · polling {busyAny ? '2s' : '15s'}</span>} padded={false}>
        {jobsQ.error && <div className="p-2"><ErrorNotice error={jobsQ.error} title={opError(jobsQ.error) || 'Could not list jobs'} onRetry={jobsQ.reload} /></div>}
        {!jobsQ.error && jobs.length === 0 && <EmptyState title="No jobs yet." />}
        {jobs.length > 0 && (
          <ul className="divide-y divide-osint-border">
            {jobs.map((j) => {
              const p = stagedPath(j);
              return (
                <li key={j.job_id} className="px-3 py-2 space-y-1">
                  <div className="flex items-center gap-2 flex-wrap">
                    <Pill tone={j.kind === 'ingest' ? 'accent' : 'cyan'}>{j.kind}</Pill>
                    <Pill tone={jobTone(j.state)}>{isRunning(j) && <Spinner size={9} />}{j.state}</Pill>
                    <span className="font-mono text-[11px] text-osint-text truncate">{j.source_id || '—'}</span>
                    <span className="ml-auto font-mono text-[10px] text-osint-muted">{elapsed(j)} · {String(j.job_id).slice(0, 8)}</span>
                  </div>
                  <div className="flex items-center gap-3 text-[10px] font-mono">
                    {j.rows_in != null && <span className="text-osint-muted">in {Number(j.rows_in).toLocaleString()}</span>}
                    {j.rows_new != null && <span className="text-neon-green">new {Number(j.rows_new).toLocaleString()}</span>}
                    {p && <button type="button" className="text-accent hover:underline" onClick={() => setIngestPath(p)}>Use file</button>}
                  </div>
                  {j.message && <div className="text-[11px] text-osint-muted break-all">{j.message}</div>}
                </li>
              );
            })}
          </ul>
        )}
      </Section>

      <ConfirmDialog open={confirmLoad} onClose={() => setConfirmLoad(false)} onConfirm={() => { setConfirmLoad(false); runLoad(); }} title="Load the catalog?" confirmLabel="Load" danger={false}
        message={preview ? `${preview.rows_new} new and ${preview.rows_existing} existing entries will be written. Existing rows are updated in place.` : ''} />
      <ConfirmDialog open={confirmIngest} onClose={() => setConfirmIngest(false)} onConfirm={() => { setConfirmIngest(false); runIngest(); }} title="Materialize records?" confirmLabel="Ingest and materialize"
        message="This writes searchable rows for every identity in the dataset and can consume a large amount of disk. Run a dry run first if you haven't." />
    </div>
  );
}

/* ============================================================= pagination */

function PaginationTab() {
  const jobsQ = useApi('/api/admin/pagination/jobs');
  const rows = jobsQ.data?.data || [];
  const running = rows.some((j) => j.state === 'running');
  const timer = useRef(null);
  useEffect(() => {
    clearInterval(timer.current);
    timer.current = setInterval(() => jobsQ.reload({ silent: true }), running ? 3000 : 20000);
    return () => clearInterval(timer.current);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [running]);
  const [limit, setLimit] = useState('');
  const [busy, setBusy] = useState(false);
  const [err, setErr] = useState(null);
  const [confirm, setConfirm] = useState(false);

  const start = async () => {
    setBusy(true); setErr(null);
    try {
      const r = await api.post(`/api/admin/pagination/probe${limit ? `?limit=${encodeURIComponent(limit)}` : ''}`);
      toast(`Probe ${r?.job_id ? String(r.job_id).slice(0, 8) : ''} started over ${r?.candidates ?? '?'} candidates`, { tone: 'success' });
      if (r?.note) toast(r.note);
      await jobsQ.reload({ silent: true });
    } catch (e) { setErr(e); }
    finally { setBusy(false); }
  };

  return (
    <div className="space-y-4">
      <Section label="Pagination survey">
        <div className="text-[11px] text-osint-muted">Thousands of registered sources declare a page size and no offset, so the page walker fetches page 1 and stops. This job measures page 1 against page 2 for each candidate instead of guessing; sources proven to paginate become url_swap repairs for the Maintenance approval flow — it never changes a URL by itself. Minutes of work and thousands of requests.</div>
        <div className="flex flex-wrap items-end gap-2 mt-2">
          <Field label="Limit (blank = all candidates)"><Input mono className="w-40" placeholder="e.g. 200" value={limit} onChange={(e) => setLimit(e.target.value.replace(/[^0-9]/g, ''))} /></Field>
          <Button variant="primary" busy={busy} disabled={running} onClick={() => setConfirm(true)}>Start probe</Button>
          {jobsQ.data?.candidates_now != null && <span className="text-[11px] font-mono text-osint-muted">{jobsQ.data.candidates_now} candidates now</span>}
          {running && <span className="text-[11px] text-accent">A probe is already running.</span>}
        </div>
        {err && <ErrorNotice error={err} title={opError(err) || 'Could not start the probe'} className="mt-2" />}
      </Section>
      <Section label={`Jobs · ${rows.length}`} right={<Button size="sm" variant="ghost" onClick={() => jobsQ.reload()}><LuRefreshCw size={12} /></Button>} padded={false}>
        {jobsQ.error && <div className="p-2"><ErrorNotice error={jobsQ.error} title={opError(jobsQ.error) || 'Could not list jobs'} onRetry={jobsQ.reload} /></div>}
        {!jobsQ.error && rows.length === 0 && <EmptyState title="No probe jobs (in-memory on the server; the list resets on restart)." />}
        {rows.length > 0 && (
          <ul className="divide-y divide-osint-border">
            {rows.map((j) => (
              <li key={j.job_id} className="px-3 py-2 space-y-1">
                <div className="flex items-center gap-2 flex-wrap">
                  <Pill tone={jobTone(j.state)}>{j.state === 'running' && <Spinner size={9} />}{j.state}</Pill>
                  <span className="font-mono text-[10px] text-osint-muted">{String(j.job_id).slice(0, 8)}</span>
                  <span className="ml-auto font-mono text-[10px] text-osint-muted">{elapsed({ started: j.started_at, ended: j.ended_at })}</span>
                </div>
                <div className="flex flex-wrap gap-x-3 text-[10px] font-mono">
                  <span className="text-osint-text">{j.probed ?? 0} / {j.candidates ?? 0} probed</span>
                  <span className="text-neon-green">{j.paginates ?? 0} paginate</span>
                  <span className="text-osint-muted">{j.ignores_offset ?? 0} ignore offset</span>
                  <span className="text-osint-muted">{j.already_complete ?? 0} already complete</span>
                  <span className="text-neon-red">{j.errors ?? 0} errors</span>
                </div>
                {j.candidates > 0 && <div className="h-1 rounded bg-osint-border overflow-hidden"><div className="h-full bg-accent" style={{ width: `${Math.min(100, Math.round(((j.probed || 0) / j.candidates) * 100))}%` }} /></div>}
                {j.message && <div className="text-[11px] text-osint-muted break-all">{j.message}</div>}
              </li>
            ))}
          </ul>
        )}
      </Section>
      <ConfirmDialog open={confirm} onClose={() => setConfirm(false)} onConfirm={() => { setConfirm(false); start(); }} title="Start the pagination probe?" confirmLabel="Start" danger={false}
        message={`Sends two requests to ${limit ? `up to ${limit}` : 'every'} candidate upstream. Nothing is rewritten; proven sources appear under Maintenance › Awaiting review.`} />
    </div>
  );
}

/* ================================================================= server */

function CapabilityCard({ label, path }) {
  const { data, error, loading, reload } = useApi(path);
  const pairs = data && typeof data === 'object' ? Object.entries(data).map(([k, v]) => [k, typeof v === 'boolean' ? (v ? 'yes' : 'no') : v]) : [];
  return (
    <Section label={label} right={<Button size="sm" variant="ghost" onClick={() => reload()}><LuRefreshCw size={12} /></Button>}>
      {loading && !data && <LoadingState />}
      {error && <ErrorNotice error={error} title={opError(error) || 'Unavailable'} onRetry={reload} />}
      {data && (pairs.length ? <KV pairs={pairs} /> : <pre className="text-[11px] font-mono text-osint-muted">{JSON.stringify(data, null, 2)}</pre>)}
    </Section>
  );
}

function ServerTab() {
  const [confirm, setConfirm] = useState(false);
  const [phase, setPhase] = useState('idle');
  const [result, setResult] = useState(null);

  const restart = async () => {
    setPhase('restarting'); setResult(null);
    let refused = null;
    try { await api.post('/api/admin/restart', {}); }
    catch (e) {
      if (e instanceof ApiError && e.status === 501) { refused = e; }
      else if (e instanceof ApiError && e.status > 0) { setResult({ ok: false, error: e }); setPhase('idle'); return; }
      // a connection reset is the expected shape of a real restart
    }
    if (refused) { setResult({ ok: false, error: refused, notImplemented: true }); setPhase('idle'); return; }
    const deadline = Date.now() + 30000;
    while (Date.now() < deadline) {
      await new Promise((r) => setTimeout(r, 1000));
      try { await api.get('/api/health'); setResult({ ok: true }); setPhase('idle'); return; } catch { /* still down */ }
    }
    setResult({ ok: false, error: new Error("Server didn't come back within 30 s. Check the host.") });
    setPhase('idle');
  };

  return (
    <div className="space-y-4">
      <Section label="Server">
        <Row label="Restart server" hint="Forces every collector to re-read API keys and other env vars. The native C server answers 501 for this route; it is shown here so the refusal is visible rather than hidden.">
          <Button variant="danger" busy={phase === 'restarting'} onClick={() => setConfirm(true)}>Restart</Button>
        </Row>
        {result?.ok && <div className="text-xs text-neon-green pt-2">Server is back — /api/health answered.</div>}
        {result && !result.ok && (
          <ErrorNotice error={result.error} title={result.notImplemented ? 'The native server does not implement restart' : 'Restart failed'} className="mt-2">
            {result.notImplemented && 'Restart the japanosint process on the host (launch.sh) instead.'}
          </ErrorNotice>
        )}
      </Section>
      <div className="grid gap-4 md:grid-cols-3">
        <CapabilityCard label="ffmpeg" path="/api/admin/ffmpeg/capabilities" />
        <CapabilityCard label="Media" path="/api/media/capabilities" />
        <CapabilityCard label="Camera stills" path="/api/camera-stills/capabilities" />
      </div>
      <ConfirmDialog open={confirm} onClose={() => setConfirm(false)} onConfirm={() => { setConfirm(false); restart(); }} title="Restart server?" confirmLabel="Restart"
        message="All in-flight requests will be dropped. The page polls /api/health for up to 30 s and reports whether the server came back." />
    </div>
  );
}

