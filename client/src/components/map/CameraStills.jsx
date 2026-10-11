import React, { useCallback, useEffect, useState } from 'react';
import {
  fetchStills, fetchStillObjectUrl,
} from '../../utils/cameraStills.js';

/**
 * Stills timeline for one camera record (native/core/camera_stills.c).
 *
 * Collapsed by default so opening a camera popup costs no request; expanding
 * lists the frames newest first (every view of the record, each labelled) and
 * the capture state. Clicking a frame fetches its raw bytes — an
 * operator-gated route — and a refusal is shown as the reason, not as a
 * broken image. `scene_changed` is tri-state: null means nobody could tell
 * (no decoder on the server), which is NOT "no change".
 */
function sceneBadge(v) {
  if (v === true) return { t: 'changed', c: 'text-amber-300' };
  if (v === false) return { t: 'same', c: 'text-gray-500' };
  return { t: 'unknown', c: 'text-gray-600' };
}

function StillFrame({ still, onClose }) {
  const [img, setImg] = useState({ status: 'loading', src: null, why: null });
  useEffect(() => {
    let cancelled = false;
    let objectUrl = null;
    setImg({ status: 'loading', src: null, why: null });
    fetchStillObjectUrl(still.id)
      .then((u) => {
        if (cancelled) { URL.revokeObjectURL(u); return; }
        objectUrl = u;
        setImg({ status: 'ok', src: u, why: null });
      })
      .catch((e) => {
        if (!cancelled) setImg({ status: 'failed', src: null, why: e?.message || 'failed' });
      });
    return () => {
      cancelled = true;
      if (objectUrl) URL.revokeObjectURL(objectUrl);
    };
  }, [still.id]);
  return (
    <div className="space-y-1">
      <div className="relative w-full aspect-video rounded border border-osint-border overflow-hidden bg-osint-panel">
        {img.status === 'ok' && (
          <img src={img.src} alt={`still ${still.captured_at}`} className="w-full h-full object-contain" />
        )}
        {img.status === 'loading' && (
          <div className="absolute inset-0 flex items-center justify-center text-[11px] text-gray-400">
            Loading frame…
          </div>
        )}
        {img.status === 'failed' && (
          <div role="alert" className="absolute inset-0 flex items-center justify-center text-[11px] text-gray-400 px-3 text-center">
            {img.why}
          </div>
        )}
      </div>
      <div className="flex justify-between text-[10px] text-gray-500 font-mono">
        <span>{still.captured_at} UTC{still.view ? ` · ${still.view}` : ''}</span>
        <button type="button" onClick={onClose} className="text-neon-cyan hover:underline">close</button>
      </div>
      {still.ocr_text && (
        <p className="text-[11px] text-gray-400 whitespace-pre-wrap">OCR: {still.ocr_text}</p>
      )}
    </div>
  );
}

export default function CameraStills({ cameraId }) {
  const [open, setOpen] = useState(false);
  const [state, setState] = useState({ status: 'idle', rows: [], meta: null, cursor: null, why: null });
  const [sel, setSel] = useState(null);

  const load = useCallback(async (cursor) => {
    setState((s) => ({ ...s, status: 'loading', why: null }));
    try {
      const r = await fetchStills(cameraId, { cursor });
      setState((s) => ({
        status: 'ok',
        rows: cursor ? [...s.rows, ...(r?.data || [])] : (r?.data || []),
        meta: r?.meta || s.meta,
        cursor: r?.page?.cursor || null,
        why: null,
      }));
    } catch (e) {
      setState((s) => ({ ...s, status: 'failed', why: e?.message || 'failed' }));
    }
  }, [cameraId]);

  useEffect(() => {
    setState({ status: 'idle', rows: [], meta: null, cursor: null, why: null });
    setSel(null);
  }, [cameraId]);

  useEffect(() => {
    if (open && state.status === 'idle') load(null);
  }, [open, state.status, load]);

  const m = state.meta;
  const targets = Array.isArray(m?.targets) ? m.targets : [];
  const failing = targets.filter((t) => t.last_error);
  return (
    <div className="rounded border border-osint-border">
      <button
        type="button"
        onClick={() => setOpen((v) => !v)}
        aria-expanded={open}
        className="w-full flex items-center justify-between px-2 py-1 text-xs text-gray-300 hover:bg-osint-panel"
      >
        <span>Stills timeline</span>
        <span className="text-gray-500">{open ? '▾' : '▸'}</span>
      </button>
      {open && (
        <div className="px-2 pb-2 space-y-2">
          {state.status === 'failed' && (
            <p role="alert" className="text-[11px] text-red-300">Could not load stills: {state.why}</p>
          )}
          {m && (
            <div className="text-[11px] text-gray-400 space-y-0.5">
              <p>
                {m.total} frame{m.total === 1 ? '' : 's'}
                {m.changed ? ` · ${m.changed} scene change${m.changed === 1 ? '' : 's'}` : ''}
                {m.unknown ? ` · ${m.unknown} undetermined` : ''}
                {m.oldest ? ` · ${m.oldest} → ${m.newest} UTC` : ''}
              </p>
              <p>
                Capture {m.capture_stills ? 'ON' : 'OFF'}
                {targets.length > 1 ? ` · ${targets.length} views` : ''}
                {m.targets_total > targets.length ? ` (of ${m.targets_total}; ${m.targets_total - targets.length} beyond the per-record limit are not captured)` : ''}
                {m.capture_stills === false ? ' — an operator enables it per camera' : ''}
              </p>
              {failing.length > 0 && (
                <p className="text-amber-300/80">
                  {failing.length} view{failing.length === 1 ? '' : 's'} failing: {failing.slice(0, 3).map((t) => `${t.view || 'main'} (${t.last_error})`).join(', ')}
                  {failing.length > 3 ? ` and ${failing.length - 3} more` : ''}
                </p>
              )}
            </div>
          )}
          {sel && <StillFrame still={sel} onClose={() => setSel(null)} />}
          {state.status === 'ok' && state.rows.length === 0 && (
            <p className="text-[11px] text-gray-500">No stills captured for this camera.</p>
          )}
          {state.rows.length > 0 && (
            <ul className="max-h-48 overflow-y-auto divide-y divide-osint-border">
              {state.rows.map((s) => {
                const b = sceneBadge(s.scene_changed);
                return (
                  <li key={s.id}>
                    <button
                      type="button"
                      onClick={() => setSel(s)}
                      className={`w-full text-left px-1 py-0.5 text-[11px] font-mono hover:bg-osint-panel ${sel?.id === s.id ? 'bg-osint-panel' : ''}`}
                    >
                      <span className="text-gray-300">{s.captured_at}</span>
                      {s.view && <span className="text-gray-500"> · {s.view}</span>}
                      <span className={`${b.c}`}> · {b.t}</span>
                      {s.blob_present === false && <span className="text-gray-600"> · no bytes</span>}
                    </button>
                  </li>
                );
              })}
            </ul>
          )}
          {state.cursor && (
            <button
              type="button"
              disabled={state.status === 'loading'}
              onClick={() => load(state.cursor)}
              className="text-[11px] text-neon-cyan hover:underline disabled:opacity-50"
            >
              {state.status === 'loading' ? 'Loading…' : `Load older (showing ${state.rows.length} of ${m?.total ?? '?'})`}
            </button>
          )}
          {state.status === 'loading' && !state.cursor && (
            <p className="text-[11px] text-gray-500">Loading…</p>
          )}
        </div>
      )}
    </div>
  );
}
