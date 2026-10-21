import React, { useEffect, useState } from 'react';
import { api } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { Pill, Button, Input, Select, ErrorNotice } from '../ui/kit.jsx';
import { fmtInterval } from '../../utils/sourceFormat.js';

/* How often THIS source runs and what decided it (GET /api/sources/:id/interval):
 * the declared interval, the measured one when the row is adaptive, the
 * system-wide policy applied to that, and — outranking all three — an
 * interval the operator set for this one source. Specific beats general.
 * The server says whether the caller may change it (can_write), so the form
 * is offered only when a PUT would not 403. Entity pivots have no cadence. */

const PRESETS = [
  ['900', 'every 15 min'], ['3600', 'hourly'], ['21600', 'every 6 h'],
  ['86400', 'daily'], ['604800', 'weekly'], ['custom', 'custom (seconds)'],
];

export function describeBasis(d) {
  if (!d) return 'not reported';
  if (!d.scheduled) return 'on demand — an entity pivot has no cadence to set';
  const eff = fmtInterval(d.effective_interval_sec);
  switch (d.effective_basis) {
    case 'override': return `every ${eff} — set for this source`;
    case 'policy':   return `every ${eff} — the system-wide policy (${d.policy})`;
    case 'measured': return `every ${eff} — measured from its newest item`;
    default:         return `every ${eff} — as declared`;
  }
}

export default function IntervalOverrideCard({ id }) {
  const { data, error, loading, reload } = useApi(`/api/sources/${encodeURIComponent(id)}/interval`);
  const [edit, setEdit] = useState(false);
  const [preset, setPreset] = useState('3600');
  const [custom, setCustom] = useState('');
  const [saving, setSaving] = useState({ busy: false, error: null });
  useEffect(() => {
    const ov = data?.override;
    if (ov && ov.interval_sec) {
      const hit = PRESETS.find(([v]) => v === String(ov.interval_sec));
      setPreset(hit ? hit[0] : 'custom'); setCustom(String(ov.interval_sec));
    }
  }, [data]);
  if (error) return <ErrorNotice error={error} title="Could not load this source's interval" onRetry={reload} />;
  if (loading && !data) return null;
  const d = data; const ov = d?.override;
  const seconds = preset === 'custom' ? Number(custom) : Number(preset);
  const call = async (fn) => {
    setSaving({ busy: true, error: null });
    try { await fn(); setSaving({ busy: false, error: null }); setEdit(false); reload(); }
    catch (err) { setSaving({ busy: false, error: err }); }
  };
  const save = (e) => { e.preventDefault(); call(() => api.put(`/api/sources/${encodeURIComponent(id)}/interval`, { interval_sec: seconds, enabled: true })); };
  const toggle = () => call(() => api.put(`/api/sources/${encodeURIComponent(id)}/interval`, { interval_sec: ov.interval_sec, enabled: !ov.enabled }));
  const clear = () => call(() => api.del(`/api/sources/${encodeURIComponent(id)}/interval`));
  return (
    <div data-testid="interval-override" className="mt-3 pt-3 border-t border-osint-border/60">
      <div className="flex items-start justify-between gap-3 flex-wrap">
        <div className="min-w-0">
          <div className="text-[10px] uppercase tracking-wider text-osint-muted">cadence for this source</div>
          <div className="text-sm text-osint-text mt-0.5">{describeBasis(d)} <Pill tone={d?.effective_basis === 'override' ? 'accent' : 'neutral'} mono={false} className="ml-1">{d?.effective_basis || 'unknown'}</Pill></div>
          {ov && (
            <div className="text-[11px] text-osint-muted mt-1">
              override: every {fmtInterval(ov.interval_sec)}, {ov.enabled ? 'enabled' : 'disabled (number kept)'}{ov.updated_at ? `, set ${ov.updated_at}${ov.updated_by ? ` by ${ov.updated_by}` : ''}` : ''}
            </div>
          )}
          {d?.scheduled && !ov && <div className="text-[11px] text-osint-muted mt-1">no per-source interval set; the policy decides (declared {fmtInterval(d.declared_interval_sec)}{d.measured_interval_sec ? `, measured ${fmtInterval(d.measured_interval_sec)}` : ''}, policy {fmtInterval(d.policy_interval_sec)})</div>}
        </div>
        {d?.scheduled && d?.can_write && !edit && (
          <div className="flex gap-2">
            <Button size="sm" onClick={() => setEdit(true)}>{ov ? 'Change' : 'Set'}</Button>
            {ov && <Button size="sm" busy={saving.busy} onClick={toggle}>{ov.enabled ? 'Disable' : 'Enable'}</Button>}
            {ov && <Button size="sm" variant="danger" busy={saving.busy} onClick={clear}>Clear</Button>}
          </div>
        )}
      </div>
      {edit && (
        <form onSubmit={save} className="mt-3 grid grid-cols-1 md:grid-cols-4 gap-2 items-end">
          <label className="text-[11px] text-osint-muted">interval
            <Select value={preset} onChange={(e) => setPreset(e.target.value)}>
              {PRESETS.map(([v, l]) => <option key={v} value={v}>{l}</option>)}
            </Select>
          </label>
          {preset === 'custom' && <label className="text-[11px] text-osint-muted">seconds<Input value={custom} onChange={(e) => setCustom(e.target.value)} placeholder={`${d.min_interval_sec}–${d.max_interval_sec}`} /></label>}
          <div className="flex gap-2">
            <Button type="submit" variant="primary" size="sm" busy={saving.busy} disabled={!(seconds >= (d.min_interval_sec || 10))}>Save</Button>
            <Button size="sm" onClick={() => { setEdit(false); setSaving({ busy: false, error: null }); }}>Cancel</Button>
          </div>
          <div className="text-[11px] text-osint-muted md:col-span-4">An interval set here outranks the system-wide policy for this source only; it runs every {fmtInterval(seconds || 0)}. Disabling keeps the number; clearing hands the source back to the policy.</div>
          {saving.error && <div className="md:col-span-4"><ErrorNotice error={saving.error} title="Not saved" /></div>}
        </form>
      )}
    </div>
  );
}
