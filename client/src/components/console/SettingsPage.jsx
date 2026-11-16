import React, { useEffect, useMemo, useState } from 'react';
import { Link } from 'react-router-dom';
import { LuWifi, LuCircleCheck, LuCircleX, LuSun, LuMoon, LuRefreshCw } from 'react-icons/lu';
import { api, errorMessage } from '../../api/client.js';
import { usePagedList } from '../../hooks/usePagedList.js';
import { useKeyedSources } from '../../hooks/useKeyedSources.js';
import { searchPath } from '../../utils/sourceFormat.js';
import { useApi } from '../../hooks/useApi.js';
import { useAuth } from '../../auth/AuthContext.jsx';
import { savedStore } from '../../store/savedStore.js';
import { ServerSettings } from '../auth/OnboardingFlow.jsx';
import { ProbeActions, mergeStatusRow } from './ApiKeysPage.jsx';
import { relativeTime, fmtAbs } from '../../utils/time.js';
import {
  Page, Section, Row, Pill, Button, Input, Segmented, ConfirmDialog,
  ErrorNotice, EmptyState, LoadingState, Spinner, PagedFooter, cx, toast,
} from '../ui/kit.jsx';

/**
 * Settings — port of the iOS `SettingsTab` (+ `SourceScheduleSettingsView`).
 * Device-only preferences live in localStorage; server-side settings talk to:
 *   GET /api/health                       {status,timestamp}
 *   GET /api/sources/search               the list (server-searched, paged)
 *   GET /api/sources/:id                  one sources row (schedule_mode), per visible row
 *   PUT /api/sources/:id/schedule         {mode:"map_cron"|"search_only"}   (platform operator)
 *   GET /api/status                       {summary, apis[]}  (probe consent; loaded on demand)
 */

const THEME_KEY = 'osint:theme';
function readTheme() {
  try { const t = window.localStorage.getItem(THEME_KEY); if (t === 'light' || t === 'dark') return t; } catch { /* ignore */ }
  return document.documentElement.getAttribute('data-theme') === 'light' ? 'light' : 'dark';
}
function applyTheme(t) {
  document.documentElement.setAttribute('data-theme', t);
  try { window.localStorage.setItem(THEME_KEY, t); } catch { /* ignore */ }
  // App.jsx owns the toggle state; tell it (and anyone else) the value moved.
  window.dispatchEvent(new CustomEvent('osint:theme-change', { detail: t }));
}

export default function SettingsPage() {
  const auth = useAuth();
  return (
    <Page title="Settings" subtitle="Backend, appearance, schedules and limits for this browser and this workspace." wide>
      <div className="grid gap-4 md:grid-cols-2">
        <div className="space-y-4">
          <BackendSection />
          <AppearanceSection />
          <ConnectionSection />
          <DataCacheSection />
        </div>
        <div className="space-y-4">
          {auth.isPlatformAdmin && <SourceScheduleSection />}
          {auth.isPlatformAdmin && <ProbeConsentSection />}
          <CredentialsSection />
          <AccountSection />
        </div>
      </div>
    </Page>
  );
}

/* ---------------------------------------------------------------- backend */

function BackendSection() {
  const [phase, setPhase] = useState('idle'); // idle | checking | live | failure
  const [result, setResult] = useState(null);
  const origin = typeof window !== 'undefined'
    ? (import.meta.env?.VITE_API_HOST ? `${window.location.protocol}//${import.meta.env.VITE_API_HOST}` : window.location.origin)
    : '';

  const check = async () => {
    setPhase('checking');
    const started = Date.now();
    try {
      const r = await api.get('/api/health');
      const ms = Date.now() - started;
      setResult({ ok: true, status: r?.status || 'unknown', timestamp: r?.timestamp || null, ms });
      setPhase('live');
    } catch (e) {
      setResult({ ok: false, error: e });
      setPhase('failure');
    }
    setTimeout(() => setPhase('idle'), 5000);
  };

  return (
    <Section label="Backend">
      <Row label="API origin" hint="The web client always talks to the origin that served it (or VITE_API_HOST).">
        <span className="text-osint-text break-all text-left">{origin}</span>
      </Row>
      <div className="pt-2">
        <Button variant={phase === 'live' ? 'primary' : phase === 'failure' ? 'danger' : 'secondary'} busy={phase === 'checking'} onClick={check} className="w-full justify-start">
          {phase === 'live' ? <LuCircleCheck size={14} className="text-neon-green" /> : phase === 'failure' ? <LuCircleX size={14} /> : <LuWifi size={14} />}
          {phase === 'idle' && 'Check connection'}
          {phase === 'checking' && 'Checking…'}
          {phase === 'live' && 'Server live'}
          {phase === 'failure' && 'Connection failed'}
        </Button>
      </div>
      {result?.ok && (
        <div className="mt-2 rounded-md border border-osint-border bg-osint-bg p-2 space-y-1 text-xs">
          <div className="flex items-center justify-between"><span className="text-osint-muted font-semibold">Health response</span><Pill tone="success">HTTP 200 · {result.ms}ms</Pill></div>
          <div className="flex items-center justify-between"><span className="text-osint-muted">Status</span><Pill tone={result.status === 'ok' ? 'success' : 'warning'}>{result.status}</Pill></div>
          {result.timestamp && (
            <div className="flex items-center justify-between"><span className="text-osint-muted">Timestamp</span>
              <span className="font-mono text-right">{fmtAbs(result.timestamp)}<br /><span className="text-osint-muted">{relativeTime(result.timestamp)}</span></span>
            </div>
          )}
        </div>
      )}
      {result && !result.ok && <ErrorNotice error={result.error} title="Health check failed" className="mt-2" />}
    </Section>
  );
}

/* ------------------------------------------------------------- appearance */

function AppearanceSection() {
  const [theme, setTheme] = useState(readTheme);
  useEffect(() => {
    const onChange = (e) => { if (e.detail === 'light' || e.detail === 'dark') setTheme(e.detail); };
    window.addEventListener('osint:theme-change', onChange);
    const mo = new MutationObserver(() => setTheme(readTheme()));
    mo.observe(document.documentElement, { attributes: true, attributeFilter: ['data-theme'] });
    return () => { window.removeEventListener('osint:theme-change', onChange); mo.disconnect(); };
  }, []);
  return (
    <Section label="Appearance">
      <Row label="Theme" hint="Situation-room dark (the iOS cyberpunk palette) or light.">
        <Segmented
          value={theme}
          onChange={(t) => { setTheme(t); applyTheme(t); }}
          options={[{ value: 'dark', label: 'Dark' }, { value: 'light', label: 'Light' }]}
        />
      </Row>
      <Row label="Preview">
        <span className="flex items-center gap-1">{theme === 'dark' ? <LuMoon size={13} /> : <LuSun size={13} />}<span className="font-mono">{theme}</span></span>
      </Row>
    </Section>
  );
}

/* ------------------------------------------------------------- connection */

function ConnectionSection() {
  const [open, setOpen] = useState(false);
  return (
    <Section label="Connection">
      <Row label="Supabase project" hint="Sign-in provider. Only change for a self-hosted project; takes effect at the next sign-in." onClick={() => setOpen((v) => !v)}>
        <span className="text-accent">{open ? 'Hide' : 'Edit'} ›</span>
      </Row>
      {open && <div className="pt-3"><ServerSettings /></div>}
    </Section>
  );
}

/* ------------------------------------------------------------ data & cache */

function countCache() {
  let layers = 0; let catalog = 0;
  try {
    for (let i = 0; i < sessionStorage.length; i += 1) {
      const k = sessionStorage.key(i);
      if (k && k.startsWith('useMapLayers:cache:')) layers += 1;
      if (k === 'useLayerCatalog:v2') catalog = 1;
    }
  } catch { /* blocked storage */ }
  return { layers, catalog };
}

function DataCacheSection() {
  const [cache, setCache] = useState(countCache);
  const [savedCount, setSavedCount] = useState(() => savedStore.all().length);
  const [confirm, setConfirm] = useState(null);
  useEffect(() => savedStore.subscribe((xs) => setSavedCount(xs.length)), []);

  const clearLayerCache = () => {
    try {
      const keys = [];
      for (let i = 0; i < sessionStorage.length; i += 1) keys.push(sessionStorage.key(i));
      keys.forEach((k) => { if (k && (k.startsWith('useMapLayers:cache:') || k === 'useLayerCatalog:v2')) sessionStorage.removeItem(k); });
    } catch { /* ignore */ }
    setCache(countCache());
    toast('Layer cache cleared — layers refetch on next map open', { tone: 'success' });
  };

  return (
    <Section label="Data & cache">
      <Row label="Cached layers" hint="Session cache of fetched layer features and the layer catalogue.">
        <span>{cache.layers} layer{cache.layers === 1 ? '' : 's'}{cache.catalog ? ' + catalogue' : ''}</span>
        <Button size="sm" variant="danger" disabled={!cache.layers && !cache.catalog} onClick={() => setConfirm('layers')}>Clear</Button>
      </Row>
      <Row label="Saved items" hint="Bookmarks kept in this browser only.">
        <span>{savedCount}</span>
        <Button size="sm" variant="danger" disabled={!savedCount} onClick={() => setConfirm('saved')}>Clear</Button>
      </Row>
      <Row label="Saved list"><Link to="/saved" className="text-accent hover:underline">Open ›</Link></Row>
      <ConfirmDialog
        open={confirm === 'layers'}
        onClose={() => setConfirm(null)}
        onConfirm={() => { setConfirm(null); clearLayerCache(); }}
        title="Clear cached layers?"
        confirmLabel="Clear and refetch"
        message="The cached layer features and catalogue are wiped from this tab and re-fetched from the backend the next time the map needs them."
      />
      <ConfirmDialog
        open={confirm === 'saved'}
        onClose={() => setConfirm(null)}
        onConfirm={() => { setConfirm(null); savedStore.clear(); toast('Saved items cleared'); }}
        title="Clear saved items?"
        confirmLabel={`Clear ${savedCount} items`}
        message="This removes every bookmark from this browser. Cases on the server are untouched."
      />
    </Section>
  );
}

/* ---------------------------------------------------- source scheduling */

/* Server-searched since 2026-11-07. This downloaded GET /api/sources — every
 * row of the sources table — and filtered it in the browser to show 60. The
 * list now comes from GET /api/sources/search (q, paged, measured total), and
 * each visible row reads its own mode from GET /api/sources/:id, because
 * search rows do not carry `schedule_mode`. What that costs, stated rather
 * than hidden: the fleet-wide "N cron · M search" tally is gone — no route
 * reports it without the whole table (a schedule_mode facet on search would). */
function SourceScheduleSection() {
  const [draft, setDraft] = useState('');
  const [q, setQ] = useState('');
  const list = usePagedList(searchPath({ q }), { pageSize: 25 });

  return (
    <Section label="Source scheduling" right={<Button size="sm" variant="ghost" onClick={() => list.reload()}><LuRefreshCw size={12} /></Button>}>
      <div className="text-[11px] text-osint-muted">Map cron pre-collects on a schedule so the map is ready instantly. Search only skips the schedule and grabs live from the Search tab. Both still feed Intel everywhere. Server-wide: platform operators only.</div>
      <form className="flex items-center gap-2 mt-2" onSubmit={(e) => { e.preventDefault(); setQ(draft.trim()); }}>
        <Input placeholder="Search sources (id, name, collector, host)…" value={draft} onChange={(e) => setDraft(e.target.value)} />
        <Button type="submit" size="sm">Search</Button>
      </form>
      {list.loading && list.rows.length === 0 && <LoadingState label="Searching sources…" />}
      {list.error && <ErrorNotice error={list.error} title="Could not search sources" onRetry={list.reload} className="mt-2" />}
      {list.loaded && !list.error && (
        <div className="mt-2 max-h-[420px] overflow-auto -mx-1 px-1">
          {list.rows.length === 0 && <EmptyState title="No sources match." />}
          {list.rows.map((r) => <ScheduleRow key={r.id} row={r} />)}
          {list.rows.length > 0 && <PagedFooter shown={list.rows.length} total={list.total} hasMore={list.hasMore} busy={list.loadingMore} onMore={list.loadMore} error={list.moreError} noun="sources" className="pt-2" />}
        </div>
      )}
    </Section>
  );
}

function ScheduleRow({ row }) {
  const one = useApi(`/api/sources/${encodeURIComponent(row.id)}`, { deps: [row.id] });
  const [saving, setSaving] = useState(false);
  const [saveError, setSaveError] = useState(null);
  const mode = one.data?.schedule_mode || (one.data ? 'map_cron' : null);
  const setMode = async (m) => {
    if (!mode || mode === m) return;
    setSaving(true); setSaveError(null);
    const prev = one.data;
    one.setData((d) => (d ? { ...d, schedule_mode: m } : d));
    try {
      await api.put(`/api/sources/${encodeURIComponent(row.id)}/schedule`, { mode: m });
    } catch (e) {
      one.setData(prev);
      setSaveError(e);
    } finally { setSaving(false); }
  };
  return (
    <div className="py-1 border-b border-osint-border last:border-0">
      <div className="flex items-center justify-between gap-2">
        <span className="min-w-0">
          <span className="block text-xs text-osint-text truncate">{row.name || row.id}</span>
          <span className="block text-[10px] text-osint-muted font-mono truncate">{row.id}{row.category ? ` · ${row.category}` : ''}</span>
        </span>
        <span className="flex items-center gap-1.5">
          {(saving || one.loading) && <Spinner size={11} />}
          {one.error
            ? <span className="text-[10px] text-neon-red font-mono">mode unreadable — {errorMessage(one.error)}</span>
            : mode && <Segmented value={mode} onChange={setMode} options={[{ value: 'map_cron', label: 'Map cron' }, { value: 'search_only', label: 'Search only' }]} />}
        </span>
      </div>
      {saveError && <div className="text-[10px] text-neon-red font-mono">Schedule change refused — {errorMessage(saveError)}</div>}
    </div>
  );
}

/* --------------------------------------------------------- probe consent */

function ProbeConsentSection() {
  /* Loaded on demand (useKeyedSources): this downloaded the whole
   * /api/status catalogue every time Settings opened, to list the keyed
   * sources, and no smaller route can filter to them yet. */
  const keyed = useKeyedSources();
  const [q, setQ] = useState('');
  const rows = keyed.rows;
  const filtered = useMemo(() => {
    const qq = q.trim().toLowerCase();
    return qq ? rows.filter((r) => (r.name || '').toLowerCase().includes(qq) || r.id.toLowerCase().includes(qq)) : rows;
  }, [rows, q]);
  const LIMIT = 40;
  const [expanded, setExpanded] = useState(false);
  const shown = expanded ? filtered : filtered.slice(0, LIMIT);
  const onUpdate = (updated) => keyed.setData((d) => (d ? { ...d, apis: mergeStatusRow(d.apis, updated) } : d));

  return (
    <Section label="Probe consent">
      <div className="text-[11px] text-osint-muted">Keyed sources are only auto-probed by the scheduler once you allow it here; a manual probe uses the configured key so the request actually reaches the upstream. Platform operators only.</div>
      {!keyed.wanted && (
        <div className="flex items-center justify-between gap-2 mt-2 text-[11px] text-osint-muted">
          <span>Not loaded: listing keyed sources downloads the full source-status catalogue (no smaller route filters to them).</span>
          <Button size="sm" onClick={keyed.load}>Load keyed sources</Button>
        </div>
      )}
      {keyed.wanted && (
        <div className="flex items-center gap-2 mt-2">
          <Input placeholder="Filter keyed sources…" value={q} onChange={(e) => setQ(e.target.value)} />
          {keyed.loaded && <span className="text-[10px] font-mono text-osint-muted whitespace-nowrap">{rows.filter((r) => r.probeConsent).length}/{rows.length} allowed</span>}
        </div>
      )}
      {keyed.loading && !keyed.data && <LoadingState label="Loading status…" />}
      {keyed.error && <ErrorNotice error={keyed.error} title="Could not load source status" onRetry={keyed.load} className="mt-2" />}
      {keyed.data && (
        <div className="mt-2 max-h-[420px] overflow-auto -mx-1 px-1">
          {shown.length === 0 && <EmptyState title={rows.length === 0 ? 'No keyed sources in the registry.' : 'No keyed sources match.'} />}
          {shown.map((r) => (
            <div key={r.id} className="py-1.5 border-b border-osint-border last:border-0">
              <div className="flex items-center gap-2">
                <span className={cx('w-2 h-2 rounded-full flex-shrink-0', `status-${r.status || 'pending'}`)} />
                <span className="min-w-0 flex-1">
                  <span className="block text-xs text-osint-text truncate">{r.name || r.id}</span>
                  <span className="block text-[10px] text-osint-muted font-mono truncate">{r.id}{r.category ? ` · ${r.category}` : ''}</span>
                </span>
                <Pill tone={r.probeConsent ? 'success' : r.gated ? 'warning' : 'neutral'}>{r.probeConsent ? 'ALLOWED' : r.gated ? 'GATED' : 'MANUAL'}</Pill>
                {r.configured === false && <Pill tone="danger">NO KEY</Pill>}
              </div>
              <ProbeActions row={r} onUpdate={onUpdate} />
            </div>
          ))}
          {filtered.length > shown.length && (
            <div className="flex items-center justify-between pt-2 text-[11px] font-mono text-accent">
              <span>showing {shown.length} of {filtered.length} sources</span>
              <Button size="sm" onClick={() => setExpanded(true)}>Show all</Button>
            </div>
          )}
        </div>
      )}
    </Section>
  );
}

/* ------------------------------------------------------------ credentials */

function CredentialsSection() {
  const auth = useAuth();
  return (
    <Section label="Credentials">
      <Row label="API keys (BYOK)" hint="Stored encrypted per workspace. Not yet used by collectors, which run on the platform keys.">
        <Link to="/console/api-keys" className="text-accent hover:underline">Open ›</Link>
      </Row>
      {auth.role === 'owner' && (
        <Row label="Key-edit policy" hint="Who, besides owner and admins, may edit this workspace's keys.">
          <Link to="/console/workspace" className="text-accent hover:underline">Workspace ›</Link>
        </Row>
      )}
      {auth.isPlatformAdmin && (
        <Row label="Platform keys" hint="The credentials collectors actually run with.">
          <Link to="/console/api-keys?scope=platform" className="text-accent hover:underline">Open ›</Link>
        </Row>
      )}
    </Section>
  );
}

/* ---------------------------------------------------------------- account */

function AccountSection() {
  const auth = useAuth();
  const [confirm, setConfirm] = useState(false);
  return (
    <Section label="Account">
      {auth.accountEmail && <Row label="Signed in as"><span className="text-osint-text break-all">{auth.accountEmail}</span></Row>}
      {auth.tenantName && <Row label="Workspace">{auth.tenantName}</Row>}
      {auth.role && <Row label="Role"><Pill tone={auth.role === 'owner' ? 'accent' : 'neutral'}>{auth.role}</Pill></Row>}
      <Row label="Platform operator" hint="Derived from the server's answer to an operator-only route, never assumed.">
        {auth.isPlatformAdmin === null ? <span>unknown</span> : <Pill tone={auth.isPlatformAdmin ? 'accent' : 'neutral'}>{auth.isPlatformAdmin ? 'YES' : 'NO'}</Pill>}
      </Row>
      <Row label={<span className="text-neon-red">Disconnect</span>} hint="Switch backend, workspace or account without reinstalling anything.">
        <Button variant="danger" size="sm" onClick={() => setConfirm(true)}>Disconnect</Button>
      </Row>
      <ConfirmDialog
        open={confirm}
        onClose={() => setConfirm(false)}
        onConfirm={() => { setConfirm(false); auth.signOut(); }}
        title="Disconnect?"
        confirmLabel="Disconnect"
        message="Signs you out and returns to the sign-in screen. Saved items and cached layers in this browser are kept."
      />
    </Section>
  );
}

