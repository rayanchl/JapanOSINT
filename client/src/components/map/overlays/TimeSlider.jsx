import React, { useEffect, useMemo, useState } from 'react';
import { LuPlay, LuPause, LuRadio, LuChevronDown, LuChevronUp } from 'react-icons/lu';
import { TIME_WINDOWS } from '../../../hooks/useTimeWindow.js';
import { Button, cx } from '../../ui/kit.jsx';

const RANGE_SEC = 7 * 86400;   // scrubber spans the last 7 days
const STEPS = [
  { label: '−1h', sec: -3600 }, { label: '−5m', sec: -300 },
  { label: '+5m', sec: 300 }, { label: '+1h', sec: 3600 },
];

function relativeAgo(at) {
  if (at == null) return 'now';
  const s = Math.max(0, Math.round((Date.now() - at) / 1000));
  if (s < 60) return `${s}s ago`;
  const m = Math.floor(s / 60);
  if (m < 60) return `${m}m ago`;
  const h = Math.floor(m / 60);
  if (h < 24) return `${h}h ${m % 60}m ago`;
  return `${Math.floor(h / 24)}d ${h % 24}h ago`;
}

function jst(ms) {
  return new Date(ms).toLocaleString('en-GB', { timeZone: 'Asia/Tokyo', month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit', hour12: false });
}

/**
 * Time window + scrubber + play/pause — the web port of the iOS
 * `TimeSliderView`. `tw` is the object from `useTimeWindow()`; `report` is
 * the per-layer result of `applyTimeWindow` so the bar can say which visible
 * layers the window does NOT filter, instead of pretending it does.
 */
export default function TimeSlider({ tw, report, view, layers, catalog, collapsed, onToggleCollapsed }) {
  const [, tick] = useState(0);
  useEffect(() => { const t = setInterval(() => tick((x) => x + 1), 15000); return () => clearInterval(t); }, []);

  const now = Date.now();
  const lo = now - RANGE_SEC * 1000;
  const norm = tw.at == null ? 1 : Math.max(0, Math.min(1, (tw.at - lo) / (now - lo)));

  const shown = useMemo(() => {
    let a = 0, b = 0;
    for (const id of report?.timeAware || []) {
      const m = view?.[id]?._timeWindow;
      if (m) { a += m.shown; b += m.of; }
    }
    return { shown: a, of: b };
  }, [report, view]);

  const name = (id) => catalog?.[id]?.name || id;
  const excluded = [...(report?.liveOnly || []), ...(report?.noTimestamps || [])];

  return (
    <div className={cx('absolute left-1/2 -translate-x-1/2 z-30 w-[min(96vw,620px)] glass-panel shadow-lg transition-all',
      'bottom-8 md:bottom-9', tw.isReplaying && 'ring-1 ring-accent/50')}>
      <div className="flex items-center gap-2 px-3 py-1.5">
        <button
          type="button"
          onClick={tw.resumeLive}
          className={cx('inline-flex items-center gap-1 rounded-full border px-2 py-0.5 text-[10px] font-mono font-semibold',
            tw.isReplaying ? 'border-osint-border text-osint-muted hover:text-neon-green' : 'border-neon-green/50 text-neon-green bg-neon-green/10')}
          title={tw.isReplaying ? 'Return to live' : 'Live — no time filter'}
        >
          <LuRadio size={11} className={tw.isReplaying ? '' : 'pulse-live'} /> LIVE
        </button>
        <div className="flex-1 min-w-0 text-[11px] font-mono text-osint-text truncate">
          {tw.isReplaying ? `Replay · ${TIME_WINDOWS.find((w) => w.sec === tw.windowSec)?.label || ''} window · ${jst(tw.at)} JST (${relativeAgo(tw.at)})` : 'Now · JST'}
        </div>
        <div className="hidden sm:flex items-center gap-0.5">
          {TIME_WINDOWS.map((w) => (
            <button
              key={w.sec}
              type="button"
              onClick={() => { tw.setWindowSec(w.sec); if (tw.at == null) tw.setAt(Date.now()); }}
              className={cx('px-1.5 py-0.5 rounded text-[10px] font-mono border transition-colors',
                tw.windowSec === w.sec ? 'border-accent/70 bg-accent/15 text-accent font-bold' : 'border-transparent text-osint-muted hover:text-osint-text')}
            >
              {w.label}
            </button>
          ))}
        </div>
        <select className="sm:hidden bg-osint-bg border border-osint-border rounded text-[10px] font-mono text-osint-text px-1 py-0.5" value={tw.windowSec} onChange={(e) => { tw.setWindowSec(Number(e.target.value)); if (tw.at == null) tw.setAt(Date.now()); }} aria-label="Time window">
          {TIME_WINDOWS.map((w) => <option key={w.sec} value={w.sec}>{w.label}</option>)}
        </select>
        <button type="button" onClick={onToggleCollapsed} className="text-osint-muted hover:text-osint-text" aria-label={collapsed ? 'Expand time controls' : 'Collapse time controls'}>
          {collapsed ? <LuChevronUp size={14} /> : <LuChevronDown size={14} />}
        </button>
      </div>

      {!collapsed && (
        <div className="px-3 pb-2 space-y-1.5 border-t border-osint-border/60">
          <div className="flex items-center gap-2 pt-1.5">
            <Button size="sm" variant={tw.playing ? 'primary' : 'secondary'} onClick={() => { if (!tw.playing && tw.at == null) tw.setAt(Date.now() - tw.windowSec * 1000); tw.setPlaying(!tw.playing); }} title={tw.playing ? 'Pause' : 'Replay'}>
              {tw.playing ? <LuPause size={12} /> : <LuPlay size={12} />}
            </Button>
            <input
              type="range" min={0} max={1000} value={Math.round(norm * 1000)}
              onChange={(e) => { const v = Number(e.target.value) / 1000; const at = lo + v * (now - lo); tw.setPlaying(false); tw.setAt(v >= 0.999 ? null : at); }}
              className="flex-1 accent-[rgb(255,179,71)]" aria-label="Scrub time"
            />
            <select value={tw.speed} onChange={(e) => tw.setSpeed(Number(e.target.value))} className="bg-osint-bg border border-osint-border rounded text-[10px] font-mono text-osint-text px-1 py-0.5" aria-label="Replay speed">
              {[10, 60, 300, 1800].map((s) => <option key={s} value={s}>{s >= 60 ? `${s / 60}m/s` : `${s}s/s`}</option>)}
            </select>
          </div>
          <div className="flex items-center gap-1 flex-wrap text-[10px]">
            {STEPS.map((s) => <Button key={s.label} size="sm" variant="ghost" onClick={() => { tw.setPlaying(false); tw.step(s.sec); }}>{s.label}</Button>)}
            <span className="ml-auto font-mono text-osint-muted">{jst(lo)} — now</span>
          </div>
          {tw.isReplaying && (
            <div className="text-[10px] font-mono">
              <span className="text-accent">showing {shown.shown} of {shown.of}</span>
              <span className="text-osint-muted"> records across {report?.timeAware?.length || 0} time-coded layer{(report?.timeAware?.length || 0) === 1 ? '' : 's'} — from what this browser has fetched; history the server was not asked for is not shown.</span>
              {excluded.length > 0 && (
                <div className="text-osint-muted mt-0.5">
                  Not filtered by time: {excluded.map((id) => `${name(id)}${(report.liveOnly || []).includes(id) ? ' (live snapshot)' : ' (no timestamps)'}`).join(', ')}
                </div>
              )}
            </div>
          )}
        </div>
      )}
    </div>
  );
}
