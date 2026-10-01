import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { LuSearch, LuX, LuCrosshair, LuEllipsis, LuShare2, LuFootprints, LuMapPin, LuChartBar, LuClock, LuLayers, LuHistory } from 'react-icons/lu';
import { api } from '../../../api/client.js';
import { cx } from '../../ui/kit.jsx';

/**
 * Floating map chrome — the web port of the iOS `MapTab` top bar:
 * layers pill · geocode search (server hits + local feature hits + recent
 * searches, `via_translation` flagged when the server ran bilingual) ·
 * reverse-geocode-the-centre · a More menu (share, travel time, draw AOI,
 * show AOIs, feature stats, time controls).
 *
 * Bilingual note: the iOS bar translates the query on-device (Apple
 * Translation) and sends it as `qAlt`. The browser has no equivalent, so the
 * web sends `q` only and shows the server's `via_translation` flag if it
 * ever sets one.
 */
const RECENT_KEY = 'osint:map:recent-searches';
const LOCAL_HIT_CAP = 8;
const NAME_KEYS = ['name', 'title', 'station_name', 'place', 'name_ja', 'name_en', 'display_name', 'label'];

function readRecent() { try { const v = JSON.parse(window.localStorage.getItem(RECENT_KEY) || '[]'); return Array.isArray(v) ? v : []; } catch { return []; } }
function writeRecent(list) { try { window.localStorage.setItem(RECENT_KEY, JSON.stringify(list.slice(0, 8))); } catch { /* ignore */ } }

function anchorOf(f) {
  const g = f?.geometry;
  if (!g) return null;
  const c = g.coordinates;
  switch (g.type) {
    case 'Point': return c;
    case 'MultiPoint': case 'LineString': return c[0];
    case 'MultiLineString': case 'Polygon': return c[0]?.[0];
    case 'MultiPolygon': return c[0]?.[0]?.[0];
    default: return null;
  }
}

export default function MapTopBar({
  mapRef, layers, layerDataView, catalog, activeCount,
  onOpenIsochrone, onOpenShare, onToggleStats, statsOpen, onStartAoi, aoiLayerOn, onToggleAoiLayer,
  timeOpen, onToggleTime, onReverseGeocode,
}) {
  const [query, setQuery] = useState('');
  const [hits, setHits] = useState([]);
  const [localHits, setLocalHits] = useState({ rows: [], total: 0 });
  const [open, setOpen] = useState(false);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState(null);
  const [recent, setRecent] = useState(readRecent);
  const [menu, setMenu] = useState(false);
  const wrap = useRef(null);

  useEffect(() => {
    const onDoc = (e) => { if (wrap.current && !wrap.current.contains(e.target)) { setOpen(false); setMenu(false); } };
    document.addEventListener('mousedown', onDoc);
    return () => document.removeEventListener('mousedown', onDoc);
  }, []);

  const searchLocal = useCallback((q) => {
    const needle = q.toLowerCase();
    const rows = [];
    let total = 0;
    for (const [id, st] of Object.entries(layers || {})) {
      if (!st.visible) continue;
      const feats = layerDataView?.[id]?.features;
      if (!Array.isArray(feats)) continue;
      for (const f of feats) {
        const p = f?.properties || {};
        const name = NAME_KEYS.map((k) => p[k]).find((v) => typeof v === 'string' && v.toLowerCase().includes(needle));
        if (!name) continue;
        total++;
        if (rows.length < LOCAL_HIT_CAP) {
          const a = anchorOf(f);
          if (a) rows.push({ id: `${id}:${p.id ?? total}`, layerId: id, layerName: catalog?.[id]?.name || id, name, lon: a[0], lat: a[1] });
        }
      }
    }
    return { rows, total };
  }, [layers, layerDataView, catalog]);

  const run = useCallback(async (q) => {
    const trimmed = (q ?? query).trim();
    if (!trimmed) return;
    setOpen(true); setBusy(true); setError(null);
    setLocalHits(searchLocal(trimmed));
    try {
      const j = await api.get('/api/geocode', { query: { q: trimmed } });
      setHits(Array.isArray(j?.results) ? j.results : []);
    } catch (e) {
      // A failed lookup must not leave a previous query's hits under this one.
      setHits([]); setError(e);
    } finally { setBusy(false); }
    const next = [trimmed, ...recent.filter((r) => r !== trimmed)];
    setRecent(next); writeRecent(next);
  }, [query, recent, searchLocal]);

  const fly = (lat, lon, zoom = 13) => {
    if (Number.isFinite(lat) && Number.isFinite(lon)) {
      window.dispatchEvent(new CustomEvent('japanosint:flyto', { detail: { lat, lon, zoom } }));
    }
    setOpen(false);
  };

  const clear = () => { setQuery(''); setHits([]); setLocalHits({ rows: [], total: 0 }); setError(null); setOpen(false); };

  const showDropdown = open && (busy || error || hits.length || localHits.rows.length || (!query && recent.length));
  const menuItems = useMemo(() => [
    { icon: LuShare2, label: 'Share this view', onClick: onOpenShare },
    { icon: LuFootprints, label: 'Travel time from…', onClick: onOpenIsochrone },
    { icon: LuMapPin, label: 'Draw area of interest', onClick: onStartAoi },
    { icon: LuMapPin, label: aoiLayerOn ? 'Hide saved areas' : 'Show saved areas', onClick: onToggleAoiLayer, active: aoiLayerOn },
    { icon: LuChartBar, label: statsOpen ? 'Hide feature stats' : 'Feature stats', onClick: onToggleStats, active: statsOpen },
    { icon: LuClock, label: timeOpen ? 'Hide time controls' : 'Time controls', onClick: onToggleTime, active: timeOpen },
  ], [onOpenShare, onOpenIsochrone, onStartAoi, onToggleAoiLayer, aoiLayerOn, onToggleStats, statsOpen, onToggleTime, timeOpen]);

  return (
    // Sits to the right of the layer panel (16rem + its toggle) and clear of
    // the basemap switcher + zoom stack on the right, at every width.
    <div ref={wrap} className="absolute top-3 left-12 right-3 md:left-[18.5rem] md:right-[15.5rem] lg:right-auto lg:w-[min(560px,calc(100%-34rem))] z-30">
      <div className="flex items-center gap-2 glass-panel rounded-[18px] px-2 py-1.5 shadow-lg">
        <span className="hidden sm:inline-flex items-center gap-1 rounded-full border border-accent/40 bg-accent/10 px-2 py-0.5 text-[10px] font-mono text-accent" title="Active layers (panel on the left)">
          <LuLayers size={11} /> {activeCount}
        </span>
        <form onSubmit={(e) => { e.preventDefault(); run(); }} className="relative flex-1 min-w-0">
          <input
            type="text"
            value={query}
            onChange={(e) => { setQuery(e.target.value); setOpen(true); }}
            onFocus={() => setOpen(true)}
            placeholder="Search location in Japan…"
            className="w-full pl-3 pr-14 py-1.5 bg-osint-bg/70 border border-osint-border rounded-full text-sm text-osint-text placeholder:text-osint-muted/70 focus:outline-none focus:border-accent/60 font-mono"
            aria-label="Search location"
          />
          <div className="absolute right-1.5 top-1/2 -translate-y-1/2 flex items-center gap-0.5">
            {query && <button type="button" onClick={clear} className="p-1 text-osint-muted hover:text-osint-text" aria-label="Clear search"><LuX size={13} /></button>}
            <button type="submit" className="p-1 text-osint-muted hover:text-accent" aria-label="Search">{busy ? '…' : <LuSearch size={14} />}</button>
          </div>
        </form>
        <button type="button" onClick={onReverseGeocode} className="p-1.5 rounded-full text-osint-muted hover:text-accent" title="What is at the map centre? (reverse geocode)" aria-label="Reverse-geocode the map centre"><LuCrosshair size={15} /></button>
        <div className="relative">
          <button type="button" onClick={() => setMenu((v) => !v)} className={cx('p-1.5 rounded-full hover:text-accent', menu ? 'text-accent' : 'text-osint-muted')} aria-label="More map options" aria-expanded={menu}><LuEllipsis size={16} /></button>
          {menu && (
            <div className="absolute right-0 mt-1 w-56 glass-panel py-1 shadow-xl z-50">
              {menuItems.map((m) => (
                <button key={m.label} type="button" onClick={() => { setMenu(false); m.onClick?.(); }} className={cx('w-full flex items-center gap-2 px-3 py-1.5 text-left text-xs hover:bg-white/5', m.active ? 'text-accent' : 'text-osint-text')}>
                  <m.icon size={13} className="text-osint-muted" /> {m.label}
                </button>
              ))}
            </div>
          )}
        </div>
      </div>

      {showDropdown && (
        <div className="mt-1 glass-panel overflow-hidden max-h-[60vh] overflow-y-auto shadow-xl">
          {error && (
            <div className="px-3 py-2 text-xs text-neon-red">Location search failed ({error.message}) — no results were obtained.</div>
          )}
          {!query && recent.length > 0 && (
            <>
              <Header><LuHistory size={11} /> Recent</Header>
              {recent.map((r) => (
                <button key={r} type="button" onClick={() => { setQuery(r); run(r); }} className="w-full text-left px-3 py-1.5 text-xs text-osint-text hover:bg-accent/10 font-mono">{r}</button>
              ))}
            </>
          )}
          {localHits.rows.length > 0 && (
            <>
              <Header>On the map · {localHits.total > localHits.rows.length ? `showing ${localHits.rows.length} of ${localHits.total}` : localHits.total}</Header>
              {localHits.rows.map((h) => (
                <button key={h.id} type="button" onClick={() => fly(h.lat, h.lon, 15)} className="w-full text-left px-3 py-1.5 text-xs hover:bg-accent/10 border-b border-osint-border/40 last:border-0">
                  <div className="text-osint-text truncate">{h.name}</div>
                  <div className="text-[10px] font-mono text-osint-muted">{h.layerName} · {h.lat.toFixed(4)}, {h.lon.toFixed(4)}</div>
                </button>
              ))}
            </>
          )}
          {hits.length > 0 && (
            <>
              <Header>Places</Header>
              {hits.map((r) => {
                const lat = parseFloat(r.lat), lon = parseFloat(r.lon);
                return (
                  <button key={`${r.lat},${r.lon}|${r.display_name}`} type="button" onClick={() => { setQuery(String(r.display_name || '').split(',')[0]); fly(lat, lon, 13); }} className="w-full text-left px-3 py-1.5 text-xs hover:bg-accent/10 border-b border-osint-border/40 last:border-0">
                    <div className="text-osint-text truncate">{r.display_name || '—'}{r.via_translation && <span className="ml-1 text-[9px] font-mono text-accent uppercase">via translation</span>}</div>
                    <div className="text-[10px] font-mono text-osint-muted">{Number.isFinite(lat) ? `${lat.toFixed(4)}, ${lon.toFixed(4)}` : ''}{r.type ? ` · ${r.type}` : ''}{r.source ? ` · ${r.source}` : ''}</div>
                  </button>
                );
              })}
            </>
          )}
          {!busy && !error && query && hits.length === 0 && localHits.rows.length === 0 && (
            <div className="px-3 py-2 text-xs text-osint-muted">No place or loaded feature matched “{query}”.</div>
          )}
        </div>
      )}
    </div>
  );
}

function Header({ children }) {
  return <div className="px-3 pt-2 pb-1 flex items-center gap-1 font-mono text-[9px] uppercase tracking-[0.12em] text-osint-muted">{children}</div>;
}
