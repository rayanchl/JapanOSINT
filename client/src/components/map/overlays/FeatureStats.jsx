import React, { useEffect, useMemo, useState } from 'react';
import { LuX, LuChevronRight, LuChevronDown } from 'react-icons/lu';
import { cx } from '../../ui/kit.jsx';

/**
 * Per-layer feature counts — the web port of the iOS `FeatureStats`
 * (total per layer, and per-source breakdown inside an expanded row), plus
 * the count inside the current viewport, recomputed on every moveend.
 *
 * Counts come from the collections this browser holds. When a collection's
 * `_meta` says the server holds more than was fetched, that bound is shown
 * as "of N on server" rather than letting the local total pose as the whole.
 */
function inBounds(f, b) {
  const g = f?.geometry;
  if (!g || !b) return false;
  const test = (c) => c[0] >= b.w && c[0] <= b.e && c[1] >= b.s && c[1] <= b.n;
  switch (g.type) {
    case 'Point': return test(g.coordinates);
    case 'MultiPoint':
    case 'LineString': return g.coordinates.some(test);
    case 'MultiLineString':
    case 'Polygon': return g.coordinates.some((r) => r.some(test));
    case 'MultiPolygon': return g.coordinates.some((p) => p.some((r) => r.some(test)));
    default: return false;
  }
}

function sourceOf(f) {
  const p = f?.properties || {};
  return p.source_id || p.source || p.collector || p.provider || p.channel || null;
}

export default function FeatureStats({ mapRef, layers, view, catalog, open, onClose }) {
  const [bounds, setBounds] = useState(null);
  const [expanded, setExpanded] = useState(() => new Set());

  useEffect(() => {
    const map = mapRef?.current;
    if (!map || !open) return undefined;
    const read = () => {
      const b = map.getBounds();
      setBounds({ w: b.getWest(), s: b.getSouth(), e: b.getEast(), n: b.getNorth() });
    };
    read();
    map.on('moveend', read);
    return () => map.off('moveend', read);
  }, [mapRef, open]);

  const rows = useMemo(() => {
    if (!open) return [];
    return Object.entries(layers || {})
      .filter(([, s]) => s.visible)
      .map(([id]) => {
        const fc = view?.[id];
        const feats = Array.isArray(fc?.features) ? fc.features : [];
        const serverTotal = Number(fc?._meta?.total ?? fc?._meta?.total_count ?? fc?._meta?.records);
        const bySource = {};
        let inView = 0;
        for (const f of feats) {
          if (bounds && inBounds(f, bounds)) inView++;
          const s = sourceOf(f);
          if (s) bySource[s] = (bySource[s] || 0) + 1;
        }
        return {
          id,
          name: catalog?.[id]?.name || id,
          color: catalog?.[id]?.color,
          total: feats.length,
          inView,
          serverTotal: Number.isFinite(serverTotal) && serverTotal > feats.length ? serverTotal : null,
          bySource: Object.entries(bySource).sort((a, b) => b[1] - a[1]),
          loading: !!layers[id]?.loading,
          timeWindow: fc?._timeWindow || null,
        };
      })
      .sort((a, b) => b.inView - a.inView);
  }, [open, layers, view, catalog, bounds]);

  if (!open) return null;
  const sumView = rows.reduce((a, r) => a + r.inView, 0);
  const sumTotal = rows.reduce((a, r) => a + r.total, 0);

  return (
    <div className="absolute right-3 top-14 md:top-3 z-40 w-[min(94vw,320px)] glass-panel shadow-xl max-h-[calc(100%-6rem)] flex flex-col">
      <div className="flex items-center justify-between px-3 py-2 border-b border-osint-border/60">
        <div className="font-mono text-[10px] uppercase tracking-[0.12em] text-accent">Feature stats</div>
        <button type="button" onClick={onClose} className="text-osint-muted hover:text-osint-text" aria-label="Close"><LuX size={14} /></button>
      </div>
      <div className="px-3 py-1.5 text-[11px] font-mono text-osint-muted border-b border-osint-border/60">
        in view <span className="text-osint-text">{sumView}</span> · loaded <span className="text-osint-text">{sumTotal}</span> · {rows.length} layer{rows.length === 1 ? '' : 's'}
      </div>
      <div className="overflow-auto flex-1">
        {rows.length === 0 && <div className="px-3 py-4 text-[11px] text-osint-muted">No layer is active.</div>}
        {rows.map((r) => {
          const isOpen = expanded.has(r.id);
          return (
            <div key={r.id} className="border-b border-osint-border/40 last:border-0">
              <button
                type="button"
                onClick={() => setExpanded((s) => { const n = new Set(s); if (n.has(r.id)) n.delete(r.id); else n.add(r.id); return n; })}
                className="w-full flex items-center gap-2 px-3 py-1.5 text-left hover:bg-white/5"
              >
                {r.bySource.length ? (isOpen ? <LuChevronDown size={12} className="text-osint-muted" /> : <LuChevronRight size={12} className="text-osint-muted" />) : <span className="w-3" />}
                <span className="inline-block w-2 h-2 rounded-full flex-shrink-0" style={{ background: r.color || 'rgb(91,231,241)' }} />
                <span className="text-[11px] text-osint-text truncate flex-1">{r.name}</span>
                <span className="font-mono text-[11px] text-osint-text">{r.inView}</span>
                <span className={cx('font-mono text-[10px]', r.serverTotal ? 'text-accent' : 'text-osint-muted')} title={r.serverTotal ? `${r.total} fetched of ${r.serverTotal} on the server` : 'features loaded in this browser'}>
                  / {r.total}{r.serverTotal ? ` of ${r.serverTotal}` : ''}{r.loading ? ' …' : ''}
                </span>
              </button>
              {r.timeWindow && (
                <div className="px-8 pb-1 text-[10px] font-mono text-accent">time window: {r.timeWindow.shown} of {r.timeWindow.of}</div>
              )}
              {isOpen && r.bySource.length > 0 && (
                <ul className="px-8 pb-1.5 space-y-0.5">
                  {r.bySource.map(([s, n]) => (
                    <li key={s} className="flex justify-between text-[10px] font-mono text-osint-muted"><span className="truncate">{s}</span><span>{n}</span></li>
                  ))}
                </ul>
              )}
            </div>
          );
        })}
      </div>
    </div>
  );
}
