import React, { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { LuUsers, LuSearch, LuSparkles, LuX, LuRefreshCw } from 'react-icons/lu';
import { api } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { useAuth } from '../../auth/AuthContext.jsx';
import { useSearchStore } from '../../hooks/useSearch.js';
import { KeyPolicySheet } from './ApiKeysPage.jsx';
import {
  Page, Section, Row, Card, Pill, Button, Input, Field, Select, Segmented,
  ConfirmDialog, ErrorNotice, EmptyState, LoadingState, BoundNote, Spinner, cx, toast,
} from '../ui/kit.jsx';

/**
 * Workspace — port of the iOS `WorkspaceSettingsTab` + `WorkspaceMembersView`
 * + `WorkspaceQueryView`.
 *
 *   GET    /api/members                      {members[{user_id,email,role}], invites[{id,email,role,created_at}]}
 *   POST   /api/members/invite   {email,role} → {ok,status:"invited"|"added"|"already_member",…}
 *   PATCH  /api/members/:userId/role {role}
 *   DELETE /api/members/:userId
 *   DELETE /api/members/invite/:id
 * Queries: GET /api/intel/items?q=&limit= (FTS), the search pipeline store
 * (LLM), GET /api/search/suggest?q= (draft an FTS string).
 */
const ROLES = ['owner', 'admin', 'analyst', 'viewer'];
const ROLE_TONE = { owner: 'accent', admin: 'cyan', analyst: 'neutral', viewer: 'neutral' };

export default function WorkspacePage() {
  const auth = useAuth();
  const [tab, setTab] = useState('members');
  const [showPolicy, setShowPolicy] = useState(false);
  const isOwner = auth.role === 'owner';

  return (
    <Page
      title="Workspace"
      subtitle="Invite users by email and manage their roles, or build FTS / LLM-pipeline queries and save them as alerts."
      actions={<Segmented value={tab} onChange={setTab} options={[{ value: 'members', label: 'Members' }, { value: 'queries', label: 'Queries' }, { value: 'switch', label: 'Switch' }]} />}
      wide
    >
      {tab === 'members' && <MembersView />}
      {tab === 'queries' && <QueryView />}
      {tab === 'switch' && <SwitchView />}

      {isOwner && (
        <Section label="Credentials">
          <Row label="Key-edit policy" hint="Choose who, besides owner and admins, may edit this workspace's API keys." onClick={() => setShowPolicy(true)}>
            <span className="text-accent">Edit ›</span>
          </Row>
        </Section>
      )}
      <KeyPolicySheet open={showPolicy} onClose={() => setShowPolicy(false)} />
    </Page>
  );
}

/* ---------------------------------------------------------------- members */

function MembersView() {
  const auth = useAuth();
  const { data, error, loading, reload } = useApi('/api/members');
  const [email, setEmail] = useState('');
  const [role, setRole] = useState('analyst');
  const [working, setWorking] = useState(false);
  const [busyId, setBusyId] = useState(null);
  const [actError, setActError] = useState(null);
  const [lastStatus, setLastStatus] = useState(null);
  const [removeTarget, setRemoveTarget] = useState(null);
  const members = data?.members || [];
  const invites = data?.invites || [];
  const myId = auth.me?.user?.id;

  const blurb = (s) => ({ added: 'Added to workspace.', already_member: 'Already a member.', invited: 'Invite sent — applied on first sign-in.' }[s] || 'Done.');

  const invite = async (e) => {
    e.preventDefault();
    const em = email.trim();
    if (!em.includes('@')) { setActError(new Error('Enter a valid email')); return; }
    setWorking(true); setActError(null); setLastStatus(null);
    try {
      const r = await api.post('/api/members/invite', { email: em, role });
      setLastStatus(r?.status || 'ok');
      setEmail('');
      await reload({ silent: true });
    } catch (err) { setActError(err); }
    finally { setWorking(false); }
  };

  const setMemberRole = async (m, newRole) => {
    if (newRole === m.role) return;
    setBusyId(m.user_id); setActError(null);
    try {
      await api.patch(`/api/members/${encodeURIComponent(m.user_id)}/role`, { role: newRole });
      toast(`${m.email} → ${newRole}`);
      await reload({ silent: true });
      if (m.user_id === myId) auth.refreshMe();
    } catch (err) { setActError(err); }
    finally { setBusyId(null); }
  };

  const remove = async (m) => {
    setBusyId(m.user_id); setActError(null);
    try {
      await api.del(`/api/members/${encodeURIComponent(m.user_id)}`);
      toast(`Removed ${m.email}`);
      await reload({ silent: true });
    } catch (err) { setActError(err); }
    finally { setBusyId(null); setRemoveTarget(null); }
  };

  const revoke = async (inv) => {
    setBusyId(inv.id); setActError(null);
    try {
      await api.del(`/api/members/invite/${encodeURIComponent(inv.id)}`);
      await reload({ silent: true });
    } catch (err) { setActError(err); }
    finally { setBusyId(null); }
  };

  return (
    <div className="space-y-4">
      <Section label="Invite by email">
        <form onSubmit={invite} className="flex flex-wrap gap-2 items-end">
          <Field label="Email" className="flex-1 min-w-[200px]">
            <Input type="email" mono placeholder="name@example.com" value={email} onChange={(e) => setEmail(e.target.value)} autoComplete="off" />
          </Field>
          <Field label="Role">
            <Select value={role} onChange={(e) => setRole(e.target.value)}>
              {ROLES.map((r) => <option key={r} value={r} disabled={r === 'owner' && auth.role !== 'owner'}>{r}</option>)}
            </Select>
          </Field>
          <Button type="submit" variant="primary" busy={working} disabled={!email.includes('@')}>Invite</Button>
        </form>
        {lastStatus && <div className="text-xs text-neon-green mt-2">{blurb(lastStatus)}</div>}
        <div className="text-[11px] text-osint-muted mt-2">If the address already has an account, they join immediately. Otherwise the invite is held and applied the first time they sign in. Only an owner can grant the owner role.</div>
      </Section>

      {actError && <ErrorNotice error={actError} title="Action failed" />}
      {loading && !data && <LoadingState label="Loading members…" />}
      {error && <ErrorNotice error={error} title="Could not load members" onRetry={reload} />}

      {data && (
        <>
          <Section label={`Members · ${members.length}`} right={<Button size="sm" variant="ghost" onClick={() => reload()}><LuRefreshCw size={12} /></Button>} padded={false}>
            {members.length === 0 ? (
              <EmptyState title="No members returned." />
            ) : (
              <ul className="divide-y divide-osint-border">
                {members.map((m) => (
                  <li key={m.user_id} className="flex items-center gap-3 px-3 py-2">
                    <span className="min-w-0 flex-1">
                      <span className="block font-mono text-sm text-osint-text truncate">{m.email}</span>
                      <span className="block text-[10px] text-osint-muted font-mono">{m.user_id}{m.user_id === myId ? ' · you' : ''}</span>
                    </span>
                    <Pill tone={ROLE_TONE[m.role] || 'neutral'}>{m.role}</Pill>
                    <Select value={m.role} disabled={busyId === m.user_id} onChange={(e) => setMemberRole(m, e.target.value)}>
                      {ROLES.map((r) => <option key={r} value={r}>{r}</option>)}
                    </Select>
                    {busyId === m.user_id ? <Spinner size={12} /> : (
                      <Button size="sm" variant="danger" onClick={() => setRemoveTarget(m)} title="Remove from workspace"><LuX size={12} /></Button>
                    )}
                  </li>
                ))}
              </ul>
            )}
          </Section>

          {invites.length > 0 && (
            <Section label={`Pending invites · ${invites.length}`} padded={false}>
              <ul className="divide-y divide-osint-border">
                {invites.map((inv) => (
                  <li key={inv.id} className="flex items-center gap-3 px-3 py-2">
                    <span className="min-w-0 flex-1">
                      <span className="block font-mono text-sm text-osint-text truncate">{inv.email}</span>
                      <span className="block text-[10px] text-osint-muted">invited as {inv.role}{inv.created_at ? ` · ${inv.created_at}` : ''}</span>
                    </span>
                    <Button size="sm" variant="danger" busy={busyId === inv.id} onClick={() => revoke(inv)}>Revoke</Button>
                  </li>
                ))}
              </ul>
            </Section>
          )}
        </>
      )}

      <ConfirmDialog
        open={Boolean(removeTarget)}
        onClose={() => setRemoveTarget(null)}
        onConfirm={() => remove(removeTarget)}
        busy={Boolean(removeTarget) && busyId === removeTarget?.user_id}
        title="Remove member?"
        confirmLabel="Remove"
        message={`${removeTarget?.email || ''} loses access to this workspace immediately. The server refuses to remove the last owner.`}
      />
    </div>
  );
}

/* ---------------------------------------------------------------- queries */

function QueryView() {
  const navigate = useNavigate();
  const { active, completed, startSearch } = useSearchStore();
  const [mode, setMode] = useState('fts');
  const [q, setQ] = useState('');
  const [nl, setNl] = useState('');
  const [results, setResults] = useState(null);
  const [page, setPage] = useState(null);
  const [running, setRunning] = useState(false);
  const [suggesting, setSuggesting] = useState(false);
  const [error, setError] = useState(null);

  const canRun = mode === 'llm' ? nl.trim().length > 0 : q.trim().length > 0;

  const run = async (e) => {
    e?.preventDefault();
    setError(null);
    if (mode === 'llm') {
      setRunning(true);
      try { await startSearch(nl.trim()); }
      catch (err) { setError(err); }
      finally { setRunning(false); }
      return;
    }
    setRunning(true);
    try {
      const r = await api.get('/api/intel/items', { query: { q: q.trim(), limit: 50 } });
      setResults(Array.isArray(r?.data) ? r.data : []);
      setPage(r?.page || null);
    } catch (err) { setError(err); setResults(null); }
    finally { setRunning(false); }
  };

  const suggest = async () => {
    setSuggesting(true); setError(null);
    try {
      const r = await api.get('/api/search/suggest', { query: { q: nl.trim() } });
      const first = (r?.suggestions || []).find((s) => String(s).trim());
      if (first) setQ(first); else setError(new Error('No FTS suggestion returned'));
    } catch (err) { setError(err); }
    finally { setSuggesting(false); }
  };

  const saveAsAlert = () => {
    const p = new URLSearchParams({ new: '1', mode });
    if (q.trim()) p.set('q', q.trim());
    if (mode === 'llm' && nl.trim()) p.set('nl_query', nl.trim());
    navigate(`/console/alerts?${p.toString()}`);
  };

  return (
    <div className="space-y-4">
      <Section label="Query">
        <form onSubmit={run} className="space-y-2">
          <Segmented value={mode} onChange={setMode} options={[{ value: 'fts', label: 'FTS' }, { value: 'llm', label: 'LLM pipeline' }]} />
          {mode === 'llm' && (
            <div className="flex gap-2">
              <Input placeholder="Natural-language query (e.g. phishing targeting JP banks)" value={nl} onChange={(e) => setNl(e.target.value)} />
              <Button busy={suggesting} disabled={!nl.trim()} onClick={suggest} type="button"><LuSparkles size={13} /> Suggest FTS</Button>
            </div>
          )}
          <Input mono placeholder="FTS query (e.g. phishing AND tld:.jp)" value={q} onChange={(e) => setQ(e.target.value)} />
          <div className="flex flex-wrap gap-2">
            <Button type="submit" variant="primary" busy={running} disabled={!canRun}><LuSearch size={13} /> Run</Button>
            <Button type="button" disabled={!canRun} onClick={saveAsAlert}>Save as alert</Button>
          </div>
        </form>
        <div className="text-[11px] text-osint-muted mt-2">
          {mode === 'llm'
            ? 'Runs the agentic search pipeline on your natural-language query, ingesting new intel. “Suggest FTS” drafts the FTS string the LLM would use — edit it, then save as an alert.'
            : 'Searches existing intel via full-text search. Save it as an alert to get pinged on new matches. “Save as alert” opens the alert editor pre-filled with this query.'}
        </div>
      </Section>

      {error && <ErrorNotice error={error} title="Query failed" />}

      {mode === 'fts' && results && (
        <Section label={`Results · ${results.length}`} right={<BoundNote shown={results.length} total={page?.total ?? null} noun="matches" />} padded={false}>
          {results.length === 0 ? <EmptyState title="No intel matched this FTS query." /> : (
            <ul className="divide-y divide-osint-border">
              {results.map((it) => (
                <li key={it.uid}>
                  <button type="button" onClick={() => navigate(`/intel/items/${encodeURIComponent(it.uid)}`)} className="w-full text-left px-3 py-2 hover:bg-white/5">
                    <div className="text-sm text-osint-text line-clamp-2">{it.title || it.summary || '(untitled)'}</div>
                    <div className="flex gap-2 text-[10px] font-mono mt-0.5">
                      <span className="text-accent">{it.source_id}</span>
                      {it.published_at && <span className="text-osint-muted">{it.published_at}</span>}
                    </div>
                    {(it.summary || it.body) && <div className="text-[11px] text-osint-muted line-clamp-2 mt-0.5">{it.summary || it.body}</div>}
                  </button>
                </li>
              ))}
            </ul>
          )}
          {page?.next_cursor && <div className="px-3 py-2 text-[11px] text-accent font-mono">More matches exist beyond the first {results.length} — open Intel › Search to page through them.</div>}
        </Section>
      )}

      {mode === 'llm' && (
        <Section label="Pipeline runs" padded={false}>
          {active.length === 0 && completed.length === 0 ? <EmptyState title="No runs yet." /> : (
            <ul className="divide-y divide-osint-border">
              {active.map((r) => (
                <li key={r.request_id} className="flex items-center gap-3 px-3 py-2">
                  <Spinner size={12} />
                  <span className="min-w-0 flex-1">
                    <span className="block text-sm text-osint-text truncate">{r.query}</span>
                    <span className="block text-[10px] text-osint-muted font-mono">{r.snapshot?.phase || 'queued'}</span>
                  </span>
                </li>
              ))}
              {completed.map((r) => {
                const n = r.snapshot?.results?.services?.length;
                return (
                  <li key={r.request_id}>
                    <button type="button" onClick={() => navigate('/search')} className="w-full flex items-center gap-3 px-3 py-2 text-left hover:bg-white/5">
                      <span className="text-neon-green">✓</span>
                      <span className="min-w-0 flex-1">
                        <span className="block text-sm text-osint-text truncate">{r.query}</span>
                        <span className="block text-[10px] text-osint-muted font-mono">completed{n ? ` · ${n} service${n === 1 ? '' : 's'}` : ''}</span>
                      </span>
                    </button>
                  </li>
                );
              })}
            </ul>
          )}
        </Section>
      )}
    </div>
  );
}

/* ----------------------------------------------------------------- switch */

function SwitchView() {
  const auth = useAuth();
  const [busy, setBusy] = useState(null);
  const ms = auth.memberships;
  return (
    <Section label="Active workspace" padded={false}>
      {ms.length === 0 ? (
        <EmptyState title={auth.tenantName ? `Active workspace: ${auth.tenantName}` : 'No memberships were returned by the server.'} />
      ) : (
        <ul className="divide-y divide-osint-border">
          {ms.map((m) => {
            const active = m.id === auth.tenantId;
            return (
              <li key={m.id}>
                <button
                  type="button"
                  disabled={busy != null}
                  onClick={async () => { setBusy(m.id); await auth.switchTenant(m.id); setBusy(null); toast(`Switched to ${m.name}`); }}
                  className={cx('w-full flex items-center justify-between gap-3 px-3 py-2 text-left', active ? 'text-accent' : 'text-osint-text hover:bg-white/5')}
                >
                  <span className="flex items-center gap-2 min-w-0">
                    <LuUsers size={14} />
                    <span className="min-w-0">
                      <span className="block text-sm truncate">{m.name}</span>
                      <span className="block text-[10px] text-osint-muted font-mono">{m.slug} · {m.role || 'member'} · {m.plan || 'free'}</span>
                    </span>
                  </span>
                  {busy === m.id ? <Spinner size={12} /> : active ? <Pill tone="accent">ACTIVE</Pill> : null}
                </button>
              </li>
            );
          })}
        </ul>
      )}
    </Section>
  );
}
