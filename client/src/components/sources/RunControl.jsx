import React, { useState } from 'react';
import { LuPlay } from 'react-icons/lu';
import { api, errorMessage } from '../../api/client.js';
import { Button, Input, toast } from '../ui/kit.jsx';
import { useAuthOptional } from '../../auth/AuthContext.jsx';

/* One place that knows how a source is run.
 *
 * A scheduled feed runs with no input. An ENTITY PIVOT answers a question
 * about one entity, so running it without one fetches nothing — the server
 * answers 422 needs_entity, and this client never sends that request in the
 * first place: the Run button stays disabled until an entity is typed, and a
 * pivot run is never reported as "ran: 0 new rows". */

export function isPivotKind(kind) { return kind === 'pivot' || kind === 'scheduled-pivot'; }

/** Who the server lets run a source: platform operator, or workspace owner/admin. */
export function useCanRun() {
  const auth = useAuthOptional();
  return Boolean(auth?.isPlatformAdmin || auth?.canManageWorkspace);
}

/** POST the run. Returns {ok, message, tone}; never throws. */
export async function runSource(id, { kind, entity } = {}) {
  const pivot = isPivotKind(kind);
  const ent = (entity || '').trim();
  if (pivot && !ent) {
    return { ok: false, needsEntity: true, tone: 'danger', message: 'This source is an entity pivot: it answers for one entity and fetches nothing without one. Enter an entity to run it.' };
  }
  try {
    const r = await api.post(`/api/intel/sources/${encodeURIComponent(id)}/run`, pivot ? { entity: ent } : undefined);
    const n = `${r?.ingested ?? '?'} new rows in ${r?.duration_ms ?? '?'} ms`;
    return { ok: true, tone: 'accent', message: pivot ? `ran for "${ent}": ${n}` : `ran: ${n}`, result: r };
  } catch (e) {
    if (e?.status === 422 && (e?.body?.error === 'needs_entity' || e?.detail === 'needs_entity')) {
      return { ok: false, needsEntity: true, tone: 'danger', message: 'The server says this source needs an entity (needs_entity). Enter one and run again; nothing was fetched.' };
    }
    if (e?.status === 409) return { ok: false, tone: 'danger', message: 'a run is already in flight' };
    if (e?.status === 403) return { ok: false, tone: 'danger', message: 'Not allowed: running a source needs the platform-operator role or workspace owner/admin.' };
    return { ok: false, tone: 'danger', message: errorMessage(e) };
  }
}

/**
 * Run control for one source. `source` = {id, kind}. Renders nothing for an
 * internal pod (the server refuses those). For a viewer who may not run
 * sources it renders a read-only note instead of a button that would 403.
 */
export default function RunControl({ source, onDone, compact = false, label }) {
  const canRun = useCanRun();
  const [entity, setEntity] = useState('');
  const [busy, setBusy] = useState(false);
  const [note, setNote] = useState(null);
  if (!source || source.kind === 'internal') return null;
  const pivot = isPivotKind(source.kind);
  const id = source.id;

  if (!canRun) {
    return <span className="text-[10px] text-osint-muted" data-testid="run-readonly">{compact ? 'Run: operator or workspace owner/admin only (read-only for you).' : `${pivot ? 'Running a pivot' : 'Running a source'} needs the operator role or workspace owner/admin (read-only for you).`}</span>;
  }

  const go = async (e) => {
    e?.preventDefault();
    if (busy) return;
    if (pivot && !entity.trim()) { setNote((await runSource(id, { kind: source.kind, entity })).message); return; }
    setBusy(true); setNote(null);
    const r = await runSource(id, { kind: source.kind, entity });
    setBusy(false);
    if (r.needsEntity) setNote(r.message);
    else toast(`${compact ? `${id}: ` : ''}${r.message}`, { tone: r.tone, ttl: 5000 });
    if (r.ok && onDone) onDone(r);
  };

  if (!pivot) {
    return <Button variant="primary" size={compact ? 'sm' : undefined} busy={busy} onClick={go} aria-label={`Run ${id} now`}><LuPlay size={13} /> {label || 'Run now'}</Button>;
  }
  return (
    <form onSubmit={go} className="flex flex-wrap items-center gap-2" aria-label={`Run pivot ${id}`}>
      <Input className={compact ? 'w-[180px]' : 'w-[240px]'} placeholder="entity (name, id, domain…)" aria-label={`Entity for ${id}`} value={entity} onChange={(e) => { setEntity(e.target.value); setNote(null); }} />
      <Button type="submit" variant="primary" size={compact ? 'sm' : undefined} busy={busy} disabled={!entity.trim()} aria-label={`Run pivot ${id}`}><LuPlay size={13} /> Run</Button>
      {note && <span role="alert" className="text-[11px] text-neon-red basis-full">{note}</span>}
    </form>
  );
}
