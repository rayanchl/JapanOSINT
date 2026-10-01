import React, { useEffect, useState } from 'react';
import usePermalink from '../../../hooks/usePermalink.js';
import { Sheet, Button, Input, CopyButton, ErrorNotice, LoadingState, KV } from '../../ui/kit.jsx';

/**
 * Share the current map view — the web port of the iOS `PermalinkShareSheet`.
 * State the server canonicalises (core/savedsearchapi.c pl_normalize):
 *   { kind, params, map: {lat, lon, zoom, bearing, pitch}, layers: [ids] }
 * Anything else is dropped server-side, so the time window rides in `params`.
 */
export function currentViewState(mapRef, layers, timeWindow) {
  const map = mapRef?.current;
  const c = map?.getCenter?.();
  const state = {
    kind: 'map',
    params: timeWindow?.at != null ? { at: new Date(timeWindow.at).toISOString(), window: timeWindow.windowSec } : {},
    map: c ? { lat: +c.lat.toFixed(6), lon: +c.lng.toFixed(6), zoom: +map.getZoom().toFixed(2), bearing: +map.getBearing().toFixed(1), pitch: +map.getPitch().toFixed(1) } : null,
    layers: Object.entries(layers || {}).filter(([, s]) => s.visible).map(([id]) => id),
  };
  return state;
}

export default function ShareSheet({ open, onClose, mapRef, layers, timeWindow }) {
  const { mint, busy, error } = usePermalink();
  const [link, setLink] = useState(null);
  const [state, setState] = useState(null);

  useEffect(() => {
    if (!open) { setLink(null); setState(null); return; }
    const s = currentViewState(mapRef, layers, timeWindow);
    setState(s);
    mint(s).then((r) => setLink(r)).catch(() => { /* error state rendered */ });
    // Mint once per open; the sheet shows the view as it was when opened.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [open]);

  const nativeShare = async () => {
    try { await navigator.share({ title: 'JapanOSINT map view', url: link.url }); } catch { /* cancelled */ }
  };

  return (
    <Sheet open={open} onClose={onClose} title="Share this view" width="max-w-md"
      footer={(
        <>
          {typeof navigator !== 'undefined' && navigator.share && link && <Button onClick={nativeShare}>Share…</Button>}
          <Button onClick={onClose}>Close</Button>
        </>
      )}
    >
      <div className="space-y-3">
        {busy && !link && <LoadingState label="Minting permalink…" />}
        {error && <ErrorNotice error={error} title="Could not mint a permalink" />}
        {link && (
          <div className="flex gap-2">
            <Input mono readOnly value={link.url} onFocus={(e) => e.target.select()} />
            <CopyButton text={link.url} size="md" />
          </div>
        )}
        {state && (
          <KV pairs={[
            ['centre', state.map ? `${state.map.lat}, ${state.map.lon}` : '—'],
            ['zoom', state.map?.zoom],
            ['layers', state.layers.length ? `${state.layers.length}: ${state.layers.join(', ')}` : 'none'],
            ['time', state.params.at ? `${state.params.at} · ${state.params.window}s window` : 'live'],
            ['token', link?.token],
          ]} />
        )}
        <div className="text-[11px] text-osint-muted">The link encodes the view itself (no server row); anyone with access to this workspace can open it.</div>
      </div>
    </Sheet>
  );
}
