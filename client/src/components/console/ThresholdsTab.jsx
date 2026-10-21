import React, { useEffect, useState } from 'react';
import { LuRefreshCw, LuRotateCcw, LuSave } from 'react-icons/lu';
import { api, ApiError } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { Section, Pill, Button, Input, ErrorNotice, EmptyState, LoadingState, toast } from '../ui/kit.jsx';

/**
 * Embedding-similarity thresholds — GET /api/admin/thresholds and
 * PUT /api/admin/thresholds/:key (core/thresholds.c). The server resolves
 * each key as runtime override (db) > environment variable (env) > compiled
 * default, and a change applies on its next read: no restart. Every save and
 * reset is written to the audit chain by the server.
 */

const SOURCE_TONE = { db: 'accent', env: 'warning', default: 'neutral' };
const SOURCE_LABEL = { db: 'runtime override', env: 'environment', default: 'default' };

/** Slider step for a range: 0.01 for cosine ranges, finer for narrow ones. */
export function stepFor(min, max) {
  const span = Number(max) - Number(min);
  return span > 0 && span <= 0.2 ? 0.001 : 0.01;
}

/** null when `raw` is a number inside [min,max], else the reason it is not. */
export function validateThreshold(raw, min, max) {
  if (raw === '' || raw === null || raw === undefined) return 'Enter a number';
  const v = Number(raw);
  if (!Number.isFinite(v)) return 'Enter a number';
  if (v < min || v > max) return `Must be between ${min} and ${max}`;
  return null;
}

function fmt(v) {
  return typeof v === 'number' ? String(Number(v.toFixed(6))) : '—';
}

export function ThresholdRow({ t, onSaved }) {
  const [draft, setDraft] = useState(String(t.value));
  const [busy, setBusy] = useState(null);
  const [err, setErr] = useState(null);
  useEffect(() => { setDraft(String(t.value)); }, [t.value]);

  const invalid = validateThreshold(draft, t.min, t.max);
  const dirty = !invalid && Number(draft) !== t.value;

  const send = async (kind, body) => {
    setBusy(kind); setErr(null);
    try {
      const r = await api.put(`/api/admin/thresholds/${encodeURIComponent(t.key)}`, body);
      onSaved(r?.data);
      toast(kind === 'reset' ? `${t.key} reset to ${fmt(r?.data?.value)} (${r?.data?.source})` : `${t.key} set to ${fmt(r?.data?.value)}`, { tone: 'success' });
    } catch (e) { setErr(e); }
    finally { setBusy(null); }
  };

  return (
    <li className="px-3 py-3 space-y-2" data-testid={`threshold-${t.key}`}>
      <div className="flex items-center gap-2 flex-wrap">
        <span className="font-mono text-xs text-osint-text">{t.key}</span>
        <Pill tone={SOURCE_TONE[t.source] || 'neutral'} title={`resolved from: ${t.source}`}>{SOURCE_LABEL[t.source] || t.source}</Pill>
        <span className="text-[10px] font-mono text-osint-muted">{t.metric}</span>
        <span className="ml-auto text-[10px] font-mono text-osint-muted">
          default {fmt(t.default)} · range [{fmt(t.min)}, {fmt(t.max)}] · {t.env}{t.env_value != null ? `=${fmt(t.env_value)}` : ' unset'}
        </span>
      </div>
      <div className="text-[11px] text-osint-muted">{t.description}</div>
      <div className="flex items-center gap-2 flex-wrap">
        <input
          type="range" aria-label={`${t.key} slider`} className="flex-1 min-w-[10rem] accent-accent"
          min={t.min} max={t.max} step={stepFor(t.min, t.max)}
          value={invalid ? t.value : Number(draft)}
          onChange={(e) => setDraft(e.target.value)}
        />
        <Input mono aria-label={`${t.key} value`} className="w-28" inputMode="decimal" value={draft} onChange={(e) => setDraft(e.target.value)} />
        <Button size="sm" variant="primary" busy={busy === 'save'} disabled={!dirty || busy !== null} onClick={() => send('save', { value: Number(draft) })}><LuSave size={12} /> Save</Button>
        <Button size="sm" busy={busy === 'reset'} disabled={t.source !== 'db' || busy !== null} onClick={() => send('reset', { reset: true })} title="Remove the runtime override; the environment variable or the default takes over"><LuRotateCcw size={12} /> Reset</Button>
      </div>
      {invalid && draft !== String(t.value) && <div className="text-[11px] text-neon-red">{invalid}</div>}
      {t.source === 'db' && t.updated_by && <div className="text-[10px] font-mono text-osint-muted">override by {t.updated_by}{t.updated_at ? ` · ${t.updated_at}` : ''}</div>}
      {err && <ErrorNotice error={err} title={err instanceof ApiError && err.status === 403 ? 'The server refused this account (platform-operator allowlist)' : 'Could not save'} />}
    </li>
  );
}

export default function ThresholdsTab() {
  const { data, error, loading, reload } = useApi('/api/admin/thresholds');
  const [rows, setRows] = useState([]);
  useEffect(() => { setRows(Array.isArray(data?.data) ? data.data : []); }, [data]);
  const replace = (row) => { if (row?.key) setRows((rs) => rs.map((r) => (r.key === row.key ? row : r))); };

  return (
    <div className="space-y-4">
      <Section label={`Similarity thresholds · ${rows.length}`} right={<Button size="sm" variant="ghost" onClick={() => reload()}><LuRefreshCw size={12} /></Button>} padded={false}>
        <div className="px-3 pt-2 text-[11px] text-osint-muted">
          Every embedding cut-off the server judges a vector comparison against. Resolution is {data?.meta?.order || 'db > env > default'}; a saved value applies on the next comparison, with no restart, and is recorded in the audit log. None has been measured on the production model — move them while watching the result.
        </div>
        {loading && !data && <LoadingState />}
        {error && <div className="p-2"><ErrorNotice error={error} title={error instanceof ApiError && error.status === 403 ? 'The server refused this account (platform-operator allowlist)' : 'Could not load thresholds'} onRetry={reload} /></div>}
        {!error && data && rows.length === 0 && <EmptyState title="The server registers no thresholds." />}
        {rows.length > 0 && <ul className="divide-y divide-osint-border">{rows.map((t) => <ThresholdRow key={t.key} t={t} onSaved={replace} />)}</ul>}
      </Section>
    </div>
  );
}
