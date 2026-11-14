import React, { useEffect, useState } from 'react';
import { LuSave, LuRefreshCw } from 'react-icons/lu';
import { api, ApiError } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { Section, Button, Input, TextArea, Field, ErrorNotice, LoadingState, Pill, toast } from '../ui/kit.jsx';

/**
 * Report template — GET/PUT /api/reports/template (core/report_template.c).
 * The workspace's case-report branding: the classification banner, a header
 * note, a footer and a logo. Any member reads it; the owner or an admin edits
 * it (the server says which in meta.can_write and refuses anyone else with
 * 403). Each field's byte limit is the one the report renderer keeps, sent by
 * the server in meta.limits, so what is saved is what a report prints. The
 * logo must be https:// and is never fetched by the server — the HTML report's
 * reader loads it. Clearing every field returns the plain, unbranded report.
 */

const FIELDS = [
  { key: 'classification', label: 'Classification banner', hint: 'One line, e.g. CONFIDENTIAL // TLP:AMBER', multiline: false },
  { key: 'header_note', label: 'Header note', hint: 'One line under the banner', multiline: false },
  { key: 'footer_md', label: 'Footer (Markdown)', hint: 'Appended to the report footer as written', multiline: true },
  { key: 'logo_url', label: 'Logo URL', hint: 'https:// only; HTML reports only', multiline: false },
];

const utf8Len = (s) => new TextEncoder().encode(s || '').length;

/** null when `value` is acceptable for field `f` under `limit` bytes. */
export function validateTemplateField(f, value, limit) {
  const v = value || '';
  if (limit != null && utf8Len(v) > limit) return `At most ${limit} bytes (this is ${utf8Len(v)})`;
  // eslint-disable-next-line no-control-regex
  if (!f.multiline && /[\u0000-\u001f\u007f]/.test(v)) return 'One line, no control characters';
  if (f.key === 'logo_url' && v && !/^https:\/\/[^/\s"'<>]/.test(v)) return 'Must be an https:// URL with a host';
  if (f.key === 'logo_url' && /[\s"'<>]/.test(v)) return 'No spaces, quotes or angle brackets';
  return null;
}

export default function ReportTemplateTab() {
  const { data, error, loading, reload } = useApi('/api/reports/template');
  const [draft, setDraft] = useState({});
  const [busy, setBusy] = useState(false);
  const [err, setErr] = useState(null);
  const tpl = data?.data || {};
  const meta = data?.meta || {};
  const canWrite = Boolean(meta.can_write);
  const limits = meta.limits || {};
  useEffect(() => {
    if (!data?.data) return;
    const d = {};
    FIELDS.forEach((f) => { d[f.key] = data.data[f.key] || ''; });
    setDraft(d);
  }, [data]);

  const problems = FIELDS.map((f) => [f.key, validateTemplateField(f, draft[f.key], limits[f.key])]).filter(([, p]) => p);
  const dirty = FIELDS.some((f) => (draft[f.key] || '') !== (tpl[f.key] || ''));

  const save = async () => {
    setBusy(true); setErr(null);
    try {
      const body = {};
      FIELDS.forEach((f) => { body[f.key] = draft[f.key] || ''; });
      const r = await api.put('/api/reports/template', body);
      if (r?.data) {
        const d = {};
        FIELDS.forEach((f) => { d[f.key] = r.data[f.key] || ''; });
        setDraft(d);
      }
      reload({ silent: true });
      toast(r?.data?.present ? 'Report template saved' : 'Report template cleared — reports are unbranded', { tone: 'success' });
    } catch (e) { setErr(e); } finally { setBusy(false); }
  };

  return (
    <Section
      label="Report template"
      right={<div className="flex items-center gap-2"><Pill tone={tpl.present ? 'accent' : 'neutral'}>{tpl.present ? 'branded' : 'unbranded'}</Pill><Button size="sm" variant="ghost" onClick={() => reload()}><LuRefreshCw size={12} /></Button></div>}
    >
      <div className="text-[11px] text-osint-muted mb-3">
        Branding printed on this workspace's case reports. {canWrite ? 'You can edit it (owner or admin).' : `Only the workspace owner or an admin can edit it; your role is ${meta.role || 'unknown'}.`} Clearing every field returns the plain report.
      </div>
      {loading && !data && <LoadingState />}
      {error && <ErrorNotice error={error} title="Could not load the report template" onRetry={reload} />}
      {data && (
        <div className="space-y-3">
          {FIELDS.map((f) => {
            const p = validateTemplateField(f, draft[f.key], limits[f.key]);
            const common = {
              'aria-label': f.label, value: draft[f.key] || '', disabled: !canWrite || busy,
              onChange: (e) => setDraft((d) => ({ ...d, [f.key]: e.target.value })),
            };
            return (
              <Field key={f.key} label={`${f.label}${limits[f.key] != null ? ` · ${utf8Len(draft[f.key])}/${limits[f.key]} bytes` : ''}`} hint={f.hint}>
                {f.multiline ? <TextArea mono {...common} /> : <Input mono={f.key === 'logo_url'} {...common} />}
                {p && <div className="text-[11px] text-neon-red">{p}</div>}
              </Field>
            );
          })}
          {tpl.updated_at && <div className="text-[10px] font-mono text-osint-muted">last saved {tpl.updated_at}</div>}
          {canWrite && (
            <Button variant="primary" busy={busy} disabled={!dirty || problems.length > 0 || busy} onClick={save}><LuSave size={12} /> Save template</Button>
          )}
          {err && <ErrorNotice error={err} title={err instanceof ApiError && err.status === 403 ? 'The server refused: owner or admin role required' : 'Could not save'} />}
        </div>
      )}
    </Section>
  );
}
