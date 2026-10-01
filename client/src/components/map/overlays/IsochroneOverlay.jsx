import React, { useEffect, useMemo, useRef, useState } from 'react';
import { LuX, LuFootprints, LuCrosshair } from 'react-icons/lu';
import useIsochrone from '../../../hooks/useIsochrone.js';
import useMapOverlayLayer from '../../../hooks/useMapOverlayLayer.js';
import { Button, Pill, ErrorNotice, Spinner, cx } from '../../ui/kit.jsx';

/**
 * Travel-time reachability — the web port of the iOS `IsochroneOverlay`
 * (roadmap 32). Origin comes from clicking the map (or the map centre);
 * bands are nested MultiPolygons graded by alpha in ONE hue, because they
 * stack. A 429 becomes a countdown, an empty collection is "walking only",
 * and a server without GTFS says so in `meta.note` — shown verbatim.
 */
const BAND_COUNT = 3;
const LAYERS = [
  { id: 'jo-iso-fill', type: 'fill', paint: { 'fill-color': 'rgb(91,231,241)', 'fill-opacity': ['interpolate', ['linear'], ['get', '_grade'], 0, 0.10, 1, 0.34] } },
  { id: 'jo-iso-line', type: 'line', paint: { 'line-color': 'rgb(91,231,241)', 'line-width': 1.2, 'line-opacity': ['interpolate', ['linear'], ['get', '_grade'], 0, 0.45, 1, 0.9] } },
];
const ORIGIN_LAYERS = [
  { id: 'jo-iso-origin', type: 'circle', paint: { 'circle-radius': 6, 'circle-color': 'rgb(255,179,71)', 'circle-stroke-color': '#0B0F14', 'circle-stroke-width': 2 } },
];

function jstLocalInput(d = new Date()) {
  // datetime-local wants "YYYY-MM-DDTHH:MM" in the chosen zone (JST).
  const p = new Intl.DateTimeFormat('en-GB', { timeZone: 'Asia/Tokyo', year: 'numeric', month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit', hour12: false }).formatToParts(d);
  const g = (t) => p.find((x) => x.type === t)?.value;
  return `${g('year')}-${g('month')}-${g('day')}T${g('hour')}:${g('minute')}`;
}

export default function IsochroneOverlay({ mapRef, open, onClose }) {
  const { result, error, running, retryAfter, compute, clear } = useIsochrone();
  const [origin, setOrigin] = useState(null);
  const [picking, setPicking] = useState(true);
  const [maxMin, setMaxMin] = useState(30);
  const [maxWalk, setMaxWalk] = useState(800);
  const [walkKmh, setWalkKmh] = useState(4.8);
  const [transfers, setTransfers] = useState(2);
  const [depart, setDepart] = useState('');   // '' = now
  const pickingRef = useRef(picking);
  pickingRef.current = picking;

  useEffect(() => {
    const map = mapRef?.current;
    if (!map || !open) return undefined;
    const canvas = map.getCanvas();
    const onClick = (e) => {
      if (!pickingRef.current) return;
      setOrigin({ lat: e.lngLat.lat, lon: e.lngLat.lng });
      setPicking(false);
    };
    map.on('click', onClick);
    const restore = canvas.style.cursor;
    if (picking) canvas.style.cursor = 'crosshair';
    return () => { map.off('click', onClick); canvas.style.cursor = restore; };
  }, [mapRef, open, picking]);

  useEffect(() => { if (!open) { clear(); setOrigin(null); setPicking(true); } }, [open, clear]);

  const bandsFc = useMemo(() => {
    if (!result) return null;
    const n = result.bands.length;
    return {
      type: 'FeatureCollection',
      features: result.bands.map((f, i) => ({ ...f, properties: { ...f.properties, _grade: n > 1 ? i / (n - 1) : 1 } })),
    };
  }, [result]);
  const originFc = useMemo(() => (origin ? { type: 'FeatureCollection', features: [{ type: 'Feature', properties: {}, geometry: { type: 'Point', coordinates: [origin.lon, origin.lat] } }] } : null), [origin]);

  useMapOverlayLayer(mapRef, 'jo-iso', open ? bandsFc : null, LAYERS);
  useMapOverlayLayer(mapRef, 'jo-iso-origin-src', open ? originFc : null, ORIGIN_LAYERS);

  const run = async () => {
    if (!origin) return;
    const step = Math.max(1, Math.round(maxMin / BAND_COUNT));
    const bands = [...new Set(Array.from({ length: BAND_COUNT }, (_, i) => Math.min(maxMin, step * (i + 1))))].sort((a, b) => a - b);
    const r = await compute({ lat: origin.lat, lon: origin.lon, maxMin, maxWalkM: maxWalk, walkKmh, maxTransfers: transfers, bands, depart: depart ? `${depart}:00+09:00` : undefined });
    const map = mapRef?.current;
    if (r?.features?.length && map) {
      let w = Infinity, s = Infinity, e = -Infinity, n = -Infinity;
      const walk = (c) => { if (typeof c[0] === 'number') { w = Math.min(w, c[0]); e = Math.max(e, c[0]); s = Math.min(s, c[1]); n = Math.max(n, c[1]); } else c.forEach(walk); };
      r.features.forEach((f) => f.geometry?.coordinates && walk(f.geometry.coordinates));
      if (Number.isFinite(w)) map.fitBounds([[w, s], [e, n]], { padding: 60, duration: 600 });
    }
  };

  const useCentre = () => {
    const c = mapRef?.current?.getCenter?.();
    if (c) { setOrigin({ lat: c.lat, lon: c.lng }); setPicking(false); }
  };

  if (!open) return null;
  const meta = result?.meta || {};
  const walkingOnly = result && (result.bands.length === 0 || (meta.stops_reached ?? 0) === 0);
  const computeLabel = retryAfter ? `Rate limited · ${retryAfter}s` : running ? 'Computing…' : 'Compute';

  return (
    <div className="absolute right-3 top-14 md:top-3 z-40 w-[min(94vw,360px)] glass-panel p-3 space-y-2.5 shadow-xl max-h-[calc(100%-6rem)] overflow-auto">
      <div className="flex items-center justify-between">
        <div className="font-mono text-[10px] uppercase tracking-[0.12em] text-accent flex items-center gap-1.5"><LuFootprints size={12} /> Travel time from…</div>
        <button type="button" onClick={onClose} className="text-osint-muted hover:text-osint-text" aria-label="Close"><LuX size={14} /></button>
      </div>

      <div className="flex items-center gap-2 text-[11px]">
        <span className="font-mono text-osint-text flex-1 truncate">{origin ? `${origin.lat.toFixed(5)}, ${origin.lon.toFixed(5)}` : (picking ? 'Click the map to set the origin' : 'No origin')}</span>
        <Button size="sm" variant={picking ? 'primary' : 'secondary'} onClick={() => setPicking(true)} title="Pick on map"><LuCrosshair size={12} /> Pick</Button>
        <Button size="sm" onClick={useCentre}>Centre</Button>
      </div>

      <Row label="Budget" value={`${maxMin} min`}>
        <input type="range" min={5} max={90} step={5} value={maxMin} onChange={(e) => setMaxMin(Number(e.target.value))} className="flex-1 accent-[rgb(255,179,71)]" />
      </Row>
      <Row label="Walk cap" value={`${maxWalk} m`}>
        <input type="range" min={100} max={2000} step={50} value={maxWalk} onChange={(e) => setMaxWalk(Number(e.target.value))} className="flex-1 accent-[rgb(255,179,71)]" />
      </Row>
      <Row label="Pace" value={`${walkKmh.toFixed(1)} km/h`}>
        <input type="range" min={2} max={7} step={0.1} value={walkKmh} onChange={(e) => setWalkKmh(Number(e.target.value))} className="flex-1 accent-[rgb(255,179,71)]" />
      </Row>
      <div className="flex items-center justify-between text-[11px] text-osint-muted">
        <span>Transfers</span>
        <span className="inline-flex items-center gap-1">
          <Button size="sm" disabled={transfers <= 0} onClick={() => setTransfers((t) => t - 1)}>−</Button>
          <span className="font-mono text-osint-text w-4 text-center">{transfers}</span>
          <Button size="sm" disabled={transfers >= 4} onClick={() => setTransfers((t) => t + 1)}>+</Button>
        </span>
      </div>
      <div className="flex items-center justify-between gap-2 text-[11px] text-osint-muted">
        <span>Departure (JST)</span>
        <span className="inline-flex items-center gap-1">
          <input type="datetime-local" value={depart} onChange={(e) => setDepart(e.target.value)} className="bg-osint-bg border border-osint-border rounded px-1.5 py-0.5 text-[11px] text-osint-text font-mono" />
          <Button size="sm" variant="ghost" onClick={() => setDepart('')}>{depart ? 'Now' : ''}</Button>
          {!depart && <Button size="sm" variant="ghost" onClick={() => setDepart(jstLocalInput())}>Set</Button>}
        </span>
      </div>
      <div className="text-[10px] text-osint-muted">One JST service day is queried — a departure in the small hours under-reports late-night service.</div>

      <div className="flex items-center gap-2">
        <Button variant="primary" className="flex-1" busy={running} disabled={!origin || running || retryAfter != null} onClick={run}>{computeLabel}</Button>
        <Button variant="ghost" onClick={() => { clear(); }}>Clear</Button>
      </div>

      {error && error.status !== 429 && <ErrorNotice error={error} title="Reachability failed" />}
      {error && error.status === 429 && <div className="text-[11px] text-accent">Rate limited by the server — retry in {retryAfter ?? '…'}s.</div>}

      {result && (
        <div className="space-y-1.5 border-t border-osint-border pt-2">
          <div className="flex items-center gap-1.5 flex-wrap text-[11px]">
            {walkingOnly && <Pill tone="warning">WALKING ONLY</Pill>}
            {meta.cached && <Pill>cached</Pill>}
            {meta.truncated && <Pill tone="danger" title={(meta.truncated_reasons || []).join(', ')}>truncated</Pill>}
            <span className="text-osint-muted">
              {result.bands.length === 0 ? 'Nothing is reachable from here on this service day.' : `${result.bands.length} band${result.bands.length === 1 ? '' : 's'} · ${meta.stops_reached ?? 0} stops reached.`}
            </span>
          </div>
          {meta.note && <div className="text-[11px] text-accent">{meta.note}</div>}
          {meta.stops_note && <div className="text-[11px] text-osint-muted">{meta.stops_note}</div>}
          {meta.stop_read_error && <div className="text-[11px] text-neon-red">stop read error: {meta.stop_read_error}</div>}
          <div className="font-mono text-[10px] text-osint-muted">
            {[meta.service_date && `${meta.service_date} ${meta.weekday || ''}`.trim(), meta.depart_at && `depart ${meta.depart_at}`, Number.isFinite(meta.elapsed_ms) && `${Math.round(meta.elapsed_ms)} ms`].filter(Boolean).join(' · ')}
          </div>
          {result.bands.length > 0 && (
            <div className="flex items-center gap-2 flex-wrap">
              {[...result.bands].reverse().map((b, i, arr) => (
                <span key={b.properties.band_min} className="inline-flex items-center gap-1 text-[10px] font-mono text-osint-text">
                  <span className="inline-block w-3 h-3 rounded-sm border border-neon-cyan/60" style={{ background: `rgba(91,231,241,${0.34 - (i / Math.max(1, arr.length - 1)) * 0.24})` }} />
                  {b.properties.band_min} min{Number.isFinite(b.properties.stops_within) ? ` · ${b.properties.stops_within} stops` : ''}
                </span>
              ))}
            </div>
          )}
        </div>
      )}
      {running && !result && <div className="flex items-center gap-2 text-[11px] text-osint-muted"><Spinner size={12} /> Seconds of CPU on a cache miss…</div>}
    </div>
  );
}

function Row({ label, value, children }) {
  return (
    <label className={cx('flex items-center gap-2 text-[11px] text-osint-muted')}>
      <span className="w-16">{label}</span>
      {children}
      <span className="font-mono text-osint-text w-16 text-right">{value}</span>
    </label>
  );
}
