import React, { useEffect, useState } from 'react';
import { LuBookmark, LuLink } from 'react-icons/lu';
import { api } from '../../api/client.js';
import { Sheet, Button, Input, Field, ErrorNotice, Pill, CopyButton, LoadingState, toast } from '../ui/kit.jsx';
import SaveStarButton from '../saved/SaveStarButton.jsx';
import PinToCaseButton from '../cases/CasePickerSheet.jsx';

/**
 * Save-search sheet (SaveSearchSheet on iOS). A saved search is a private
 * bookmark: POST /api/saved-searches { name?, kind, params } — `kind` is
 * fixed once saved, and only `intel` searches can later become alert rules.
 */
export function SaveSearchSheet({ open, onClose, query, kind = 'osint' }) {
  const [name, setName] = useState('');
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState(null);
  useEffect(() => { if (open) { setName(query || ''); setError(null); } }, [open, query]);
  const save = async () => {
    setBusy(true); setError(null);
    try {
      const body = { kind, params: { q: query } };
      if (name.trim()) body.name = name.trim();
      const r = await api.post('/api/saved-searches', body);
      toast(`Saved “${r?.data?.name || query}”`, { tone: 'accent' });
      onClose?.();
    } catch (e) { setError(e); }
    finally { setBusy(false); }
  };
  return (
    <Sheet open={open} onClose={onClose} title="Save search" width="max-w-md"
      footer={<><Button onClick={onClose}>Cancel</Button><Button variant="primary" busy={busy} onClick={save}>Save</Button></>}>
      <div className="space-y-3">
        <Field label="Name" hint="Optional. An unnamed saved search still works — it just shows its query instead of a title.">
          <Input value={name} onChange={(e) => setName(e.target.value)} placeholder="Optional" />
        </Field>
        <div className="flex items-center gap-2 text-xs">
          <Pill tone="accent">{kind.toUpperCase()}</Pill>
          <span className="font-mono text-osint-text truncate">{query}</span>
        </div>
        <div className="text-[11px] text-osint-muted">
          {kind === 'intel'
            ? 'Intel searches can later be turned into an alert rule from Console → Saved searches.'
            : 'Only intel searches can later become alert rules — this kind cannot. The kind is fixed once saved.'}
        </div>
        {error && <ErrorNotice error={error} title="Save failed" />}
      </div>
    </Sheet>
  );
}

/**
 * Share-view sheet (PermalinkShareSheet on iOS). `/api/permalink` is a
 * stateless codec: the token carries VIEW STATE ONLY and is not a grant —
 * whoever opens the link still signs in and the server re-checks tenancy.
 */
export function SearchShareSheet({ open, onClose, query, requestId }) {
  const [state, setState] = useState({ minting: false, link: null, error: null });
  useEffect(() => {
    if (!open) return undefined;
    let alive = true;
    setState({ minting: true, link: null, error: null });
    const params = { q: query };
    if (requestId) params.request_id = requestId;
    api.post('/api/permalink', { kind: 'osint', params })
      .then((r) => {
        if (!alive) return;
        const token = r?.data?.token;
        if (!token) throw new Error('the server returned no token');
        setState({ minting: false, link: `${window.location.origin}/search?t=${encodeURIComponent(token)}`, error: null });
      })
      .catch((e) => { if (alive) setState({ minting: false, link: null, error: e }); });
    return () => { alive = false; };
  }, [open, query, requestId]);
  return (
    <Sheet open={open} onClose={onClose} title="Share view" width="max-w-md" footer={<Button onClick={onClose}>Close</Button>}>
      <div className="space-y-3">
        <div className="text-[11px] text-osint-muted">Reopens the search for <span className="font-mono text-osint-text">{query}</span>.</div>
        {state.minting && <LoadingState label="Minting link…" />}
        {state.error && <ErrorNotice error={state.error} title="Could not create the link" />}
        {state.link && (
          <div className="space-y-2">
            <div className="font-mono text-xs text-osint-text break-all rounded-md border border-osint-border bg-osint-bg p-2 select-all">{state.link}</div>
            <div className="flex gap-2">
              <CopyButton text={state.link} label="Copy link" size="md" />
              {typeof navigator !== 'undefined' && navigator.share && (
                <Button onClick={() => navigator.share({ title: `JapanOSINT: ${query}`, url: state.link }).catch(() => {})}>Share…</Button>
              )}
            </div>
          </div>
        )}
        <div className="rounded-md border border-osint-border bg-osint-bg/50 p-2 text-[11px] text-osint-muted">
          <span className="text-osint-text font-medium">Not a grant.</span> The link carries only what to show. Whoever opens it signs in with their own account, and the server re-checks their workspace and role before answering.
        </div>
      </div>
    </Sheet>
  );
}

/** Toolbar for a completed run: star, pin to case, save search, share. */
export default function RunActions({ requestId, query, link }) {
  const [saveOpen, setSaveOpen] = useState(false);
  const [shareOpen, setShareOpen] = useState(false);
  return (
    <div className="flex flex-wrap items-center gap-1.5">
      <SaveStarButton size="sm" item={{ kind: 'search_run', refId: requestId, displayName: query, link }} />
      <PinToCaseButton refType="search_run" refId={requestId} label={query} />
      <Button size="sm" onClick={() => setSaveOpen(true)} title="Save this search"><LuBookmark size={12} /> Save search</Button>
      <Button size="sm" onClick={() => setShareOpen(true)} title="Share a link to this view"><LuLink size={12} /> Share</Button>
      <SaveSearchSheet open={saveOpen} onClose={() => setSaveOpen(false)} query={query} />
      <SearchShareSheet open={shareOpen} onClose={() => setShareOpen(false)} query={query} requestId={requestId} />
    </div>
  );
}
