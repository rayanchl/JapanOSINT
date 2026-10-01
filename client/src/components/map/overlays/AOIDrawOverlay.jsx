import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { LuUndo2, LuCircleCheck, LuX } from 'react-icons/lu';
import useMapOverlayLayer from '../../../hooks/useMapOverlayLayer.js';
import { circlePolygon } from '../../../hooks/useAoi.js';
import { Button, Input, Segmented, ErrorNotice, Field, cx, toast } from '../../ui/kit.jsx';

/**
 * Draw an area of interest on the map — the web port of the iOS
 * `AOIDrawOverlay`.
 *   • polygon — click each vertex, double-click (or "Close ring") to finish.
 *               Sent as [[lon,lat], …]; the server auto-closes the ring.
 *   • circle  — click the centre, move to size, click again (or use the
 *               slider). Sent as {lat, lon, radius_m}.
 * Saves through POST /api/aoi (core/aoiapi.c) via the `onSave` callback the
 * page supplies from `useAoi().create`, then goes to Areas of interest.
 */
const AOI_FILL = { 'fill-color': 'rgb(255,179,71)', 'fill-opacity': 0.12 };
const AOI_LINE = { 'line-color': 'rgb(255,179,71)', 'line-width': 2, 'line-dasharray': [2, 1.5] };
const DRAW_LAYERS = [
  { id: 'jo-aoi-draw-fill', type: 'fill', paint: AOI_FILL, filter: ['==', ['geometry-type'], 'Polygon'] },
  { id: 'jo-aoi-draw-line', type: 'line', paint: AOI_LINE, filter: ['in', ['geometry-type'], ['literal', ['Polygon', 'LineString']]] },
  { id: 'jo-aoi-draw-pts', type: 'circle', paint: { 'circle-radius': 5, 'circle-color': 'rgb(255,179,71)', 'circle-stroke-color': '#0B0F14', 'circle-stroke-width': 1.5 }, filter: ['==', ['geometry-type'], 'Point'] },
];

export function formatRadius(m) {
  if (!Number.isFinite(m)) return '—';
  return m >= 1000 ? `${(m / 1000).toFixed(m >= 10000 ? 0 : 1)} km` : `${Math.round(m)} m`;
}

function haversineM(a, b) {
  const R = 6371000;
  const toR = (d) => (d * Math.PI) / 180;
  const dLat = toR(b.lat - a.lat);
  const dLon = toR(b.lng - a.lng);
  const s = Math.sin(dLat / 2) ** 2 + Math.cos(toR(a.lat)) * Math.cos(toR(b.lat)) * Math.sin(dLon / 2) ** 2;
  return 2 * R * Math.asin(Math.sqrt(s));
}

export default function AOIDrawOverlay({ mapRef, active, onClose, onSave }) {
  const navigate = useNavigate();
  const [mode, setMode] = useState('polygon');
  const [points, setPoints] = useState([]);           // [{lng,lat}]
  const [closed, setClosed] = useState(false);
  const [center, setCenter] = useState(null);
  const [radius, setRadius] = useState(500);
  const [sizing, setSizing] = useState(false);
  const [name, setName] = useState('');
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState(null);
  const stateRef = useRef({ mode, sizing, center });
  stateRef.current = { mode, sizing, center };

  const clearAll = useCallback(() => {
    setPoints([]); setClosed(false); setCenter(null); setSizing(false); setError(null);
  }, []);

  // Map interaction while active.
  useEffect(() => {
    const map = mapRef?.current;
    if (!map || !active) return undefined;
    const canvas = map.getCanvas();
    const prevCursor = canvas.style.cursor;
    canvas.style.cursor = 'crosshair';
    const dcz = map.doubleClickZoom.isEnabled();
    map.doubleClickZoom.disable();

    const onClick = (e) => {
      const { mode: m, sizing: s, center: c } = stateRef.current;
      if (m === 'polygon') {
        setClosed(false);
        setPoints((ps) => [...ps, { lng: e.lngLat.lng, lat: e.lngLat.lat }]);
      } else if (!c) {
        setCenter({ lng: e.lngLat.lng, lat: e.lngLat.lat });
        setSizing(true);
      } else if (s) {
        setRadius(Math.max(10, haversineM(c, e.lngLat)));
        setSizing(false);
      } else {
        setCenter({ lng: e.lngLat.lng, lat: e.lngLat.lat });
        setSizing(true);
      }
    };
    const onMove = (e) => {
      const { mode: m, sizing: s, center: c } = stateRef.current;
      if (m === 'circle' && s && c) setRadius(Math.max(10, haversineM(c, e.lngLat)));
    };
    const onDbl = (e) => {
      e.preventDefault?.();
      if (stateRef.current.mode === 'polygon') setClosed(true);
    };
    map.on('click', onClick);
    map.on('mousemove', onMove);
    map.on('dblclick', onDbl);
    return () => {
      map.off('click', onClick);
      map.off('mousemove', onMove);
      map.off('dblclick', onDbl);
      canvas.style.cursor = prevCursor;
      if (dcz) map.doubleClickZoom.enable();
    };
  }, [mapRef, active]);

  useEffect(() => { if (!active) clearAll(); }, [active, clearAll]);

  const drawn = useMemo(() => {
    if (mode === 'polygon') {
      if (points.length < 3) return null;
      const ring = points.map((p) => [p.lng, p.lat]);
      let w = Infinity, s = Infinity, e = -Infinity, n = -Infinity;
      for (const [x, y] of ring) { w = Math.min(w, x); e = Math.max(e, x); s = Math.min(s, y); n = Math.max(n, y); }
      return { kind: 'polygon', geometry: ring, summary: `Polygon · ${points.length} pts`, bbox: [w, s, e, n] };
    }
    if (!center || !(radius > 0)) return null;
    return { kind: 'circle', geometry: { lat: center.lat, lon: center.lng, radius_m: Math.round(radius) }, summary: `Circle · ${formatRadius(radius)}` };
  }, [mode, points, center, radius]);

  // Preview geometry on the map.
  const preview = useMemo(() => {
    const features = [];
    if (mode === 'polygon') {
      const coords = points.map((p) => [p.lng, p.lat]);
      for (const c of coords) features.push({ type: 'Feature', properties: {}, geometry: { type: 'Point', coordinates: c } });
      if (coords.length >= 3 && closed) {
        features.push({ type: 'Feature', properties: {}, geometry: { type: 'Polygon', coordinates: [[...coords, coords[0]]] } });
      } else if (coords.length >= 2) {
        features.push({ type: 'Feature', properties: {}, geometry: { type: 'LineString', coordinates: coords } });
      }
    } else if (center) {
      features.push({ type: 'Feature', properties: {}, geometry: { type: 'Point', coordinates: [center.lng, center.lat] } });
      if (radius > 0) features.push({ type: 'Feature', properties: {}, geometry: circlePolygon(center.lat, center.lng, radius) });
    }
    return { type: 'FeatureCollection', features };
  }, [mode, points, closed, center, radius]);

  useMapOverlayLayer(mapRef, 'jo-aoi-draw', active ? preview : null, DRAW_LAYERS);

  const save = async () => {
    if (!drawn) return;
    const nm = name.trim();
    if (!nm) { setError(new Error('Give the area a name.')); return; }
    setSaving(true); setError(null);
    try {
      await onSave({ name: nm, kind: drawn.kind, geometry: drawn.geometry });
      toast(`Saved area "${nm}"`, { tone: 'accent' });
      onClose?.();
      navigate('/console/aoi');
    } catch (e) { setError(e); }
    finally { setSaving(false); }
  };

  if (!active) return null;

  return (
    <div className="absolute left-1/2 -translate-x-1/2 bottom-10 md:bottom-12 z-40 w-[min(96vw,460px)] glass-panel p-3 space-y-2 shadow-xl">
      <div className="flex items-center justify-between gap-2">
        <div className="font-mono text-[10px] uppercase tracking-[0.12em] text-accent">Draw area of interest</div>
        <button type="button" onClick={onClose} className="text-osint-muted hover:text-osint-text" aria-label="Cancel drawing"><LuX size={14} /></button>
      </div>
      <div className="flex items-center gap-2 flex-wrap">
        <Segmented value={mode} onChange={(m) => { setMode(m); clearAll(); }} options={[{ value: 'polygon', label: 'Polygon' }, { value: 'circle', label: 'Circle' }]} />
        {mode === 'polygon' ? (
          <>
            <span className="font-mono text-[11px] text-osint-muted">{points.length} pt</span>
            <Button size="sm" disabled={!points.length} onClick={() => { setPoints((ps) => ps.slice(0, -1)); setClosed(false); }}><LuUndo2 size={12} /> Undo point</Button>
            <Button size="sm" disabled={points.length < 3} variant={closed ? 'primary' : 'secondary'} onClick={() => setClosed(true)}><LuCircleCheck size={12} /> Close ring</Button>
          </>
        ) : (
          <>
            <span className="font-mono text-[11px] text-osint-muted">{center ? `${center.lat.toFixed(4)}, ${center.lng.toFixed(4)}` : 'click the centre'}</span>
            {center && <Button size="sm" onClick={() => setSizing((v) => !v)}>{sizing ? 'Done' : 'Drag radius'}</Button>}
          </>
        )}
        <Button size="sm" variant="ghost" onClick={clearAll}>Clear</Button>
      </div>
      {mode === 'circle' && center && (
        <label className="flex items-center gap-2 text-[11px] text-osint-muted">
          <span className="w-16">Radius</span>
          <input type="range" min={50} max={50000} step={50} value={Math.min(50000, Math.round(radius))} onChange={(e) => { setRadius(Number(e.target.value)); setSizing(false); }} className="flex-1 accent-[rgb(255,179,71)]" aria-label="Circle radius" />
          <span className="font-mono text-osint-text w-16 text-right">{formatRadius(radius)}</span>
        </label>
      )}
      <div className="text-[11px] text-osint-muted">
        {mode === 'polygon'
          ? 'Click each vertex; double-click or “Close ring” to finish.'
          : 'Click the centre, move the pointer to size it, click again to fix the radius.'}
      </div>
      {drawn && (
        <div className={cx('flex items-end gap-2')}>
          <Field label={drawn.summary} className="flex-1">
            <Input value={name} onChange={(e) => setName(e.target.value)} placeholder="Area name" onKeyDown={(e) => { if (e.key === 'Enter') save(); }} />
          </Field>
          <Button variant="primary" busy={saving} disabled={!name.trim()} onClick={save}>Save</Button>
        </div>
      )}
      {error && <ErrorNotice error={error} title="Could not save the area" />}
      <div className="text-[10px] text-osint-muted">Saved areas can be reused by any alert rule, and stay editable from Areas of interest.</div>
    </div>
  );
}

/** Existing areas drawn as a faint amber layer (toggle from the More menu). */
const AOI_LAYERS = [
  { id: 'jo-aoi-fill', type: 'fill', paint: { 'fill-color': 'rgb(255,179,71)', 'fill-opacity': 0.08 } },
  { id: 'jo-aoi-line', type: 'line', paint: { 'line-color': 'rgb(255,179,71)', 'line-width': 1.5, 'line-opacity': 0.8 } },
];
export function AoiLayer({ mapRef, geojson, visible }) {
  useMapOverlayLayer(mapRef, 'jo-aoi', visible ? geojson : null, AOI_LAYERS);
  return null;
}
