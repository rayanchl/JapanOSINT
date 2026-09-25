import { useCallback, useEffect, useMemo, useRef, useState } from 'react';

/**
 * Global playback time window — the web port of the iOS `PlaybackState` +
 * `TimeWindow`. One window applied uniformly to every active time-coded
 * layer: `at == null` is LIVE (no filter); otherwise features whose event
 * time falls in [at − window, at] are shown.
 *
 * The server's layer catalogue tells us which layers are time-coded
 * (`temporal: { field, fallbackField }`, from core/miscapi.c) and which are
 * live-only snapshots (`liveOnly: true`). The filter is applied client-side
 * to the collections already fetched — /api/layers/:id/geojson takes no
 * `at`/`window` today, so this cannot page history the client never
 * fetched; the UI states that bound.
 */
export const TIME_WINDOWS = [
  { sec: 300, label: '5m' },
  { sec: 900, label: '15m' },
  { sec: 3600, label: '1h' },
  { sec: 21600, label: '6h' },
  { sec: 86400, label: '24h' },
  { sec: 259200, label: '3d' },
  { sec: 604800, label: '7d' },
];

const STORAGE = 'osint:map:time-window';

function parseTs(v) {
  if (v == null || v === '') return NaN;
  if (typeof v === 'number') return v < 1e12 ? v * 1000 : v; // epoch s vs ms
  const t = Date.parse(String(v));
  return Number.isFinite(t) ? t : NaN;
}

/** Event time of one feature under a catalogue temporal spec, or NaN. */
export function featureTime(feature, temporal) {
  const p = feature?.properties;
  if (!p) return NaN;
  const cand = [temporal?.field, temporal?.fallbackField, 'published_at', 'fetched_at', 'timestamp', 'time', 'observed_at', 'updated_at'];
  for (const k of cand) {
    if (!k) continue;
    const t = parseTs(p[k]);
    if (Number.isFinite(t)) return t;
  }
  return NaN;
}

/**
 * Filter a layerDataView by the window. Returns the filtered view plus a
 * per-layer report so the UI can say which visible layers the window does
 * NOT apply to, instead of implying every dot on the map is inside it.
 */
export function applyTimeWindow(layerDataView, layers, catalog, { at, windowSec }) {
  const report = { timeAware: [], liveOnly: [], noTimestamps: [] };
  if (at == null) return { view: layerDataView, report, filtering: false };
  const lo = at - windowSec * 1000;
  const hi = at;
  const view = {};
  for (const [id, fc] of Object.entries(layerDataView || {})) {
    const visible = layers?.[id]?.visible;
    const entry = catalog?.[id];
    const temporal = entry?.temporal && typeof entry.temporal === 'object' ? entry.temporal : null;
    if (!visible || !Array.isArray(fc?.features)) { view[id] = fc; continue; }
    if (entry?.liveOnly) { report.liveOnly.push(id); view[id] = fc; continue; }
    let seen = 0;
    const kept = fc.features.filter((f) => {
      const t = featureTime(f, temporal);
      if (!Number.isFinite(t)) return false;
      seen++;
      return t >= lo && t <= hi;
    });
    if (seen === 0) {
      // Not one record carries a parsable time: the window cannot be applied.
      report.noTimestamps.push(id);
      view[id] = fc;
      continue;
    }
    report.timeAware.push(id);
    view[id] = { ...fc, features: kept, _timeWindow: { shown: kept.length, of: fc.features.length, withTime: seen } };
  }
  return { view, report, filtering: true };
}

export default function useTimeWindow() {
  const [windowSec, setWindowSec] = useState(() => {
    try {
      const v = Number(window.localStorage.getItem(STORAGE));
      return TIME_WINDOWS.some((w) => w.sec === v) ? v : 3600;
    } catch { return 3600; }
  });
  const [at, setAt] = useState(null);          // null = LIVE
  const [playing, setPlaying] = useState(false);
  const [speed, setSpeed] = useState(60);       // seconds of data per real second
  const raf = useRef(null);

  useEffect(() => { try { window.localStorage.setItem(STORAGE, String(windowSec)); } catch { /* ignore */ } }, [windowSec]);

  // Replay: advance `at` toward now; stop at now and drop to LIVE.
  useEffect(() => {
    if (!playing) return undefined;
    let last = performance.now();
    const tick = (now) => {
      const dt = (now - last) / 1000; last = now;
      setAt((cur) => {
        const base = cur == null ? Date.now() - windowSec * 1000 : cur;
        const next = base + dt * speed * 1000;
        if (next >= Date.now()) { setPlaying(false); return null; }
        return next;
      });
      raf.current = requestAnimationFrame(tick);
    };
    raf.current = requestAnimationFrame(tick);
    return () => { if (raf.current) cancelAnimationFrame(raf.current); };
  }, [playing, speed, windowSec]);

  const resumeLive = useCallback(() => { setPlaying(false); setAt(null); }, []);
  const step = useCallback((seconds) => {
    setAt((cur) => {
      const base = cur == null ? Date.now() : cur;
      const next = Math.min(Date.now(), base + seconds * 1000);
      return next >= Date.now() && seconds > 0 ? null : next;
    });
  }, []);

  return useMemo(() => ({
    at, setAt, windowSec, setWindowSec, playing, setPlaying, speed, setSpeed, resumeLive, step,
    isReplaying: at != null,
  }), [at, windowSec, playing, speed, resumeLive, step]);
}
