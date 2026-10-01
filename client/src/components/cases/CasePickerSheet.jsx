import React, { useEffect, useState } from 'react';
import { LuFolderPlus } from 'react-icons/lu';
import { api } from '../../api/client.js';
import { useApi } from '../../hooks/useApi.js';
import { Sheet, Button, Input, ErrorNotice, LoadingState, EmptyState, cx, toast } from '../ui/kit.jsx';

/**
 * "Pin to case" — the iOS `CasePickerSheet`. Lists open cases, lets the user
 * create one inline, and POSTs the reference:
 *   POST /api/cases/:id/items  { ref_type, ref_id, label? }
 * ref_type is the server's shared vocabulary:
 *   intel_item | entity | breach_item | feature | camera | search_run | attachment
 */
export function CasePickerSheet({ open, onClose, refType, refId, label, onPinned }) {
  const { data, error, loading, reload } = useApi(open ? '/api/cases?limit=100&status=open' : null, { deps: [open] });
  const [busy, setBusy] = useState(null);
  const [pinError, setPinError] = useState(null);
  const [newName, setNewName] = useState('');
  const [creating, setCreating] = useState(false);
  const cases = Array.isArray(data?.data) ? data.data : Array.isArray(data) ? data : [];

  useEffect(() => { if (open) { setPinError(null); setNewName(''); } }, [open]);

  const pin = async (caseRow) => {
    setBusy(caseRow.id); setPinError(null);
    try {
      const body = { ref_type: refType, ref_id: String(refId) };
      if (label) body.label = String(label).slice(0, 200);
      const r = await api.post(`/api/cases/${encodeURIComponent(caseRow.id)}/items`, body);
      toast(`Pinned to ${caseRow.name || 'case'}`, { tone: 'accent' });
      onPinned?.(caseRow, r?.data ?? r);
      onClose?.();
    } catch (e) {
      setPinError(e);
    } finally { setBusy(null); }
  };

  const create = async () => {
    const name = newName.trim();
    if (!name) return;
    setCreating(true); setPinError(null);
    try {
      const r = await api.post('/api/cases', { name, priority: 0 });
      const row = r?.data ?? r;
      await reload();
      if (row?.id) await pin(row);
    } catch (e) { setPinError(e); }
    finally { setCreating(false); }
  };

  return (
    <Sheet open={open} onClose={onClose} title="Pin to case" width="max-w-md">
      <div className="space-y-3">
        <div className="text-[11px] text-osint-muted font-mono truncate">{refType} · {String(refId)}</div>
        {loading && <LoadingState label="Loading cases…" />}
        {error && <ErrorNotice error={error} title="Could not list cases" onRetry={reload} />}
        {pinError && <ErrorNotice error={pinError} title="Pin failed" />}
        {!loading && !error && cases.length === 0 && (
          <EmptyState title="No open cases yet.">Create one below and this item becomes its first pinned reference.</EmptyState>
        )}
        {cases.length > 0 && (
          <ul className="divide-y divide-osint-border rounded-md border border-osint-border">
            {cases.map((c) => (
              <li key={c.id}>
                <button
                  type="button"
                  disabled={busy != null}
                  onClick={() => pin(c)}
                  className={cx('w-full flex items-center justify-between gap-3 px-3 py-2 text-left hover:bg-white/5 disabled:opacity-50')}
                >
                  <span className="min-w-0">
                    <span className="block text-sm text-osint-text truncate">{c.name || c.id}</span>
                    <span className="block text-[11px] text-osint-muted font-mono">
                      {c.status || 'open'}{c.item_count != null ? ` · ${c.item_count} items` : ''}{c.updated_at ? ` · ${new Date(c.updated_at).toLocaleDateString('en-GB')}` : ''}
                    </span>
                  </span>
                  <span className="text-xs text-accent">{busy === c.id ? '…' : 'Pin'}</span>
                </button>
              </li>
            ))}
          </ul>
        )}
        <div className="flex gap-2">
          <Input placeholder="New case name…" value={newName} onChange={(e) => setNewName(e.target.value)} onKeyDown={(e) => { if (e.key === 'Enter') create(); }} />
          <Button variant="primary" busy={creating} disabled={!newName.trim()} onClick={create}><LuFolderPlus size={14} /> Create & pin</Button>
        </div>
      </div>
    </Sheet>
  );
}

/** Button + sheet in one, for toolbars: <PinToCaseButton refType="intel_item" refId={uid} label={title} /> */
export default function PinToCaseButton({ refType, refId, label, size = 'sm', className, children }) {
  const [open, setOpen] = useState(false);
  if (refId == null) return null;
  return (
    <>
      <Button size={size} className={className} onClick={() => setOpen(true)} title="Pin to case">
        <LuFolderPlus size={13} /> {children ?? 'Pin to case'}
      </Button>
      <CasePickerSheet open={open} onClose={() => setOpen(false)} refType={refType} refId={refId} label={label} />
    </>
  );
}
