import React, { useEffect, useState } from 'react';
import { api } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { Card, Pill, Button, Input, KV, ErrorNotice } from '../ui/kit.jsx';
import { fmtInterval, fmtCount } from '../../utils/sourceFormat.js';

/* The system-wide cadence policy (GET/PUT /api/sources/cadence): one global
 * interval for every scheduled source, or a floor and a ceiling the declared
 * intervals are clamped into, or nothing. The server states what each choice
 * costs in runs/day over the whole registry — measured, not guessed — so the
 * number is in front of whoever changes it. Entity pivots are never affected. */

export function describePolicy(p) {
  if (!p) return 'not reported';
  if (p.global_interval_sec > 0) return `every scheduled source runs every ${fmtInterval(p.global_interval_sec)}`;
  const parts = [];
  if (p.min_interval_sec > 0) parts.push(`never more often than every ${fmtInterval(p.min_interval_sec)}`);
  if (p.max_interval_sec > 0) parts.push(`never less often than every ${fmtInterval(p.max_interval_sec)}`);
  return parts.length ? parts.join(', ') : 'each source runs at its own declared interval';
}

export default function CadenceCard({ canEdit = false }) {
  const { data, error, loading, reload } = useApi('/api/sources/cadence');
  const [edit, setEdit] = useState(false);
  const [form, setForm] = useState({ global_interval_sec: '', min_interval_sec: '', max_interval_sec: '' });
  const [saving, setSaving] = useState({ busy: false, error: null });
  useEffect(() => {
    const p = data?.policy;
    if (p) setForm({ global_interval_sec: String(p.global_interval_sec || ''), min_interval_sec: String(p.min_interval_sec || ''), max_interval_sec: String(p.max_interval_sec || '') });
  }, [data]);
  if (error) return <ErrorNotice error={error} title="Could not load the cadence policy" onRetry={reload} />;
  if (loading && !data) return null;
  const p = data?.policy; const est = data?.estimate;
  const save = async (e) => {
    e.preventDefault();
    setSaving({ busy: true, error: null });
    const num = (v) => (v === '' ? 0 : Number(v));
    try {
      await api.put('/api/sources/cadence', { global_interval_sec: num(form.global_interval_sec), min_interval_sec: num(form.min_interval_sec), max_interval_sec: num(form.max_interval_sec) });
      setSaving({ busy: false, error: null }); setEdit(false); reload();
    } catch (err) { setSaving({ busy: false, error: err }); }
  };
  return (
    <Card data-testid="cadence-card">
      <div className="flex items-start justify-between gap-3 flex-wrap">
        <div className="min-w-0">
          <div className="text-[10px] uppercase tracking-wider text-osint-muted">cadence policy · system-wide</div>
          <div className="text-sm text-osint-text mt-0.5">{describePolicy(p)} <Pill tone={p?.mode === 'declared' ? 'neutral' : 'accent'} mono={false} className="ml-1">{p?.mode || 'unknown'}</Pill></div>
          <div className="text-[11px] text-osint-muted mt-1">
            {est ? <>{fmtCount(est.scheduled_rows)} scheduled sources · {fmtCount(est.runs_per_day_effective)} runs/day under this policy{est.runs_per_day_effective !== est.runs_per_day_declared ? ` (${fmtCount(est.runs_per_day_declared)} as declared)` : ''} · fastest {fmtInterval(est.fastest_effective_sec)}, slowest {fmtInterval(est.slowest_effective_sec)}</> : 'cost not reported'}
            {' · '}source: {data?.source}{data?.updated_at ? `, set ${data.updated_at}${data.updated_by ? ` by ${data.updated_by}` : ''}` : ''}
          </div>
        </div>
        {canEdit && !edit && <Button size="sm" onClick={() => setEdit(true)}>Change</Button>}
      </div>
      {edit && (
        <form onSubmit={save} className="mt-3 grid grid-cols-1 md:grid-cols-4 gap-2 items-end">
          <label className="text-[11px] text-osint-muted">global interval (s)<Input value={form.global_interval_sec} onChange={(e) => setForm({ ...form, global_interval_sec: e.target.value })} placeholder="0 = off" /></label>
          <label className="text-[11px] text-osint-muted">floor (s)<Input value={form.min_interval_sec} onChange={(e) => setForm({ ...form, min_interval_sec: e.target.value })} placeholder="0 = off" /></label>
          <label className="text-[11px] text-osint-muted">ceiling (s)<Input value={form.max_interval_sec} onChange={(e) => setForm({ ...form, max_interval_sec: e.target.value })} placeholder="0 = off" /></label>
          <div className="flex gap-2"><Button type="submit" disabled={saving.busy}>{saving.busy ? 'Saving…' : 'Save'}</Button><Button type="button" onClick={() => setEdit(false)}>Cancel</Button></div>
          <div className="md:col-span-4 text-[11px] text-osint-muted">A global interval overrides every declared interval (pivots excepted); a floor/ceiling clamps them instead. Takes effect in the running scheduler within one interval. Platform-operator only.</div>
          {saving.error && <div className="md:col-span-4"><ErrorNotice error={saving.error} title="The server refused the policy" /></div>}
        </form>
      )}
    </Card>
  );
}
