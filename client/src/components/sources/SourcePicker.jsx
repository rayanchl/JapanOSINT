import React, { useEffect, useId, useState } from 'react';
import { api, errorMessage } from '../../api/client.js';
import { searchPath } from '../../utils/sourceFormat.js';
import { Input } from '../ui/kit.jsx';

/**
 * A source-id input whose suggestions come from the SERVER's registry search
 * (GET /api/sources/search?q=&limit=), not from a downloaded catalogue.
 *
 * Why (2026-11-07): the Timeline's source dropdown downloaded the whole
 * GET /api/intel/sources catalogue — tens of MB, every registered source —
 * to fill one <select>. The search route answers 20 matches for what has been
 * typed, with its own total, so the picker says how many it is not showing.
 *
 * `value` is the chosen source id ('' = none). Typing an id and leaving the
 * field commits it; a suggestion commits on pick.
 */
export default function SourcePicker({ value, onChange, placeholder = 'source id or name…', limit = 20, hasItems = true, className }) {
  const listId = useId();
  const [draft, setDraft] = useState(value || '');
  const [opts, setOpts] = useState({ rows: [], total: null, error: null });
  useEffect(() => { setDraft(value || ''); }, [value]);

  useEffect(() => {
    const q = draft.trim();
    if (!q || q === value) { setOpts((o) => (o.rows.length ? { rows: [], total: null, error: null } : o)); return undefined; }
    let cancelled = false;
    const t = setTimeout(async () => {
      try {
        const j = await api.get(searchPath({ q, has_items: hasItems }, { limit }));
        if (!cancelled) setOpts({ rows: Array.isArray(j?.data) ? j.data : [], total: Number.isFinite(j?.page?.total) ? j.page.total : null, error: null });
      } catch (e) {
        if (!cancelled) setOpts({ rows: [], total: null, error: e });
      }
    }, 250);
    return () => { cancelled = true; clearTimeout(t); };
  }, [draft, value, limit, hasItems]);

  const commit = (v) => { const id = (v || '').trim(); if (id !== (value || '')) onChange(id); };

  return (
    <span className={className}>
      <Input
        mono
        list={listId}
        placeholder={placeholder}
        value={draft}
        onChange={(e) => {
          setDraft(e.target.value);
          // A pick from the datalist arrives as a change whose value is an exact id.
          if (opts.rows.some((r) => r.id === e.target.value)) commit(e.target.value);
        }}
        onBlur={() => commit(draft)}
        onKeyDown={(e) => { if (e.key === 'Enter') { e.preventDefault(); commit(draft); } }}
      />
      <datalist id={listId}>
        {opts.rows.map((r) => <option key={r.id} value={r.id}>{r.name || r.id}</option>)}
      </datalist>
      {opts.total != null && opts.total > opts.rows.length && (
        <span className="block text-[10px] text-osint-muted font-mono">{opts.rows.length} of {opts.total.toLocaleString()} matches — type more to narrow</span>
      )}
      {opts.error && <span className="block text-[10px] text-neon-red font-mono">source search failed — {errorMessage(opts.error)}</span>}
    </span>
  );
}
