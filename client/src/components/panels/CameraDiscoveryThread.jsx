import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import apiUrl from '../../utils/apiUrl.js';
import {
  MdInfoOutline,
  MdMyLocation,
  MdStar,
  MdStarBorder,
  MdOpenInNew,
  MdSearch,
  MdMap,
  MdFilterList,
  MdClose,
} from 'react-icons/md';
import useCameraDiscoveryStream from '../../hooks/useCameraDiscoveryStream';

const FAV_STORAGE_KEY = 'japanosint.cameraFavorites';

// ── Favorites ──────────────────────────────────────────────────────────────
function loadFavorites() {
  try {
    const raw = localStorage.getItem(FAV_STORAGE_KEY);
    if (!raw) return new Set();
    const arr = JSON.parse(raw);
    return new Set(Array.isArray(arr) ? arr : []);
  } catch {
    return new Set();
  }
}

function saveFavorites(set) {
  try {
    localStorage.setItem(FAV_STORAGE_KEY, JSON.stringify([...set]));
  } catch { /* quota / private mode */ }
}

function useCameraFavorites() {
  const [favs, setFavs] = useState(() => loadFavorites());

  const toggle = useCallback((uid) => {
    if (!uid) return;
    setFavs((prev) => {
      const next = new Set(prev);
      if (next.has(uid)) next.delete(uid);
      else next.add(uid);
      saveFavorites(next);
      return next;
    });
  }, []);

  useEffect(() => {
    const onStorage = (e) => {
      if (e.key === FAV_STORAGE_KEY) setFavs(loadFavorites());
    };
    window.addEventListener('storage', onStorage);
    return () => window.removeEventListener('storage', onStorage);
  }, []);

  return { favs, toggle };
}

// ── Address (reverse-geocoded) ─────────────────────────────────────────────
// Module-level cache so re-renders / list reorders don't refetch. Keys are
// rounded `${lat},${lon}` to ~1m precision; matches the server's cache key.
// Bounded by ADDR_CACHE_MAX entries with ADDR_CACHE_TTL_MS expiry — a long
// camera-discovery session would otherwise grow this Map indefinitely.
const ADDR_CACHE_MAX = 500;
const ADDR_CACHE_TTL_MS = 30 * 60 * 1000;
const addressCache = new Map();       // key -> { value, ts }   value: 'pending' | null | object
const addressSubscribers = new Map(); // key -> Set<() => void>

function notifyAddress(key) {
  const subs = addressSubscribers.get(key);
  if (subs) for (const fn of subs) fn();
}

/// Returns the cached value (matching the legacy three-state contract:
/// `undefined` for miss, `'pending'`, `null` for failed, or the data object).
/// Drops expired entries on access so a stale popup doesn't keep showing
/// "resolving…" indefinitely.
function getCachedAddress(key) {
  const entry = addressCache.get(key);
  if (!entry) return undefined;
  if (entry.value !== 'pending' && Date.now() - entry.ts > ADDR_CACHE_TTL_MS) {
    addressCache.delete(key);
    addressSubscribers.delete(key);
    return undefined;
  }
  return entry.value;
}

function setCachedAddress(key, value) {
  // Re-insert (delete + set) keeps Map iteration order = LRU. When at
  // capacity, drop the oldest insertion (Map iterators yield in insertion
  // order; the first key returned is the least-recently-written).
  if (addressCache.has(key)) addressCache.delete(key);
  addressCache.set(key, { value, ts: Date.now() });
  while (addressCache.size > ADDR_CACHE_MAX) {
    const oldest = addressCache.keys().next().value;
    if (oldest === undefined) break;
    addressCache.delete(oldest);
    addressSubscribers.delete(oldest);
  }
}

function fetchAddress(key, lat, lon) {
  if (getCachedAddress(key) !== undefined) return;
  setCachedAddress(key, 'pending');
  fetch(apiUrl(`/api/geocode/reverse?lat=${lat}&lon=${lon}`))
    .then((r) => (r.ok ? r.json() : null))
    .then((data) => {
      if (!data) {
        setCachedAddress(key, null);
      } else {
        setCachedAddress(key, {
          ja: data.display_name_ja || null,
          en: data.display_name_en || null,
          full: data.display_name || null,
          address: data.address || null,
          source: data.source || null,
        });
      }
    })
    .catch(() => setCachedAddress(key, null))
    .finally(() => notifyAddress(key));
}

function useAddress(lat, lon) {
  const key = useMemo(() => {
    if (!Number.isFinite(lat) || !Number.isFinite(lon)) return null;
    return `${lat.toFixed(5)},${lon.toFixed(5)}`;
  }, [lat, lon]);
  const [, setTick] = useState(0);

  useEffect(() => {
    if (!key) return undefined;
    fetchAddress(key, lat, lon);
    const subs = addressSubscribers.get(key) || new Set();
    const listener = () => setTick((t) => t + 1);
    subs.add(listener);
    addressSubscribers.set(key, subs);
    return () => {
      subs.delete(listener);
      if (subs.size === 0) addressSubscribers.delete(key);
    };
  }, [key, lat, lon]);

  if (!key) return { state: 'idle', data: null };
  const cached = getCachedAddress(key);
  if (cached === undefined || cached === 'pending') return { state: 'loading', data: null };
  if (cached === null) return { state: 'failed', data: null };
  return { state: 'ok', data: cached };
}

function CardAddress({ lat, lon }) {
  const { state, data } = useAddress(lat, lon);
  if (state === 'loading') {
    return <div className="text-[9px] text-gray-600 italic mt-0.5">resolving address…</div>;
  }
  if (state !== 'ok' || !data) return null;
  const ja = data.ja;
  const en = data.en;
  if (!ja && !en) return null;
  return (
    <div className="mt-0.5 flex flex-col gap-0.5">
      {ja && (
        <span className="text-[10px] text-gray-300 leading-tight" title={ja}>
          {ja}
        </span>
      )}
      {en && (
        <span className="text-[10px] text-gray-400 leading-tight" title={en}>
          {en}
        </span>
      )}
    </div>
  );
}

// ── Map dispatchers ────────────────────────────────────────────────────────
function pinCameraOnMap(camera) {
  window.dispatchEvent(
    new CustomEvent('japanosint:pin-camera', { detail: { camera } }),
  );
}

function openCameraPopup(camera) {
  window.dispatchEvent(
    new CustomEvent('japanosint:open-camera-popup', { detail: { camera } }),
  );
}

function pinCamerasOnMap(cameras) {
  window.dispatchEvent(
    new CustomEvent('japanosint:pin-cameras', { detail: { cameras } }),
  );
}

function showCamerasLayer() {
  window.dispatchEvent(new CustomEvent('japanosint:show-cameras-layer'));
}

function pickPointFromMap() {
  return new Promise((resolve) => {
    const handler = (e) => {
      window.removeEventListener('japanosint:pick-point-result', handler);
      resolve(e.detail || null);
    };
    window.addEventListener('japanosint:pick-point-result', handler);
    window.dispatchEvent(new CustomEvent('japanosint:pick-point'));
  });
}

// ── Helpers ────────────────────────────────────────────────────────────────
function fmtElapsed(ms) {
  if (!Number.isFinite(ms)) return '—';
  const s = Math.floor(ms / 1000);
  const mm = Math.floor(s / 60).toString().padStart(2, '0');
  const ss = (s % 60).toString().padStart(2, '0');
  return `${mm}:${ss}`;
}

function haversineKm(lat1, lon1, lat2, lon2) {
  const R = 6371;
  const toRad = (d) => (d * Math.PI) / 180;
  const dLat = toRad(lat2 - lat1);
  const dLon = toRad(lon2 - lon1);
  const a =
    Math.sin(dLat / 2) ** 2 +
    Math.cos(toRad(lat1)) * Math.cos(toRad(lat2)) * Math.sin(dLon / 2) ** 2;
  return 2 * R * Math.asin(Math.sqrt(a));
}

function eventLatLon(ev) {
  const c = ev?.camera?.geometry?.coordinates;
  if (!Array.isArray(c)) return null;
  const [lon, lat] = c;
  if (!Number.isFinite(lat) || !Number.isFinite(lon)) return null;
  return { lat, lon };
}

function eventCity(ev) {
  const p = ev?.camera?.properties || {};
  if (p.city) return p.city;
  const ll = eventLatLon(ev);
  if (!ll) return null;
  const cached = addressCache.get(`${ll.lat.toFixed(5)},${ll.lon.toFixed(5)}`);
  if (cached && cached !== 'pending' && typeof cached === 'object') {
    return cached.address?.city || cached.address?.town || cached.address?.village || null;
  }
  return null;
}

// ── Small UI atoms ─────────────────────────────────────────────────────────
function KindBadge({ kind, isLive }) {
  if (isLive) {
    return (
      <span className="px-1.5 py-0.5 rounded text-[9px] font-mono bg-status-online/15 text-status-online border border-status-online/30">
        NEW
      </span>
    );
  }
  if (kind === 'historical') {
    return (
      <span className="px-1.5 py-0.5 rounded text-[9px] font-mono bg-osint-bg/60 text-gray-500 border border-osint-border/40">
        HIST
      </span>
    );
  }
  if (kind === 'new') {
    return (
      <span className="px-1.5 py-0.5 rounded text-[9px] font-mono bg-status-online/15 text-status-online border border-status-online/30">
        NEW
      </span>
    );
  }
  return (
    <span className="px-1.5 py-0.5 rounded text-[9px] font-mono bg-neon-cyan/10 text-neon-cyan border border-neon-cyan/30">
      UPDATED
    </span>
  );
}

function ChannelChip({ channel }) {
  return (
    <span className="px-1.5 py-0.5 rounded text-[9px] font-mono bg-osint-bg/80 text-gray-300 border border-osint-border/60">
      {channel}
    </span>
  );
}

function IconButton({ title, onClick, active, children, href, disabled }) {
  const baseClasses =
    'inline-flex items-center justify-center w-7 h-7 rounded border transition-colors flex-shrink-0';
  const stateClasses = disabled
    ? 'bg-osint-bg/30 border-osint-border/30 text-gray-600 cursor-not-allowed'
    : active
      ? 'bg-osint-border/60 border-osint-border text-neon-cyan'
      : 'bg-osint-bg/60 border-osint-border/60 text-gray-300 hover:bg-osint-border/50 hover:text-gray-100';
  if (href) {
    return (
      <a
        href={href}
        target="_blank"
        rel="noreferrer"
        title={title}
        aria-label={title}
        onClick={(e) => e.stopPropagation()}
        className={`${baseClasses} ${stateClasses}`}
      >
        {children}
      </a>
    );
  }
  return (
    <button
      type="button"
      onClick={(e) => {
        e.stopPropagation();
        if (!disabled && onClick) onClick(e);
      }}
      title={title}
      aria-label={title}
      aria-pressed={active ? 'true' : undefined}
      disabled={disabled}
      className={`${baseClasses} ${stateClasses}`}
    >
      {children}
    </button>
  );
}

function FilterChip({ label, active, onClick, count }) {
  return (
    <button
      type="button"
      onClick={onClick}
      className={`px-1.5 py-0.5 rounded text-[10px] font-mono border transition-colors ${
        active
          ? 'bg-neon-cyan/15 text-neon-cyan border-neon-cyan/40'
          : 'bg-osint-bg/60 text-gray-400 border-osint-border/60 hover:text-gray-200'
      }`}
    >
      {label}{Number.isFinite(count) ? <span className="opacity-70 ml-1">{count}</span> : null}
    </button>
  );
}

// ── Event card ─────────────────────────────────────────────────────────────
function EventRow({ ev, isFavorite, onToggleFavorite, onCardClick }) {
  const camera = ev.camera;
  const p = camera?.properties || {};
  const coords = camera?.geometry?.coordinates || [];
  const [lon, lat] = coords;
  const thumb = p.thumbnail_url;
  const uid = p.camera_uid || p.camera_id || null;
  const sourceUrl = p.url || p.stream_url;
  const hasCoords = Number.isFinite(lat) && Number.isFinite(lon);

  return (
    <div
      role="button"
      tabIndex={0}
      onClick={() => onCardClick(camera)}
      onKeyDown={(e) => {
        if (e.key === 'Enter' || e.key === ' ') {
          e.preventDefault();
          onCardClick(camera);
        }
      }}
      className="border border-osint-border/40 rounded-md bg-osint-bg/40 p-2 cursor-pointer hover:bg-osint-bg/60 hover:border-osint-border/70 transition-colors text-left"
    >
      <div className="flex gap-2">
        {thumb ? (
          <img
            src={thumb}
            alt=""
            className="w-14 h-14 object-cover rounded border border-osint-border/40 flex-shrink-0"
            loading="lazy"
            onError={(e) => { e.currentTarget.style.display = 'none'; }}
          />
        ) : (
          <div className="w-14 h-14 rounded border border-osint-border/40 bg-osint-bg/70 flex-shrink-0" />
        )}
        <div className="flex-1 min-w-0 flex flex-col">
          <div className="flex items-center gap-1 flex-wrap">
            <KindBadge kind={ev.kind} isLive={ev.isLive} />
            <ChannelChip channel={ev.channel} />
            <span className="text-[9px] text-gray-500 font-mono ml-auto">
              {new Date(ev.ts).toLocaleTimeString('en-GB')}
            </span>
          </div>
          <div className="text-[11px] text-gray-200 truncate mt-1" title={p.name}>
            {p.name || 'Unknown camera'}
          </div>
          <div className="text-[9px] text-gray-500 font-mono mt-0.5">
            {hasCoords ? `${lat.toFixed(4)}, ${lon.toFixed(4)}` : '— no coordinates —'}
          </div>
          {hasCoords && <CardAddress lat={lat} lon={lon} />}
        </div>
      </div>
      <div className="flex items-center justify-end gap-1.5 mt-1.5">
        <IconButton
          title={isFavorite ? 'Remove from favorites' : 'Add to favorites'}
          active={isFavorite}
          onClick={() => onToggleFavorite(uid)}
        >
          {isFavorite ? <MdStar size={14} /> : <MdStarBorder size={14} />}
        </IconButton>
        <IconButton
          title={hasCoords ? 'Pin on map' : 'No coordinates available'}
          disabled={!hasCoords}
          onClick={() => pinCameraOnMap(camera)}
        >
          <MdMyLocation size={14} />
        </IconButton>
        {sourceUrl ? (
          <IconButton title="Open source" href={sourceUrl}>
            <MdOpenInNew size={14} />
          </IconButton>
        ) : null}
      </div>
    </div>
  );
}

function RunBanner({ activeRun, lastRun }) {
  if (activeRun) {
    const channelsDone = activeRun.channels_done?.length ?? 0;
    return (
      <div className="px-3 py-2 border-b border-osint-border/40 bg-neon-cyan/5">
        <div className="flex items-center justify-between text-[10px]">
          <div className="flex items-center gap-2">
            <span className="w-2 h-2 rounded-full bg-status-online pulse-live" />
            <span className="text-neon-cyan font-mono">RUN ACTIVE</span>
            <span className="text-gray-400 font-mono">{activeRun.run_id.slice(11, 19)}</span>
          </div>
          <span className="font-mono text-gray-300">{fmtElapsed(activeRun.elapsed_ms)}</span>
        </div>
        <div className="mt-1 text-[10px] text-gray-400 font-mono flex flex-wrap gap-x-3 gap-y-0.5">
          <span>
            <span className="text-status-online">{activeRun.new_count}</span> new
          </span>
          <span>
            <span className="text-neon-cyan">{activeRun.updated_count}</span> updated
          </span>
          <span>{channelsDone}/15 channels done</span>
        </div>
      </div>
    );
  }
  if (lastRun) {
    return (
      <div className="px-3 py-2 border-b border-osint-border/40">
        <div className="text-[10px] text-gray-400 font-mono flex flex-wrap gap-x-3">
          <span className="text-gray-500">Last run</span>
          <span>
            <span className="text-status-online">+{lastRun.new_count}</span> new,{' '}
            <span className="text-neon-cyan">{lastRun.updated_count}</span> updated
          </span>
          <span>in {fmtElapsed(lastRun.elapsed_ms)}</span>
          <span className="text-gray-500">DB total {lastRun.db_total}</span>
        </div>
      </div>
    );
  }
  return (
    <div className="px-3 py-2 border-b border-osint-border/40 text-[10px] text-gray-500 italic">
      Waiting for first camera run…
    </div>
  );
}

// ── Filter bar ─────────────────────────────────────────────────────────────
function DiscoveryFilterBar({
  events,
  query, setQuery,
  channels, toggleChannel,
  types, toggleType,
  status, setStatus,
  favoritesOnly, setFavoritesOnly,
  operators, toggleOperator,
  cities, toggleCity,
  distance, setDistance,
  showFilters, setShowFilters,
  onPickDistanceCenter,
}) {
  // Build option lists from current events.
  const channelOpts = useMemo(() => {
    const m = new Map();
    for (const e of events) {
      const c = e.channel || 'unknown';
      m.set(c, (m.get(c) || 0) + 1);
    }
    return [...m.entries()].sort((a, b) => b[1] - a[1]);
  }, [events]);

  const typeOpts = useMemo(() => {
    const m = new Map();
    for (const e of events) {
      const t = e.camera?.properties?.camera_type || e.camera?.properties?.type || 'other';
      m.set(t, (m.get(t) || 0) + 1);
    }
    return [...m.entries()].sort((a, b) => b[1] - a[1]);
  }, [events]);

  const operatorOpts = useMemo(() => {
    const m = new Map();
    for (const e of events) {
      const op = e.camera?.properties?.operator;
      if (!op) continue;
      m.set(op, (m.get(op) || 0) + 1);
    }
    return [...m.entries()].sort((a, b) => b[1] - a[1]);
  }, [events]);

  const cityOpts = useMemo(() => {
    const m = new Map();
    for (const e of events) {
      const c = eventCity(e);
      if (!c) continue;
      m.set(c, (m.get(c) || 0) + 1);
    }
    return [...m.entries()].sort((a, b) => b[1] - a[1]);
  }, [events]);

  return (
    <div className="border-b border-osint-border/40 bg-osint-bg/30">
      {/* Always-visible: search + filter toggle */}
      <div className="flex items-center gap-1.5 px-3 py-1.5">
        <div className="relative flex-1">
          <MdSearch
            size={14}
            className="absolute left-1.5 top-1/2 -translate-y-1/2 text-gray-500 pointer-events-none"
          />
          <input
            type="text"
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            placeholder="Search name, channel, operator…"
            className="w-full pl-6 pr-6 py-1 bg-osint-bg/70 border border-osint-border/60 rounded text-[11px] text-gray-200 placeholder-gray-600 font-mono focus:outline-none focus:border-neon-cyan/40"
          />
          {query && (
            <button
              type="button"
              onClick={() => setQuery('')}
              className="absolute right-1.5 top-1/2 -translate-y-1/2 text-gray-500 hover:text-gray-200"
              aria-label="Clear search"
            >
              <MdClose size={12} />
            </button>
          )}
        </div>
        <button
          type="button"
          onClick={() => setShowFilters((v) => !v)}
          title="Filters"
          aria-label="Toggle filters"
          aria-pressed={showFilters ? 'true' : 'false'}
          className={`inline-flex items-center justify-center w-7 h-7 rounded border ${
            showFilters
              ? 'bg-osint-border/60 border-osint-border text-neon-cyan'
              : 'bg-osint-bg/60 border-osint-border/60 text-gray-300 hover:bg-osint-border/50'
          }`}
        >
          <MdFilterList size={14} />
        </button>
      </div>

      {showFilters && (
        <div className="px-3 pb-2 space-y-1.5 text-[10px]">
          {/* Status segmented */}
          <div className="flex items-center gap-1.5">
            <span className="text-gray-500 uppercase tracking-wider w-16 flex-shrink-0">Status</span>
            <div className="flex gap-1">
              {['all', 'new', 'updated'].map((s) => (
                <FilterChip
                  key={s}
                  label={s.toUpperCase()}
                  active={status === s}
                  onClick={() => setStatus(s)}
                />
              ))}
            </div>
            <FilterChip
              label="★ FAV"
              active={favoritesOnly}
              onClick={() => setFavoritesOnly(!favoritesOnly)}
            />
          </div>

          {channelOpts.length >= 2 && (
            <div className="flex items-start gap-1.5">
              <span className="text-gray-500 uppercase tracking-wider w-16 flex-shrink-0 mt-0.5">Channel</span>
              <div className="flex flex-wrap gap-1">
                {channelOpts.map(([ch, n]) => (
                  <FilterChip
                    key={ch}
                    label={ch}
                    count={n}
                    active={channels.has(ch)}
                    onClick={() => toggleChannel(ch)}
                  />
                ))}
              </div>
            </div>
          )}

          {typeOpts.length >= 2 && (
            <div className="flex items-start gap-1.5">
              <span className="text-gray-500 uppercase tracking-wider w-16 flex-shrink-0 mt-0.5">Type</span>
              <div className="flex flex-wrap gap-1">
                {typeOpts.map(([t, n]) => (
                  <FilterChip
                    key={t}
                    label={t}
                    count={n}
                    active={types.has(t)}
                    onClick={() => toggleType(t)}
                  />
                ))}
              </div>
            </div>
          )}

          {operatorOpts.length >= 2 && (
            <div className="flex items-start gap-1.5">
              <span className="text-gray-500 uppercase tracking-wider w-16 flex-shrink-0 mt-0.5">Operator</span>
              <div className="flex flex-wrap gap-1">
                {operatorOpts.slice(0, 12).map(([op, n]) => (
                  <FilterChip
                    key={op}
                    label={op.length > 18 ? `${op.slice(0, 17)}…` : op}
                    count={n}
                    active={operators.has(op)}
                    onClick={() => toggleOperator(op)}
                  />
                ))}
              </div>
            </div>
          )}

          {cityOpts.length >= 2 && (
            <div className="flex items-start gap-1.5">
              <span className="text-gray-500 uppercase tracking-wider w-16 flex-shrink-0 mt-0.5">City</span>
              <div className="flex flex-wrap gap-1">
                {cityOpts.slice(0, 12).map(([c, n]) => (
                  <FilterChip
                    key={c}
                    label={c.length > 16 ? `${c.slice(0, 15)}…` : c}
                    count={n}
                    active={cities.has(c)}
                    onClick={() => toggleCity(c)}
                  />
                ))}
              </div>
            </div>
          )}

          {/* Distance filter */}
          <div className="flex items-center flex-wrap gap-1.5 pt-1 border-t border-osint-border/30">
            <span className="text-gray-500 uppercase tracking-wider w-16 flex-shrink-0">Distance</span>
            <FilterChip
              label={distance.enabled ? 'ON' : 'OFF'}
              active={distance.enabled}
              onClick={() =>
                setDistance((d) => ({ ...d, enabled: !d.enabled }))
              }
            />
            {distance.enabled && (
              <>
                <button
                  type="button"
                  onClick={onPickDistanceCenter}
                  className="px-1.5 py-0.5 rounded text-[10px] font-mono border bg-osint-bg/60 text-gray-300 border-osint-border/60 hover:bg-osint-border/50"
                >
                  {distance.center
                    ? `${distance.center.lat.toFixed(3)}, ${distance.center.lon.toFixed(3)}`
                    : 'Pick point on map'}
                </button>
                <label className="flex items-center gap-1 text-gray-400 font-mono">
                  <span>{distance.radiusKm} km</span>
                  <input
                    type="range"
                    min="1"
                    max="500"
                    step="1"
                    value={distance.radiusKm}
                    onChange={(e) =>
                      setDistance((d) => ({ ...d, radiusKm: Number(e.target.value) }))
                    }
                    className="w-24"
                  />
                </label>
              </>
            )}
          </div>
        </div>
      )}
    </div>
  );
}

// ── Main ───────────────────────────────────────────────────────────────────
export default function CameraDiscoveryThread() {
  const { events, activeRun, lastRun, connected, clearEvents, loadMore, hasMore, loadingMore } = useCameraDiscoveryStream();
  const { favs, toggle: toggleFavorite } = useCameraFavorites();

  // Filter state
  const [query, setQuery] = useState('');
  const [channels, setChannels] = useState(() => new Set());
  const [types, setTypes] = useState(() => new Set());
  const [status, setStatus] = useState('all'); // all | new | updated
  const [favoritesOnly, setFavoritesOnly] = useState(false);
  const [operators, setOperators] = useState(() => new Set());
  const [cities, setCities] = useState(() => new Set());
  const [distance, setDistance] = useState({ enabled: false, center: null, radiusKm: 25 });
  const [showFilters, setShowFilters] = useState(false);

  // Toggle helpers — all use the same shape.
  const toggleSet = useCallback((setter) => (v) => {
    setter((prev) => {
      const next = new Set(prev);
      if (next.has(v)) next.delete(v);
      else next.add(v);
      return next;
    });
  }, []);
  const toggleChannel = useMemo(() => toggleSet(setChannels), [toggleSet]);
  const toggleType = useMemo(() => toggleSet(setTypes), [toggleSet]);
  const toggleOperator = useMemo(() => toggleSet(setOperators), [toggleSet]);
  const toggleCity = useMemo(() => toggleSet(setCities), [toggleSet]);

  const onPickDistanceCenter = useCallback(async () => {
    const pt = await pickPointFromMap();
    if (pt && Number.isFinite(pt.lat) && Number.isFinite(pt.lon)) {
      setDistance((d) => ({ ...d, enabled: true, center: { lat: pt.lat, lon: pt.lon } }));
    }
  }, []);

  // Filtering pipeline.
  const filteredEvents = useMemo(() => {
    const q = query.trim().toLowerCase();
    return events.filter((ev) => {
      if (status !== 'all' && ev.kind !== status) return false;
      const p = ev.camera?.properties || {};
      const uid = p.camera_uid || p.camera_id;
      if (favoritesOnly && (!uid || !favs.has(uid))) return false;
      if (channels.size > 0 && !channels.has(ev.channel)) return false;
      if (types.size > 0) {
        const t = p.camera_type || p.type || 'other';
        if (!types.has(t)) return false;
      }
      if (operators.size > 0) {
        if (!p.operator || !operators.has(p.operator)) return false;
      }
      if (cities.size > 0) {
        const city = eventCity(ev);
        if (!city || !cities.has(city)) return false;
      }
      if (distance.enabled && distance.center) {
        const ll = eventLatLon(ev);
        if (!ll) return false;
        const km = haversineKm(distance.center.lat, distance.center.lon, ll.lat, ll.lon);
        if (km > distance.radiusKm) return false;
      }
      if (q) {
        const hay = [
          p.name, p.operator, p.camera_type, p.type, ev.channel, p.url, p.stream_url, p.city,
        ]
          .filter(Boolean)
          .join(' ')
          .toLowerCase();
        if (!hay.includes(q)) return false;
      }
      return true;
    });
  }, [events, query, status, favoritesOnly, favs, channels, types, operators, cities, distance]);

  const anyFilterActive =
    !!query ||
    status !== 'all' ||
    favoritesOnly ||
    channels.size > 0 ||
    types.size > 0 ||
    operators.size > 0 ||
    cities.size > 0 ||
    (distance.enabled && distance.center);

  const onCardClick = useCallback((camera) => {
    openCameraPopup(camera);
  }, []);

  const onViewAllOnMap = useCallback(() => {
    if (anyFilterActive) {
      const cams = filteredEvents.map((ev) => ev.camera).filter(Boolean);
      if (cams.length > 0) pinCamerasOnMap(cams);
    } else {
      showCamerasLayer();
    }
  }, [anyFilterActive, filteredEvents]);

  const header = useMemo(() => {
    const run = activeRun;
    if (!run) return null;
    const entries = Object.entries(run.run_counts || {});
    entries.sort((a, b) => b[1] - a[1]);
    return entries;
  }, [activeRun]);

  return (
    <div className="flex flex-col h-full">
      <div className="px-3 py-2 border-b border-osint-border/40 flex items-start gap-2 text-xs text-gray-500">
        <MdInfoOutline className="flex-shrink-0 mt-0.5 w-4 h-4" />
        <div>
          Camera Discovery — live stream of cameras being found by the discovery collector. Each row is a single camera either newly added to the DB (NEW) or re-confirmed (UPDATED). Runs are triggered hourly by the scheduler; toggle the Cameras layer on the map to force a run on demand.
        </div>
      </div>
      <RunBanner activeRun={activeRun} lastRun={lastRun} />

      {header && header.length > 0 && (
        <div className="px-3 py-1.5 border-b border-osint-border/40 flex flex-wrap gap-1">
          {header.map(([ch, n]) => (
            <span
              key={ch}
              className="px-1.5 py-0.5 rounded text-[9px] font-mono bg-osint-bg/60 text-gray-300 border border-osint-border/50"
              title={ch}
            >
              {ch}: <span className="text-neon-green">{n}</span>
            </span>
          ))}
        </div>
      )}

      <DiscoveryFilterBar
        events={events}
        query={query} setQuery={setQuery}
        channels={channels} toggleChannel={toggleChannel}
        types={types} toggleType={toggleType}
        status={status} setStatus={setStatus}
        favoritesOnly={favoritesOnly} setFavoritesOnly={setFavoritesOnly}
        operators={operators} toggleOperator={toggleOperator}
        cities={cities} toggleCity={toggleCity}
        distance={distance} setDistance={setDistance}
        showFilters={showFilters} setShowFilters={setShowFilters}
        onPickDistanceCenter={onPickDistanceCenter}
      />

      <div className="flex items-center justify-between px-3 py-1.5 border-b border-osint-border/40 text-[10px] gap-2">
        <span className="text-gray-500 truncate">
          {anyFilterActive
            ? <>Showing <span className="text-gray-200">{filteredEvents.length}</span> of {events.length}</>
            : <>{events.length} camera{events.length === 1 ? '' : 's'}</>}
          {!connected && <span className="ml-2 text-status-offline">· WS offline</span>}
        </span>
        <div className="flex items-center gap-1.5">
          <button
            type="button"
            onClick={onViewAllOnMap}
            disabled={anyFilterActive ? filteredEvents.length === 0 : false}
            title={anyFilterActive ? 'Pin filtered cameras on the map' : 'Show Cameras layer on the map'}
            className="inline-flex items-center gap-1 px-1.5 py-0.5 rounded text-[10px] font-mono border bg-osint-bg/60 text-gray-300 border-osint-border/60 hover:bg-osint-border/50 hover:text-gray-100 disabled:opacity-40 disabled:cursor-not-allowed"
          >
            <MdMap size={12} />
            <span>{anyFilterActive ? `Pin ${filteredEvents.length} on map` : 'Show on map'}</span>
          </button>
          {events.length > 0 && (
            <button
              type="button"
              onClick={clearEvents}
              className="text-gray-500 hover:text-neon-cyan font-mono"
            >
              clear
            </button>
          )}
        </div>
      </div>

      <div className="flex-1 overflow-y-auto px-3 py-2 space-y-1.5 min-h-0">
        {filteredEvents.length === 0 && (
          <div className="text-gray-500 text-xs px-2 py-6 text-center italic">
            {events.length === 0
              ? (activeRun
                  ? 'Scanning channels… features will stream in as they arrive.'
                  : 'No discoveries yet. The next run is scheduled hourly, or triggers on server boot.')
              : 'No cameras match the current filters.'}
          </div>
        )}
        {filteredEvents.map((ev, i) => {
          const uid = ev.camera?.properties?.camera_uid;
          return (
            <EventRow
              key={`${ev.run_id ?? 'hist'}:${uid ?? i}`}
              ev={ev}
              isFavorite={uid ? favs.has(uid) : false}
              onToggleFavorite={toggleFavorite}
              onCardClick={onCardClick}
            />
          );
        })}
        {hasMore && (
          <div className="flex justify-center pt-2 pb-1">
            <button
              type="button"
              onClick={loadMore}
              disabled={loadingMore}
              className="px-3 py-1 rounded text-[10px] border bg-osint-bg/60 text-gray-400 border-osint-border hover:text-neon-cyan hover:border-neon-cyan/40 disabled:opacity-40 disabled:cursor-not-allowed"
            >
              {loadingMore ? 'Loading…' : 'Load older'}
            </button>
          </div>
        )}
      </div>
    </div>
  );
}
