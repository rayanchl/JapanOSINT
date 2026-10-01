import React, { useEffect, useState } from 'react';
import { Link } from 'react-router-dom';
import { LuRefreshCw, LuShieldPlus, LuTrash2, LuShieldCheck, LuShieldAlert, LuLock } from 'react-icons/lu';
import { api, errorMessage, ApiError } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { relativeTime, fmtAbs } from '../../utils/time.js';
import {
  Page, Section, Card, Pill, Button, Input, Field, Select, Sheet, ConfirmDialog, ErrorNotice, EmptyState,
  LoadingState, BoundNote, CopyButton, KV, toast, cx,
} from '../ui/kit.jsx';

/**
 * Breach exposure monitors — port of iOS `BreachMonitorsView`.
 *   GET  /api/breach-monitors?limit&cursor → {data:[{id,kind,label,rule_id,value_domain,created_at,last_checked_at,hash_prefix,delivers}], page}
 *   POST /api/breach-monitors {kind: email|domain|username|phone, value, label?}
 *        403 {error:"domain_not_verified", remedy} · 409 {error:"already_monitored", id}
 *   GET  /api/breach-monitors/:id/hits → {data:[{uid,type,breach_id,has_secret,count,first_seen,breach:{…}}], meta:{count}}
 *   DELETE /api/breach-monitors/:id
 *   GET/POST /api/breach-monitors/domains {domain} → TXT token · POST /domains/verify {domain}
 * Only a hash of the identifier is stored; the server never returns the value.
 */
const KINDS = [
  { value: 'email', label: 'Email address' },
  { value: 'domain', label: 'Domain' },
  { value: 'username', label: 'Username' },
  { value: 'phone', label: 'Phone number' },
];

export default function BreachMonitorsPage() {
  const [rows, setRows] = useState([]);
  const [page, setPage] = useState(null);
  const [error, setError] = useState(null);
  const [loading, setLoading] = useState(true);
  const [adding, setAdding] = useState(false);
  const [confirmDel, setConfirmDel] = useState(null);
  const [open, setOpen] = useState(null);
  const [busy, setBusy] = useState(null);

  const load = async (cursor) => {
    setLoading(true); setError(null);
    try {
      const j = await api.get('/api/breach-monitors', { query: { limit: 100, cursor } });
      const d = Array.isArray(j?.data) ? j.data : [];
      setRows((xs) => (cursor ? [...xs, ...d] : d));
      setPage(j?.page || null);
    } catch (e) { setError(e); }
    finally { setLoading(false); }
  };
  useEffect(() => { load(); }, []);

  const del = async () => {
    const m = confirmDel; setConfirmDel(null); setBusy(m.id);
    try {
      await api.del(`/api/breach-monitors/${encodeURIComponent(m.id)}`);
      toast('Monitor deleted', { tone: 'accent' });
      setRows((xs) => xs.filter((x) => x.id !== m.id));
      if (open?.id === m.id) setOpen(null);
    } catch (e) { toast(errorMessage(e), { tone: 'danger', ttl: 6000 }); }
    finally { setBusy(null); }
  };

  return (
    <Page
      title="Breach monitors"
      subtitle="Watch an address, domain, username or phone number against the breach corpus. Only a hash is stored."
      actions={(
        <>
          <Button onClick={() => load()} title="Reload"><LuRefreshCw size={13} /></Button>
          <Button variant="primary" onClick={() => setAdding(true)}><LuShieldPlus size={13} /> Add monitor</Button>
        </>
      )}
    >
      {error && <ErrorNotice error={error} title="Could not load monitors" onRetry={() => load()} />}
      {loading && rows.length === 0 && !error && <LoadingState label="Loading monitors…" />}
      {!loading && !error && rows.length === 0 && (
        <EmptyState title="No monitors yet" action={<Button variant="primary" onClick={() => setAdding(true)}>Watch an identifier</Button>}>
          Add an email, domain, username or phone. New breach records that contain it raise an inbox event; the identifier itself is stored as a hash.
        </EmptyState>
      )}

      {rows.length > 0 && (
        <div className="space-y-1.5">
          {rows.map((m) => (
            <Card key={m.id} padded={false}>
              <button type="button" onClick={() => setOpen(open?.id === m.id ? null : m)} className="w-full text-left p-3 flex items-start gap-3 flex-wrap hover:bg-white/[0.02]">
                <div className="min-w-0 flex-1">
                  <div className="flex items-center gap-2 flex-wrap">
                    <Pill tone="cyan">{m.kind}</Pill>
                    <span className="text-sm font-medium text-osint-text truncate">{m.label || (m.value_domain ? `@${m.value_domain}` : `hash ${m.hash_prefix || '…'}`)}</span>
                    {m.delivers ? <Pill tone="success" title={m.rule_id}>delivers</Pill> : <Pill tone="neutral" title="No channel set — matches land in the inbox only">inbox only</Pill>}
                  </div>
                  <div className="text-[11px] text-osint-muted font-mono mt-0.5 flex items-center gap-1">
                    <LuLock size={10} /> only a hash is stored{m.hash_prefix ? ` · ${m.hash_prefix}` : ''}{m.value_domain ? ` · ${m.value_domain}` : ''}
                  </div>
                  <div className="text-[11px] text-osint-muted mt-0.5" title={fmtAbs(m.last_checked_at)}>
                    WATCHING since {relativeTime(m.created_at)} · last checked {m.last_checked_at ? relativeTime(m.last_checked_at) : 'never'}
                  </div>
                </div>
                <div className="flex items-center gap-1" onClick={(e) => e.stopPropagation()}>
                  <CopyButton text={m.id} label="Copy id" />
                  <Button size="sm" variant="ghost" busy={busy === m.id} onClick={() => setConfirmDel(m)} title="Delete"><LuTrash2 size={12} /></Button>
                </div>
              </button>
              {open?.id === m.id && <MonitorHits monitor={m} />}
            </Card>
          ))}
          <div className="flex items-center justify-between">
            <BoundNote shown={rows.length} total={page?.next_cursor ? rows.length + 1 : rows.length} noun="monitors" />
            {page?.next_cursor && <Button size="sm" busy={loading} onClick={() => load(page.next_cursor)}>Load more</Button>}
          </div>
        </div>
      )}

      <DomainsSection />

      <AddMonitorSheet open={adding} onClose={() => setAdding(false)} onCreated={(row) => { setAdding(false); setRows((xs) => [row, ...xs]); }} />

      <ConfirmDialog
        open={confirmDel != null}
        onClose={() => setConfirmDel(null)}
        onConfirm={del}
        title="Delete this monitor?"
        confirmLabel="Delete"
        message="Stops watching this identifier. Inbox events already raised are kept."
      />
    </Page>
  );
}

function MonitorHits({ monitor }) {
  const { data, error, loading, reload } = useApi(`/api/breach-monitors/${encodeURIComponent(monitor.id)}/hits`);
  const hits = Array.isArray(data?.data) ? data.data : [];
  return (
    <div className="border-t border-osint-border p-3 space-y-2 bg-osint-bg/40">
      <div className="flex items-center justify-between">
        <div className="font-mono text-[10px] uppercase tracking-[0.12em] text-osint-muted">{data?.meta?.count != null ? `${data.meta.count} match${data.meta.count === 1 ? '' : 'es'}` : 'Matches'}</div>
        <Button size="sm" onClick={() => reload()} busy={loading} title="Reload matches"><LuRefreshCw size={11} /> Check again</Button>
      </div>
      {error && <ErrorNotice error={error} title="Could not load matches" onRetry={reload} />}
      {loading && !data && <LoadingState label="Loading matches…" />}
      {!loading && !error && hits.length === 0 && <div className="text-xs text-osint-muted">No matches. The identifier has not been seen in the breach corpus this workspace has loaded.</div>}
      {hits.length > 0 && (
        <ul className="divide-y divide-osint-border">
          {hits.map((h) => {
            const b = h.breach || {};
            return (
              <li key={h.uid} className="py-2 text-xs space-y-1">
                <div className="flex items-center gap-2 flex-wrap">
                  <Link to={`/intel/items/${encodeURIComponent(h.uid)}`} className="text-osint-text font-medium hover:text-accent">{b.title || b.name || h.breach_id || h.uid}</Link>
                  {b.domain && <span className="font-mono text-osint-muted">{b.domain}</span>}
                  {b.breach_date && <span className="font-mono text-osint-muted">{b.breach_date}</span>}
                  {b.pwn_count != null && <span className="text-osint-muted">· {Number(b.pwn_count).toLocaleString()} accounts</span>}
                  {h.count != null && h.count > 1 && <Pill>{h.count} records</Pill>}
                  {b.verified && <Pill tone="success">verified</Pill>}
                  {b.sensitive && <Pill tone="warning">sensitive</Pill>}
                  {h.has_secret && <Pill tone="danger" title="A secret is stored for this record and stays redacted."><LuLock size={9} /> secret redacted</Pill>}
                </div>
                {Array.isArray(b.data_classes) && b.data_classes.length > 0 && (
                  <div className="flex flex-wrap gap-1">{b.data_classes.map((c) => <Pill key={c} tone="neutral" mono={false}>{c}</Pill>)}</div>
                )}
                {h.first_seen && <div className="text-[11px] text-osint-muted font-mono">first seen {relativeTime(h.first_seen)}</div>}
              </li>
            );
          })}
        </ul>
      )}
    </div>
  );
}

/** Domain ownership proofs — prerequisite for monitoring identifiers under a domain. */
function DomainsSection() {
  const { data, error, loading, reload } = useApi('/api/breach-monitors/domains');
  const domains = Array.isArray(data?.data) ? data.data : [];
  const [domain, setDomain] = useState('');
  const [busy, setBusy] = useState(null);
  const [issued, setIssued] = useState(null);
  const [actErr, setActErr] = useState(null);

  const claim = async () => {
    setBusy('claim'); setActErr(null); setIssued(null);
    try {
      const r = await api.post('/api/breach-monitors/domains', { domain: domain.trim() });
      setIssued(r);
      setDomain('');
      await reload({ silent: true });
    } catch (e) { setActErr(e); }
    finally { setBusy(null); }
  };
  const verify = async (d) => {
    setBusy(d); setActErr(null);
    try {
      await api.post('/api/breach-monitors/domains/verify', { domain: d });
      toast(`${d} verified`, { tone: 'accent' });
      await reload({ silent: true });
    } catch (e) {
      const body = e instanceof ApiError ? e.body : null;
      setActErr(body?.detail ? new Error(`${d}: ${body.detail}`) : e);
      await reload({ silent: true });
    } finally { setBusy(null); }
  };

  return (
    <Section label="Verified domains" right={<Button size="sm" variant="ghost" onClick={() => reload()}><LuRefreshCw size={11} /></Button>}>
      <div className="space-y-2 text-xs">
        <div className="text-osint-muted">Monitoring an email or a domain requires proving control of the domain: publish the TXT record the server issues, then verify.</div>
        {error && <ErrorNotice error={error} title="Could not load domains" onRetry={reload} />}
        {loading && !data && <LoadingState label="Loading domains…" />}
        {domains.length > 0 && (
          <ul className="divide-y divide-osint-border rounded-md border border-osint-border">
            {domains.map((d) => (
              <li key={d.domain} className="px-2 py-1.5 flex items-center gap-2 flex-wrap">
                {d.verified ? <LuShieldCheck size={13} className="text-neon-green" /> : <LuShieldAlert size={13} className="text-accent" />}
                <span className="font-mono text-osint-text">{d.domain}</span>
                {d.verified ? <Pill tone="success">verified {relativeTime(d.verified_at)}</Pill> : <Pill tone="warning">pending</Pill>}
                {d.last_error && !d.verified && <span className="text-neon-red truncate max-w-[40%]" title={d.last_error}>{d.last_error}</span>}
                <span className="flex-1" />
                {!d.verified && d.txt_record && <CopyButton text={d.txt_record} label="Copy TXT" />}
                {!d.verified && <Button size="sm" busy={busy === d.domain} onClick={() => verify(d.domain)}>Verify now</Button>}
              </li>
            ))}
          </ul>
        )}
        <div className="flex gap-2">
          <Input mono value={domain} onChange={(e) => setDomain(e.target.value)} placeholder="example.co.jp" onKeyDown={(e) => { if (e.key === 'Enter') claim(); }} />
          <Button busy={busy === 'claim'} disabled={!domain.trim()} onClick={claim}>Claim domain</Button>
        </div>
        {issued && (
          <div className="rounded-md border border-accent/40 bg-accent/10 p-2 space-y-1">
            <div className="text-accent font-medium">Publish this TXT record for {issued.domain}, then verify.</div>
            <div className="font-mono break-all text-osint-text">{issued.txt_record}</div>
            {issued.instructions && <div className="text-osint-muted">{issued.instructions}</div>}
            <CopyButton text={issued.txt_record} label="Copy TXT record" />
          </div>
        )}
        {actErr && <ErrorNotice error={actErr} title="Domain action failed" />}
      </div>
    </Section>
  );
}

function AddMonitorSheet({ open, onClose, onCreated }) {
  const [kind, setKind] = useState('email');
  const [value, setValue] = useState('');
  const [label, setLabel] = useState('');
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState(null);
  const [remedy, setRemedy] = useState(null);
  useEffect(() => { if (open) { setError(null); setRemedy(null); } }, [open]);

  const save = async () => {
    setError(null); setRemedy(null);
    if (!value.trim()) { setError(new Error('Enter the identifier to watch.')); return; }
    setSaving(true);
    try {
      const body = { kind, value: value.trim() };
      if (label.trim()) body.label = label.trim().slice(0, 200);
      const r = await api.post('/api/breach-monitors', body);
      toast('Monitor added', { tone: 'accent' });
      setValue(''); setLabel('');
      onCreated(r?.data ?? r);
    } catch (e) {
      const b = e instanceof ApiError ? e.body : null;
      if (b?.error === 'domain_not_verified') setRemedy(b.remedy || `Verify ownership of ${b.domain || 'the domain'} first (see Verified domains below).`);
      else if (b?.error === 'already_monitored') setRemedy(`This identifier is already monitored (id ${b.id}).`);
      setError(e);
    } finally { setSaving(false); }
  };

  return (
    <Sheet open={open} onClose={onClose} title="Add monitor" width="max-w-md"
      footer={<><Button onClick={onClose}>Cancel</Button><Button variant="primary" busy={saving} onClick={save}>Watch</Button></>}
    >
      <div className="space-y-3">
        <Field label="Kind">
          <Select className="w-full" value={kind} onChange={(e) => setKind(e.target.value)}>{KINDS.map((k) => <option key={k.value} value={k.value}>{k.label}</option>)}</Select>
        </Field>
        <Field label="Identifier" hint="Hashed before storage; the server never returns it.">
          <Input mono value={value} onChange={(e) => setValue(e.target.value)} placeholder={kind === 'email' ? 'ceo@example.co.jp' : kind === 'domain' ? 'example.co.jp' : kind === 'phone' ? '+81 90 …' : 'handle'} />
        </Field>
        <Field label="Label (optional)"><Input value={label} onChange={(e) => setLabel(e.target.value)} placeholder="e.g. CEO personal address" maxLength={200} /></Field>
        {remedy && <div className="rounded-md border border-accent/40 bg-accent/10 px-3 py-2 text-xs text-accent">{remedy}</div>}
        {error && <ErrorNotice error={error} title="Could not add monitor" />}
        <KV pairs={[['stored', 'SHA-1 hash + 10-char prefix'], ['matches', 'raise an inbox event; add a channel via the rule to deliver']]} />
      </div>
    </Sheet>
  );
}
