import React, { useEffect, useState } from 'react';
import { LuRefreshCw, LuRotateCcw, LuSave, LuPlus } from 'react-icons/lu';
import { api, ApiError } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { Section, Pill, Button, Input, Field, ErrorNotice, EmptyState, LoadingState, toast } from '../ui/kit.jsx';

/**
 * Per-source cache TTLs — GET /api/admin/collector-ttls and
 * GET/PUT/DELETE /api/admin/collector-ttls/:key (core/collector_ttl.c).
 * The key is a registered SOURCE id (what the /api/data and source-list
 * readers look up); the TTL is an integer number of milliseconds inside the
 * cache's own clamp, which the server states in `meta` and enforces — an
 * out-of-range value is a 400, never a clamp. Every save and reset is written
 * to the audit chain by the server.
 */

/** null when `raw` is an integer inside [min,max], else the reason. */
export function validateTtl(raw, min, max) {
  if (raw === '' || raw === null || raw === undefined) return 'Enter a number of milliseconds';
  const v = Number(raw);
  if (!Number.isFinite(v) || !Number.isInteger(v)) return 'Enter a whole number of milliseconds';
  if (v < min || v > max) return `Must be between ${min} and ${max} ms`;
  return null;
}

export function humanMs(ms) {
  if (typeof ms !== 'number') return '—';
  if (ms % 3600000 === 0) return `${ms / 3600000} h`;
  if (ms % 60000 === 0) return `${ms / 60000} min`;
  return `${(ms / 1000).toFixed(1)} s`;
}

const forbidden = (e) => e instanceof ApiError && e.status === 403;

function TtlRow({ row, meta, onChanged }) {
  const [draft, setDraft] = useState(String(row.ttl_ms));
  const [busy, setBusy] = useState(null);
  const [err, setErr] = useState(null);
  useEffect(() => { setDraft(String(row.ttl_ms)); }, [row.ttl_ms]);
  const invalid = validateTtl(draft, meta.min_ttl_ms, meta.max_ttl_ms);
  const dirty = !invalid && Number(draft) !== row.ttl_ms;
  const path = `/api/admin/collector-ttls/${encodeURIComponent(row.key)}`;

  const save = async () => {
    setBusy('save'); setErr(null);
    try {
      const r = await api.put(path, { ttl_ms: Number(draft) });
      onChanged(row.key, r?.data);
      toast(`${row.key}: TTL ${humanMs(r?.data?.ttl_ms)}`, { tone: 'success' });
    } catch (e) { setErr(e); } finally { setBusy(null); }
  };
  const reset = async () => {
    setBusy('reset'); setErr(null);
    try {
      await api.del(path);
      onChanged(row.key, null);
      toast(`${row.key}: back to the ${humanMs(meta.default_ttl_ms)} default`, { tone: 'success' });
    } catch (e) { setErr(e); } finally { setBusy(null); }
  };

  return (
    <li className="px-3 py-3 space-y-2" data-testid={`ttl-${row.key}`}>
      <div className="flex items-center gap-2 flex-wrap">
        <span className="font-mono text-xs text-osint-text">{row.key}</span>
        <Pill tone={row.source === 'user' ? 'accent' : 'neutral'}>{row.source}</Pill>
        {row.registered === false && <Pill tone="warning" title="No registered source has this id; the readers still answer it for that id">not registered</Pill>}
        {row.in_range === false && <Pill tone="warning" title="Outside the cache clamp: the cache stores the clamped value, not this one">outside clamp</Pill>}
        <span className="ml-auto text-[10px] font-mono text-osint-muted">{humanMs(row.ttl_ms)}{row.updated_by ? ` · by ${row.updated_by}` : ''}</span>
      </div>
      <div className="flex items-center gap-2 flex-wrap">
        <Input mono aria-label={`${row.key} ttl_ms`} className="w-40" inputMode="numeric" value={draft} onChange={(e) => setDraft(e.target.value)} />
        <span className="text-[11px] text-osint-muted">ms</span>
        <Button size="sm" variant="primary" busy={busy === 'save'} disabled={!dirty || busy !== null || row.registered === false} onClick={save}><LuSave size={12} /> Save</Button>
        <Button size="sm" busy={busy === 'reset'} disabled={busy !== null} onClick={reset} title="Delete the row; the default applies"><LuRotateCcw size={12} /> Reset</Button>
      </div>
      {invalid && draft !== String(row.ttl_ms) && <div className="text-[11px] text-neon-red">{invalid}</div>}
      {err && <ErrorNotice error={err} title={forbidden(err) ? 'The server refused this account (platform-operator allowlist)' : 'Could not save'} />}
    </li>
  );
}

export default function CollectorTtlsTab() {
  const { data, error, loading, reload } = useApi('/api/admin/collector-ttls');
  const [rows, setRows] = useState([]);
  const [newKey, setNewKey] = useState('');
  const [newTtl, setNewTtl] = useState('');
  const [addBusy, setAddBusy] = useState(false);
  const [addErr, setAddErr] = useState(null);
  useEffect(() => { setRows(Array.isArray(data?.data) ? data.data : []); }, [data]);
  const meta = data?.meta || { min_ttl_ms: 60000, max_ttl_ms: 86400000, default_ttl_ms: 900000 };

  const changed = (key, row) => setRows((rs) => (row ? rs.map((r) => (r.key === key ? row : r)) : rs.filter((r) => r.key !== key)));
  const addInvalid = newTtl !== '' ? validateTtl(newTtl, meta.min_ttl_ms, meta.max_ttl_ms) : null;
  const add = async () => {
    setAddBusy(true); setAddErr(null);
    try {
      const r = await api.put(`/api/admin/collector-ttls/${encodeURIComponent(newKey.trim())}`, { ttl_ms: Number(newTtl) });
      if (r?.data) setRows((rs) => [...rs.filter((x) => x.key !== r.data.key), r.data].sort((a, b) => a.key.localeCompare(b.key)));
      setNewKey(''); setNewTtl('');
      toast(`${r?.data?.key}: TTL ${humanMs(r?.data?.ttl_ms)}`, { tone: 'success' });
    } catch (e) { setAddErr(e); } finally { setAddBusy(false); }
  };

  return (
    <div className="space-y-4">
      <Section label="Set a source's cache TTL">
        <div className="text-[11px] text-osint-muted mb-2">
          The key is a registered source id. Allowed: {humanMs(meta.min_ttl_ms)} to {humanMs(meta.max_ttl_ms)} (the cache's own clamp); without a row a source uses {humanMs(meta.default_ttl_ms)}. {meta.applies ? `Applies to ${meta.applies}.` : ''}
        </div>
        <div className="flex items-end gap-2 flex-wrap">
          <Field label="Source id" className="flex-1 min-w-[12rem]"><Input mono aria-label="new source id" value={newKey} onChange={(e) => setNewKey(e.target.value)} /></Field>
          <Field label="TTL (ms)" className="w-40"><Input mono aria-label="new ttl_ms" inputMode="numeric" value={newTtl} onChange={(e) => setNewTtl(e.target.value)} /></Field>
          <Button variant="primary" busy={addBusy} disabled={!newKey.trim() || newTtl === '' || Boolean(addInvalid) || addBusy} onClick={add}><LuPlus size={12} /> Set</Button>
        </div>
        {addInvalid && <div className="text-[11px] text-neon-red mt-1">{addInvalid}</div>}
        {addErr && <div className="mt-2"><ErrorNotice error={addErr} title={forbidden(addErr) ? 'The server refused this account (platform-operator allowlist)' : 'Could not set'} /></div>}
      </Section>
      <Section label={`Stored TTLs · ${rows.length}`} right={<Button size="sm" variant="ghost" onClick={() => reload()}><LuRefreshCw size={12} /></Button>} padded={false}>
        {loading && !data && <LoadingState />}
        {error && <div className="p-2"><ErrorNotice error={error} title={forbidden(error) ? 'The server refused this account (platform-operator allowlist)' : 'Could not load TTLs'} onRetry={reload} /></div>}
        {!error && data && rows.length === 0 && <EmptyState title={`No stored TTLs — every source uses the ${humanMs(meta.default_ttl_ms)} default.`} />}
        {rows.length > 0 && <ul className="divide-y divide-osint-border">{rows.map((r) => <TtlRow key={r.key} row={r} meta={meta} onChanged={changed} />)}</ul>}
      </Section>
    </div>
  );
}
