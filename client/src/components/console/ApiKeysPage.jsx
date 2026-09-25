import React, { useMemo, useState } from 'react';
import { Link, useNavigate, useSearchParams } from 'react-router-dom';
import { LuKey, LuKeyRound, LuLock, LuRefreshCw } from 'react-icons/lu';
import { api, ApiError, isForbidden } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { useAuth } from '../../auth/AuthContext.jsx';
import {
  Page, Section, Row, Card, Pill, Button, Input, Field, Select, Segmented, Sheet,
  ConfirmDialog, ErrorNotice, EmptyState, LoadingState, CopyButton, cx, toast,
} from '../ui/kit.jsx';

/**
 * API keys — port of the iOS `ApiKeysView` / `ApiKeyDetailView` /
 * `KeyPolicySettingsView`.
 *
 * Two scopes:
 *   workspace  GET /api/tenant-keys            {items[{name,role,byok,set,source}],canManage,policy,policyMemberId}
 *              GET /api/tenant-keys/:name      {name,value|null}  (owner/admin only — reveal)
 *              PUT /api/tenant-keys/:name      {value}  ("" clears)  → {name,byok,set,source}
 *              GET/PUT /api/tenant-keys/policy {policy,memberId,members[]}
 *   platform   GET /api/keys                   [{name,role,set,hasOverlay}]   (platform operator + admin role)
 *              GET/PUT /api/keys/:name
 * "Used by" comes from GET /api/status → apis[].envVars[{name,role}].
 */

const ROLE_TONE = { required: 'accent', anyOf: 'warning', optional: 'neutral' };
const ROLE_RANK = { required: 0, anyOf: 1, optional: 2 };

function normalizeRow(raw, scope) {
  if (scope === 'platform') {
    return {
      name: raw.name, role: raw.role, set: Boolean(raw.set), byok: Boolean(raw.hasOverlay),
      source: raw.set ? (raw.hasOverlay ? 'tenant' : 'platform') : null,
    };
  }
  return { name: raw.name, role: raw.role, set: Boolean(raw.set), byok: Boolean(raw.byok), source: raw.source ?? null };
}

export default function ApiKeysPage() {
  const auth = useAuth();
  const [params, setParams] = useSearchParams();
  const scopeParam = params.get('scope') === 'platform' ? 'platform' : 'workspace';
  const [scope, setScopeState] = useState(scopeParam);
  const setScope = (s) => { setScopeState(s); setParams(s === 'platform' ? { scope: 'platform' } : {}, { replace: true }); };

  const listPath = scope === 'platform' ? '/api/keys' : '/api/tenant-keys';
  const { data, error, loading, reload, setData } = useApi(listPath, { deps: [scope] });
  const status = useApi('/api/status');

  const [q, setQ] = useState('');
  const [statusFilter, setStatusFilter] = useState('all');
  const [roleFilter, setRoleFilter] = useState('');
  const [selected, setSelected] = useState(params.get('key') || null);
  const [showPolicy, setShowPolicy] = useState(false);

  const rows = useMemo(() => {
    if (!data) return [];
    const raw = scope === 'platform' ? (Array.isArray(data) ? data : []) : (data.items || []);
    return raw.map((r) => normalizeRow(r, scope));
  }, [data, scope]);
  const canManage = scope === 'platform' ? true : (data?.canManage ?? true);
  const policy = scope === 'platform' ? null : data?.policy;
  const statusRows = status.data?.apis || [];

  const roles = useMemo(() => [...new Set(rows.map((r) => r.role))].sort((a, b) => (ROLE_RANK[a] ?? 9) - (ROLE_RANK[b] ?? 9)), [rows]);
  const filtered = useMemo(() => {
    const qq = q.trim().toLowerCase();
    return rows.filter((r) => {
      if (statusFilter === 'set' && !r.set) return false;
      if (statusFilter === 'unset' && r.set) return false;
      if (statusFilter === 'mine' && !r.byok) return false;
      if (roleFilter && r.role !== roleFilter) return false;
      if (qq && !r.name.toLowerCase().includes(qq)) return false;
      return true;
    });
  }, [rows, q, statusFilter, roleFilter]);

  const counts = useMemo(() => ({
    all: rows.length, set: rows.filter((r) => r.set).length,
    unset: rows.filter((r) => !r.set).length, mine: rows.filter((r) => r.byok).length,
  }), [rows]);

  const onUpdated = (updated) => {
    setData((d) => {
      if (!d) return d;
      if (scope === 'platform') return (Array.isArray(d) ? d : []).map((r) => (r.name === updated.name ? { ...r, set: updated.set, hasOverlay: updated.byok } : r));
      return { ...d, items: (d.items || []).map((r) => (r.name === updated.name ? { ...r, ...updated } : r)) };
    });
  };

  const selectedRow = selected ? rows.find((r) => r.name === selected) : null;

  return (
    <Page
      title={scope === 'platform' ? 'Platform keys' : 'API keys'}
      subtitle={scope === 'platform'
        ? 'Server-wide default credentials every workspace falls back to. Platform operators only.'
        : 'Bring your own keys. Each value is encrypted per workspace and injected into collectors at run time.'}
      actions={(
        <>
          {auth.isPlatformAdmin && (
            <Segmented value={scope} onChange={setScope} options={[{ value: 'workspace', label: 'Workspace' }, { value: 'platform', label: 'Platform' }]} />
          )}
          <Button size="sm" onClick={() => { reload(); status.reload(); }} title="Reload"><LuRefreshCw size={13} /></Button>
        </>
      )}
      wide
    >
      {scope === 'workspace' && data && (
        <Card className="flex flex-wrap items-center justify-between gap-2">
          <div className="text-xs text-osint-muted flex items-center gap-2">
            {canManage ? <LuKeyRound size={14} className="text-accent" /> : <LuLock size={14} />}
            {canManage
              ? <span>You can edit this workspace's keys. Policy: <span className="font-mono text-osint-text">{policy || '—'}</span></span>
              : <span>Read-only — your workspace owner restricts who can edit keys (policy <span className="font-mono">{policy}</span>).</span>}
          </div>
          {auth.role === 'owner' && <Button size="sm" onClick={() => setShowPolicy(true)}>Key-edit policy</Button>}
        </Card>
      )}

      <div className="flex flex-wrap items-center gap-2">
        <Input placeholder="Search keys…" value={q} onChange={(e) => setQ(e.target.value)} className="max-w-xs" mono />
        <Segmented
          value={statusFilter}
          onChange={setStatusFilter}
          options={[
            { value: 'all', label: 'All', count: counts.all },
            { value: 'set', label: 'Set', count: counts.set },
            { value: 'unset', label: 'Unset', count: counts.unset },
            { value: 'mine', label: scope === 'platform' ? 'Overlay' : 'Custom', count: counts.mine },
          ]}
        />
        <Select value={roleFilter} onChange={(e) => setRoleFilter(e.target.value)}>
          <option value="">Any role</option>
          {roles.map((r) => <option key={r} value={r}>{r}</option>)}
        </Select>
      </div>

      {loading && <LoadingState label="Loading keys…" />}
      {error && (
        <ErrorNotice error={error} title={isForbidden(error) ? 'The server refused this account' : 'Could not load keys'} onRetry={reload}>
          {isForbidden(error) && scope === 'platform' && 'Platform keys need the platform-operator allowlist AND an owner/admin role in the active workspace.'}
        </ErrorNotice>
      )}
      {status.error && <ErrorNotice error={status.error} title="Source status unavailable (the “used by” lists will be empty)" onRetry={status.reload} />}

      {!loading && !error && (
        filtered.length === 0 ? (
          <EmptyState title={rows.length === 0 ? 'The server lists no keys.' : `No keys match${q ? ` “${q}”` : ' the current filters'}.`} />
        ) : (
          <Card padded={false}>
            <ul className="divide-y divide-osint-border">
              {filtered.map((r) => {
                const usedBy = statusRows.filter((s) => (s.envVars || []).some((v) => v.name === r.name)).length;
                return (
                  <li key={r.name}>
                    <button type="button" onClick={() => setSelected(r.name)} className="w-full flex items-center gap-3 px-3 py-2.5 text-left hover:bg-white/5">
                      <LuKey size={15} className={r.set ? 'text-neon-green' : 'text-osint-muted'} />
                      <span className="min-w-0 flex-1">
                        <span className="block font-mono text-sm text-osint-text truncate">{r.name}</span>
                        <span className="flex items-center gap-1 mt-0.5">
                          <Pill tone={ROLE_TONE[r.role] || 'neutral'}>{String(r.role).toUpperCase()}</Pill>
                          {scope === 'workspace' && r.byok && <Pill tone="accent">YOUR KEY</Pill>}
                          {scope === 'workspace' && !r.byok && r.source === 'platform' && <Pill>ADMIN</Pill>}
                          {scope === 'platform' && r.byok && <Pill tone="accent">OVERLAY</Pill>}
                          {usedBy > 0 && <span className="text-[10px] text-osint-muted font-mono ml-1">{usedBy} source{usedBy === 1 ? '' : 's'}</span>}
                        </span>
                      </span>
                      <Pill tone={r.set ? 'success' : 'warning'}>{r.set ? 'SET' : 'UNSET'}</Pill>
                      <span className="text-osint-muted">›</span>
                    </button>
                  </li>
                );
              })}
            </ul>
          </Card>
        )
      )}

      <KeyDetailSheet
        row={selectedRow}
        scope={scope}
        canManage={canManage}
        statusRows={statusRows}
        onStatusUpdate={(updated) => status.setData((d) => (d ? { ...d, apis: (d.apis || []).map((s) => (s.id === updated.id ? updated : s)) } : d))}
        onClose={() => setSelected(null)}
        onUpdated={onUpdated}
      />
      <KeyPolicySheet open={showPolicy} onClose={() => setShowPolicy(false)} onSaved={() => reload({ silent: true })} />
    </Page>
  );
}

/* ------------------------------------------------------------------------ */

function KeyDetailSheet({ row, scope, canManage, statusRows, onStatusUpdate, onClose, onUpdated }) {
  const navigate = useNavigate();
  const [working, setWorking] = useState(row);
  const [revealed, setRevealed] = useState(null);
  const [newValue, setNewValue] = useState('');
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState(null);
  const [saved, setSaved] = useState(false);
  const [confirmClear, setConfirmClear] = useState(false);
  const rowName = row?.name;

  React.useEffect(() => { setWorking(row); setRevealed(null); setNewValue(''); setError(null); setSaved(false); }, [rowName, row]);

  if (!row || !working) return null;
  const base = scope === 'platform' ? '/api/keys' : '/api/tenant-keys';
  const usedBy = statusRows.filter((s) => (s.envVars || []).some((v) => v.name === row.name));
  const roleFor = (s) => (s.envVars || []).find((v) => v.name === row.name)?.role;

  const sourceLabel = working.source === 'tenant'
    ? (scope === 'workspace' ? 'Your key' : 'Overlay')
    : working.source === 'platform' ? (scope === 'workspace' ? 'Set by admin' : 'Platform default') : 'Not set';
  const revealEmpty = scope === 'workspace'
    ? (working.source === 'platform' ? '(provided by admin — value hidden)' : '(no key set for this workspace)')
    : '(no value set)';

  const reveal = async () => {
    setBusy(true); setError(null);
    try {
      const v = await api.get(`${base}/${encodeURIComponent(row.name)}`);
      setRevealed(v?.value ?? '');
      setNewValue(v?.value ?? '');
    } catch (e) { setError(e); }
    finally { setBusy(false); }
  };

  const write = async (value) => {
    setBusy(true); setError(null); setSaved(false);
    try {
      const w = await api.put(`${base}/${encodeURIComponent(row.name)}`, { value });
      const updated = scope === 'platform'
        ? normalizeRow(w, 'platform')
        : { name: w.name, role: working.role, set: Boolean(w.set), byok: Boolean(w.byok), source: w.source ?? null };
      setWorking(updated);
      onUpdated(updated);
      setSaved(true);
      if (value) { setRevealed(value); toast('Key saved', { tone: 'success' }); }
      else { setRevealed(null); setNewValue(''); toast('Key cleared'); }
    } catch (e) { setError(e); }
    finally { setBusy(false); }
  };

  return (
    <Sheet open onClose={onClose} title={row.name} width="max-w-xl" footer={<Button onClick={onClose}>Done</Button>}>
      <div className="space-y-4">
        <Section label="Key">
          <Row label="Name"><span className="text-osint-text">{working.name}</span></Row>
          <Row label="Role">{working.role}</Row>
          <Row label="Status"><span className={working.set ? 'text-neon-green' : 'text-accent'}>{working.set ? 'Set' : 'Unset'}</span></Row>
          <Row label="Source"><span className={working.byok ? 'text-accent' : ''}>{sourceLabel}</span></Row>
          {scope === 'workspace' && <div className="text-[11px] text-osint-muted pt-2">Your key is encrypted per workspace. When unset, collectors fall back to the platform default automatically.</div>}
        </Section>

        <Section label="Current value">
          {revealed === null ? (
            <div className="space-y-1.5">
              <Button variant="primary" busy={busy} onClick={reveal}>Reveal value</Button>
              <div className="text-[11px] text-osint-muted">
                {scope === 'workspace' ? "Reveals your workspace's own key only (never the platform key). Owner or admin role required." : 'Reveals the overlay value; the .env-baked value is never returned.'}
              </div>
            </div>
          ) : revealed === '' ? (
            <div className="text-sm text-osint-muted italic">{revealEmpty}</div>
          ) : (
            <div className="flex items-start gap-2">
              <code className="flex-1 font-mono text-xs break-all bg-osint-bg border border-osint-border rounded-md p-2 text-osint-text">{revealed}</code>
              <CopyButton text={revealed} />
            </div>
          )}
        </Section>

        {canManage ? (
          <Section label="Modify">
            <form className="flex gap-2" onSubmit={(e) => { e.preventDefault(); if (newValue) write(newValue); }}>
              <Input type="password" mono placeholder="New value" value={newValue} onChange={(e) => setNewValue(e.target.value)} autoComplete="off" />
              <Button type="submit" variant="primary" busy={busy} disabled={!newValue}>Save</Button>
            </form>
            <div className="text-[11px] text-osint-muted pt-2">Empty values aren't accepted here — use Clear to drop back to the {scope === 'workspace' ? 'platform default' : '.env-baked value'}.</div>
            {working.byok && (
              <div className="pt-2">
                <Button variant="danger" size="sm" busy={busy} onClick={() => setConfirmClear(true)}>Clear</Button>
              </div>
            )}
          </Section>
        ) : (
          <Card className="text-xs text-osint-muted flex items-center gap-2"><LuLock size={13} /> Editing is restricted by your workspace owner.</Card>
        )}

        {error && <ErrorNotice error={error} title={error instanceof ApiError && error.status === 403 ? 'The server refused this account' : 'Request failed'} />}
        {saved && !error && <div className="text-xs text-neon-green">Saved.</div>}

        <Section label={`Used by · ${usedBy.length}`}>
          {usedBy.length === 0 ? (
            <div className="text-xs text-osint-muted">No sources reference this key in the current registry{statusRows.length === 0 ? ' (source status did not load)' : ''}.</div>
          ) : (
            <ul className="divide-y divide-osint-border -my-1">
              {usedBy.map((s) => (
                <li key={s.id} className="py-1.5">
                  <div className="flex items-center gap-2">
                    <span className={cx('w-2 h-2 rounded-full flex-shrink-0', `status-${s.status || 'pending'}`)} />
                    <button type="button" onClick={() => { onClose(); navigate(`/console/sources?source=${encodeURIComponent(s.id)}`); }} className="min-w-0 flex-1 text-left hover:text-accent">
                      <span className="block text-xs text-osint-text truncate">{s.name || s.id}</span>
                      <span className="block text-[10px] text-osint-muted">{s.category}{s.gated ? ' · gated' : ''}{s.configured === false ? ' · not configured' : ''}</span>
                    </button>
                    {roleFor(s) && <Pill tone={ROLE_TONE[roleFor(s)] || 'neutral'}>{String(roleFor(s)).toUpperCase()}</Pill>}
                  </div>
                  {s.requiresKey && <ProbeActions row={s} onUpdate={onStatusUpdate} />}
                </li>
              ))}
            </ul>
          )}
        </Section>
      </div>

      <ConfirmDialog
        open={confirmClear}
        onClose={() => setConfirmClear(false)}
        onConfirm={() => { setConfirmClear(false); write(''); }}
        title="Clear this key?"
        confirmLabel="Clear"
        message={scope === 'workspace' ? "Removes your workspace's key. Collectors fall back to the platform default if one exists." : 'Removes the overlay value. The server falls back to the .env-baked value if any.'}
      />
    </Sheet>
  );
}

/** Probe / consent controls for a keyed source (iOS `ProbeActionsView`).
 *  POST /api/status/:id/probe · POST /api/status/:id/consent {consent} — both operator-gated. */
export function ProbeActions({ row, onUpdate }) {
  const [busy, setBusy] = useState(null);
  const [error, setError] = useState(null);
  const run = async (kind) => {
    setBusy(kind); setError(null);
    try {
      const r = kind === 'probe'
        ? await api.post(`/api/status/${encodeURIComponent(row.id)}/probe`)
        : await api.post(`/api/status/${encodeURIComponent(row.id)}/consent`, { consent: !row.probeConsent });
      if (r && r.id) onUpdate?.(r);
      toast(kind === 'probe' ? `Probe: ${r?.status || 'done'}` : (row.probeConsent ? 'Auto-probe disabled' : 'Auto-probe allowed'));
    } catch (e) { setError(e); }
    finally { setBusy(null); }
  };
  return (
    <div className="mt-1 ml-4 flex flex-wrap items-center gap-1.5">
      <Button size="sm" busy={busy === 'probe'} onClick={() => run('probe')}>Probe now</Button>
      <Button size="sm" busy={busy === 'consent'} onClick={() => run('consent')}>{row.probeConsent ? 'Disallow auto-probe' : 'Allow auto-probe'}</Button>
      {row.missingVars?.length > 0 && <span className="text-[10px] text-accent font-mono">missing: {row.missingVars.join(', ')}</span>}
      {error && <span className="text-[10px] text-neon-red font-mono">{error.message}</span>}
    </div>
  );
}

/* ------------------------------------------------------------------------ */

const POLICY_CHOICES = [
  { value: 'owner_only', label: 'Owner only', help: 'Only you (owner) and workspace admins can edit keys.' },
  { value: 'selected_member', label: 'A chosen member', help: 'One delegate you pick, plus owner/admins, can edit keys.' },
  { value: 'all_members', label: 'All members', help: 'Anyone in the workspace can edit this workspace’s keys.' },
];

export function KeyPolicySheet({ open, onClose, onSaved }) {
  const { data, error, loading, reload } = useApi(open ? '/api/tenant-keys/policy' : null, { deps: [open] });
  const [policy, setPolicy] = useState(null);
  const [memberId, setMemberId] = useState(null);
  const [busy, setBusy] = useState(false);
  const [saveError, setSaveError] = useState(null);
  React.useEffect(() => { if (data) { setPolicy(data.policy); setMemberId(data.memberId ?? null); } }, [data]);
  const members = data?.members || [];
  const cur = policy ?? data?.policy ?? 'owner_only';

  const save = async () => {
    setBusy(true); setSaveError(null);
    try {
      const body = { policy: cur };
      if (cur === 'selected_member' && memberId) body.memberId = memberId;
      const r = await api.put('/api/tenant-keys/policy', body);
      setPolicy(r.policy); setMemberId(r.memberId ?? null);
      toast('Policy saved', { tone: 'success' });
      onSaved?.();
    } catch (e) { setSaveError(e); }
    finally { setBusy(false); }
  };

  return (
    <Sheet open={open} onClose={onClose} title="Key-edit policy" width="max-w-md"
      footer={(
        <>
          <Button onClick={onClose}>Close</Button>
          <Button variant="primary" busy={busy} disabled={!data || (cur === 'selected_member' && !memberId)} onClick={save}>Save policy</Button>
        </>
      )}
    >
      {loading && <LoadingState />}
      {error && <ErrorNotice error={error} title="Could not load the policy" onRetry={reload} />}
      {data && (
        <div className="space-y-3">
          <div className="space-y-1">
            {POLICY_CHOICES.map((c) => (
              <label key={c.value} className={cx('flex items-start gap-2 rounded-md border px-3 py-2 cursor-pointer', cur === c.value ? 'border-accent/50 bg-accent/5' : 'border-osint-border')}>
                <input type="radio" name="policy" className="mt-0.5 accent-[rgb(var(--accent))]" checked={cur === c.value} onChange={() => setPolicy(c.value)} />
                <span>
                  <span className="block text-sm text-osint-text">{c.label}</span>
                  <span className="block text-[11px] text-osint-muted">{c.help}</span>
                </span>
              </label>
            ))}
          </div>
          {cur === 'selected_member' && (
            <Field label="Delegate">
              {members.length === 0 ? (
                <div className="text-xs text-osint-muted">No other members in this workspace yet.</div>
              ) : (
                <Select className="w-full" value={memberId || ''} onChange={(e) => setMemberId(e.target.value || null)}>
                  <option value="">None</option>
                  {members.map((m) => <option key={m.id} value={m.id}>{m.email} · {m.role}</option>)}
                </Select>
              )}
            </Field>
          )}
          {saveError && <ErrorNotice error={saveError} title="Save failed" />}
          <div className="text-[11px] text-osint-muted">Owner and admins can always edit keys regardless of this setting.</div>
        </div>
      )}
    </Sheet>
  );
}

/** Small link used by other pages. */
export function KeyPolicyLink() {
  return <Link to="/console/api-keys" className="text-accent hover:underline text-xs">Open API keys</Link>;
}
