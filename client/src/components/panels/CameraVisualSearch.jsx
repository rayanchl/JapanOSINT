import React, { useCallback, useEffect, useState } from 'react';
import { MdImageSearch, MdClose } from 'react-icons/md';
import { fetchStillObjectUrl } from '../../utils/cameraStills.js';
import {
  fetchVisualSearch, fetchSimilarToStill, coverageNote,
} from '../../utils/visualSearch.js';

/**
 * "Search cameras by what they show" (native/core/visualsearchapi.h).
 *
 * A sentence ("snow on the road", "冠水した道路") is matched against whole
 * camera frames by the server's vision index; results are one row per camera
 * with its best-matching frame. Each still hit can be pivoted into "find
 * similar cameras" (image→image). Public-infrastructure cameras only — the
 * server applies the policy; nothing here matches people or faces.
 *
 * Thumbnails are fetched with fetch() + a byte sniff (the raw route is an
 * operator-gated octet-stream), so a 401/403 is shown as its reason in place
 * of the image. A camera that matched on its weekly reference frame has no
 * still to show or pivot on, and says so.
 */

function openCameraOnMap(hit) {
  if (!Number.isFinite(hit.lat) || !Number.isFinite(hit.lon)) return;
  window.dispatchEvent(new CustomEvent('japanosint:pin-camera', {
    detail: {
      camera: {
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [hit.lon, hit.lat] },
        properties: { uid: hit.item_uid, name: hit.name, camera_uid: hit.camera_id },
      },
    },
  }));
}

export function FrameThumb({ hit }) {
  const stillId = hit?.still?.id || null;
  const [img, setImg] = useState({ status: stillId ? 'loading' : 'none', src: null, why: null });
  useEffect(() => {
    if (!stillId) { setImg({ status: 'none', src: null, why: null }); return undefined; }
    let cancelled = false;
    let objectUrl = null;
    setImg({ status: 'loading', src: null, why: null });
    fetchStillObjectUrl(stillId)
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
  }, [stillId]);
  const box = 'w-20 h-14 rounded border border-osint-border/40 bg-osint-bg/70 flex-shrink-0 overflow-hidden';
  if (img.status === 'ok') {
    return <img src={img.src} alt={`frame of ${hit.name || hit.camera_id}`} className={`${box} object-cover`} />;
  }
  const text = img.status === 'loading' ? 'loading…'
    : img.status === 'failed' ? img.why
      : 'reference frame — no still';
  return (
    <div
      className={`${box} flex items-center justify-center text-[8px] leading-tight text-gray-500 text-center px-1`}
      title={text}
      role={img.status === 'failed' ? 'note' : undefined}
    >
      {text}
    </div>
  );
}

function HitRow({ hit, onSimilar }) {
  const when = hit.still?.captured_at || hit.frame?.captured_at || null;
  return (
    <li className="flex gap-2 py-1.5">
      <FrameThumb hit={hit} />
      <div className="flex-1 min-w-0">
        <div className="text-[11px] text-gray-200 truncate" title={hit.name || hit.camera_id}>
          {hit.name || hit.camera_id}
          {hit.view ? <span className="text-gray-500"> · {hit.view}</span> : null}
        </div>
        <div className="text-[9px] text-gray-500 font-mono">
          cos {Number.isFinite(hit.cosine) ? hit.cosine.toFixed(3) : '—'}
          {' · '}{hit.frames_matched} frame{hit.frames_matched === 1 ? '' : 's'}
          {when ? ` · ${when} UTC` : ''}
          {hit.frame?.kind === 'refframe' ? ' · reference frame' : ''}
        </div>
        <div className="flex gap-2 mt-0.5 text-[10px]">
          {hit.still?.id && onSimilar && (
            <button type="button" onClick={() => onSimilar(hit)} className="text-neon-cyan hover:underline">
              Find similar cameras
            </button>
          )}
          {Number.isFinite(hit.lat) && Number.isFinite(hit.lon) && (
            <button type="button" onClick={() => openCameraOnMap(hit)} className="text-gray-400 hover:text-gray-200 hover:underline">
              Pin on map
            </button>
          )}
        </div>
      </div>
    </li>
  );
}

function Results({ state, onSimilar }) {
  if (state.status === 'loading') return <p className="text-[11px] text-gray-500">Searching…</p>;
  if (state.status === 'failed') return <p role="alert" className="text-[11px] text-amber-300/90">{state.why}</p>;
  if (state.status !== 'ok') return null;
  const rows = state.body?.data || [];
  const note = coverageNote(state.body?.meta);
  return (
    <div className="space-y-1">
      {note && <p className="text-[10px] text-gray-500">{note}</p>}
      {rows.length === 0
        ? <p className="text-[11px] text-gray-500">No indexed camera matches.</p>
        : (
          <ul className="divide-y divide-osint-border/40">
            {rows.map((h) => <HitRow key={h.camera_id} hit={h} onSimilar={onSimilar} />)}
          </ul>
        )}
    </div>
  );
}

/** Cameras whose frames look like one still; used here and in the stills timeline. */
export function SimilarCameras({ stillId, label, onClose, onSimilar }) {
  const [state, setState] = useState({ status: 'loading', body: null, why: null });
  useEffect(() => {
    let cancelled = false;
    setState({ status: 'loading', body: null, why: null });
    fetchSimilarToStill(stillId)
      .then((b) => { if (!cancelled) setState({ status: 'ok', body: b, why: null }); })
      .catch((e) => { if (!cancelled) setState({ status: 'failed', body: null, why: e?.message || 'failed' }); });
    return () => { cancelled = true; };
  }, [stillId]);
  return (
    <div className="rounded border border-osint-border/60 p-2 space-y-1">
      <div className="flex items-center justify-between text-[10px] text-gray-400">
        <span>Cameras that look like {label || 'this frame'}</span>
        {onClose && (
          <button type="button" onClick={onClose} aria-label="Close similar cameras" className="text-gray-500 hover:text-gray-200">
            <MdClose size={12} />
          </button>
        )}
      </div>
      <Results state={state} onSimilar={onSimilar} />
    </div>
  );
}

export default function CameraVisualSearch() {
  const [q, setQ] = useState('');
  const [state, setState] = useState({ status: 'idle', body: null, why: null });
  const [similar, setSimilar] = useState(null);   // the hit being pivoted on

  const run = useCallback(async (e) => {
    e?.preventDefault?.();
    const text = q.trim();
    if (!text) return;
    setSimilar(null);
    setState({ status: 'loading', body: null, why: null });
    try {
      const body = await fetchVisualSearch(text);
      setState({ status: 'ok', body, why: null });
    } catch (err) {
      setState({ status: 'failed', body: null, why: err?.message || 'failed' });
    }
  }, [q]);

  return (
    <div className="border-b border-osint-border/40 px-3 py-2 space-y-1.5">
      <form onSubmit={run} className="flex items-center gap-1.5" role="search">
        <div className="relative flex-1">
          <MdImageSearch size={14} className="absolute left-1.5 top-1/2 -translate-y-1/2 text-gray-500 pointer-events-none" />
          <input
            type="search"
            value={q}
            onChange={(e) => setQ(e.target.value)}
            placeholder="Search cameras by what they show — e.g. snow on the road"
            aria-label="Search cameras by what they show"
            className="w-full pl-6 pr-2 py-1 bg-osint-bg/70 border border-osint-border/60 rounded text-[11px] text-gray-200 placeholder-gray-600 focus:outline-none focus:border-neon-cyan/40"
          />
        </div>
        <button
          type="submit"
          disabled={!q.trim() || state.status === 'loading'}
          className="px-2 py-1 rounded text-[10px] font-mono border bg-osint-bg/60 text-gray-300 border-osint-border/60 hover:text-neon-cyan disabled:opacity-40"
        >
          Search
        </button>
      </form>
      <p className="text-[9px] text-gray-600">
        Whole-frame scene matching over public-infrastructure cameras; no person or face matching.
      </p>
      {similar
        ? (
          <SimilarCameras
            key={similar.still.id}
            stillId={similar.still.id}
            label={similar.name || similar.camera_id}
            onClose={() => setSimilar(null)}
            onSimilar={setSimilar}
          />
        )
        : <Results state={state} onSimilar={setSimilar} />}
    </div>
  );
}
