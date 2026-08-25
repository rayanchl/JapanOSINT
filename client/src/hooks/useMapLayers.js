import { useState, useCallback, useRef, useMemo, useEffect } from 'react';
import useLayerLoading from './useLayerLoading.js';
import apiUrl from '../utils/apiUrl.js';
import { LAYER_DEFINITIONS, LAYER_CATEGORIES } from './layerDefinitions.js';
import useLayerCatalog, { loadServerLayers } from './useLayerCatalog.js';
import { buildCatalog } from './layerCatalog.js';

// sessionStorage survives page reloads within the same tab but clears on
// tab close, so we can cache FCs across F5 without bloating long-term
// storage. Keyed per-layer. Load lazily on mount; write through on every
// cacheRef update (via a useEffect that mirrors cacheRef into storage).
const SS_PREFIX = 'useMapLayers:cache:';
const SS_MAX_BYTES_PER_LAYER = 5 * 1024 * 1024; // 5 MB cap; bigger layers skip storage

// Page size for GET /api/layers/:id/geojson. Every page is fetched until the
// server says `truncated: false` (house rule 2: a source we spend a request
// on is used exhaustively); PAGE_CEILING only bounds a runaway loop, and a
// collection that hits it stays marked truncated so the panel says so.
export const LAYER_PAGE_LIMIT = 5000;
const PAGE_CEILING = 400;

function loadPersistedCache() {
  const out = {};
  if (typeof window === 'undefined' || !window.sessionStorage) return out;
  try {
    for (let i = 0; i < sessionStorage.length; i += 1) {
      const key = sessionStorage.key(i);
      if (!key || !key.startsWith(SS_PREFIX)) continue;
      const layerId = key.slice(SS_PREFIX.length);
      const raw = sessionStorage.getItem(key);
      if (!raw) continue;
      try {
        const parsed = JSON.parse(raw);
        if (parsed && typeof parsed === 'object' && parsed.type === 'FeatureCollection') {
          out[layerId] = parsed;
        }
      } catch { /* skip corrupt entries */ }
    }
  } catch { /* quota/privacy-mode errors: ignore */ }
  return out;
}

function persistToSession(layerId, fc) {
  if (typeof window === 'undefined' || !window.sessionStorage) return;
  try {
    const json = JSON.stringify(fc);
    if (json.length > SS_MAX_BYTES_PER_LAYER) return;
    sessionStorage.setItem(SS_PREFIX + layerId, json);
  } catch { /* quota exceeded: silently drop this entry */ }
}

// LAYER_DEFINITIONS / LAYER_CATEGORIES live in layerDefinitions.js; re-exported
// here so existing imports keep resolving.
export { LAYER_DEFINITIONS, LAYER_CATEGORIES };

class HttpError extends Error {
  constructor(status) {
    super(`HTTP ${status}`);
    this.status = status;
  }
}

function toFeatureCollection(data) {
  if (data && data.type === 'FeatureCollection') return data;
  return {
    type: 'FeatureCollection',
    features: Array.isArray(data) ? data : (data?.features || []),
    ...(data && data._meta ? { _meta: data._meta } : {}),
  };
}

/**
 * Fetch every page of GET /api/layers/:id/geojson and fold them into one
 * FeatureCollection. `onPage(fc)` is called after each page with the
 * collection so far (its `_meta.truncated` is true until the last page
 * lands) so the map can paint progressively and the panel can state
 * "showing N of M" while the rest is still arriving.
 */
export async function fetchLayerGeojsonPaged(serverId, { limit = LAYER_PAGE_LIMIT, onPage, fetchImpl } = {}) {
  const doFetch = fetchImpl || fetch;
  let offset = 0;
  let features = [];
  let meta = null;
  let pages = 0;
  for (;;) {
    const url = apiUrl(`/api/layers/${encodeURIComponent(serverId)}/geojson?limit=${limit}&offset=${offset}`);
    const res = await doFetch(url);
    if (!res.ok) throw new HttpError(res.status);
    const page = toFeatureCollection(await res.json());
    const pageMeta = page._meta || {};
    const got = Array.isArray(page.features) ? page.features : [];
    features = features.concat(got);
    pages += 1;
    const total = Number.isFinite(pageMeta.records_available) ? pageMeta.records_available : null;
    const truncated = !!pageMeta.truncated;
    const next = Number.isFinite(pageMeta.next_offset) ? pageMeta.next_offset : offset + got.length;
    const stalled = truncated && next <= offset;
    const ceiling = truncated && pages >= PAGE_CEILING;
    const done = !truncated || stalled || ceiling || got.length === 0;
    meta = {
      ...pageMeta,
      records_available: total,
      records_used: features.length,
      offset: 0,
      limit,
      pages,
      // Still truncated if the server said so and we could not go on.
      truncated: !done ? true : (truncated ? true : false),
      ...(done && truncated ? { truncation_reason: stalled ? 'next_offset did not advance' : (ceiling ? `page ceiling ${PAGE_CEILING}` : 'server returned an empty page while truncated') } : {}),
    };
    if (typeof onPage === 'function') onPage({ type: 'FeatureCollection', features, _meta: meta });
    if (done) break;
    offset = next;
  }
  return { type: 'FeatureCollection', features, _meta: meta };
}

export default function useMapLayers() {
  const { catalog, status: catalogStatus, error: catalogError } = useLayerCatalog();
  const catalogRef = useRef(catalog);
  useEffect(() => { catalogRef.current = catalog; }, [catalog]);

  const [layers, setLayers] = useState(() => {
    const initial = {};
    for (const key of Object.keys(LAYER_DEFINITIONS)) {
      const def = LAYER_DEFINITIONS[key];
      initial[key] = {
        visible: false,
        opacity: 1,
        loading: false,
        // Temporal layers carry a [start, end] year_month window; null = "all".
        // Defaults to the full range so toggling on doesn't filter anything
        // out until the user moves the slider.
        ...(def?.temporal ? { temporalWindow: null } : {}),
      };
    }
    return initial;
  });


  const [layerData, setLayerData] = useState({});
  // Latest-value mirror of `layers` so the toggle helpers can decide what to
  // fetch *before* calling setLayers — React state updaters must stay pure
  // (StrictMode double-invokes them, which would double every fetch).
  const layersRef = useRef(layers);
  useEffect(() => { layersRef.current = layers; }, [layers]);

  // Server-only layers arrive after /api/layers answers: give each a state
  // slot so it can be toggled like any other.
  useEffect(() => {
    const missing = Object.keys(catalog).filter((id) => !layersRef.current[id]);
    if (missing.length === 0) return;
    const add = {};
    for (const id of missing) add[id] = { visible: false, opacity: 1, loading: false };
    layersRef.current = { ...layersRef.current, ...add };
    setLayers((prev) => {
      const next = { ...prev };
      for (const id of missing) if (!next[id]) next[id] = add[id];
      return next;
    });
  }, [catalog]);
  // Seed cacheRef from sessionStorage so a hard reload restores cached
  // FCs without round-tripping to the server. See persistToSession() for
  // the write-through in doFetch().
  const cacheRef = useRef(loadPersistedCache());
  // Memo for the temporal-window view below, keyed per layer on (source FC,
  // window) so the filtered collection keeps a stable identity.
  const viewCacheRef = useRef({});

  // Server-driven loading flags keyed by endpoint-slug (e.g. 'fire-station-map')
  // — merged into per-layer `loading` via OR, so the spinner fires whenever
  // EITHER the client fetch is in flight OR the server is doing collector
  // work on our behalf. The server keys layer_work_* events on the kebab
  // layer id; map both the server id and the /api/data slug back to ours.
  const serverLoadingByEndpoint = useLayerLoading();
  const slugToLayerId = useMemo(() => {
    const map = {};
    for (const [id, entry] of Object.entries(catalog)) {
      if (entry.serverId && !map[entry.serverId]) map[entry.serverId] = id;
      const seg = entry.endpoint ? String(entry.endpoint).split('/').filter(Boolean).pop() : null;
      if (seg && !map[seg]) map[seg] = id;
    }
    return map;
  }, [catalog]);
  const serverLoadingByLayerId = useMemo(() => {
    const out = {};
    for (const [endpointSlug, busy] of Object.entries(serverLoadingByEndpoint)) {
      const layerId = slugToLayerId[endpointSlug];
      if (layerId) out[layerId] = busy;
    }
    return out;
  }, [serverLoadingByEndpoint, slugToLayerId]);

  // Is a cached FC still considered fresh? Use the server-supplied
  // _meta.age_ms + _meta.ttl_ms (Track 1) to decide. A cached copy is fresh
  // while its effective age is under half of TTL — past the halfway mark we
  // still render it instantly but kick a background refresh (SWR).
  //
  // `age_ms` is frozen at response time, and nothing used to record when the
  // client stored its copy — so a collection cached with age_ms: 0 stayed
  // "fresh" across F5 and across hours, and aircraft/AIS/river/dam positions
  // were restated as current observations. Age it from receipt time too.
  const isCachedFresh = (fc) => {
    const ttl = fc?._meta?.ttl_ms;
    const age = fc?._meta?.age_ms;
    if (!Number.isFinite(ttl) || ttl <= 0) return false;
    const storedAt = fc?._meta?.client_stored_at;
    const sinceStored = Number.isFinite(storedAt) ? Math.max(0, Date.now() - storedAt) : 0;
    const effectiveAge = (Number.isFinite(age) ? age : 0) + sinceStored;
    return effectiveAge < ttl / 2;
  };

  // Resolve the catalogue entry for a layer AFTER the server taxonomy has
  // been asked for, so a toggle that lands before /api/layers answers still
  // goes to /api/layers/:id/geojson rather than the legacy /api/data path.
  const resolveEntry = useCallback(async (layerId) => {
    const current = catalogRef.current[layerId];
    if (current && !current.clientOnly) return current;
    const serverLayers = await loadServerLayers();
    if (serverLayers) {
      const rebuilt = buildCatalog(serverLayers, LAYER_DEFINITIONS);
      if (rebuilt[layerId]) return rebuilt[layerId];
    }
    return current || null;
  }, []);

  // Internal: do the network fetch + cache update. `background: true` skips
  // the `loading: true` flag so the spinner doesn't flash while we're
  // silently refreshing behind an already-rendered cached copy.
  const doFetch = useCallback(async (layerId, { background = false } = {}) => {
    const entry = await resolveEntry(layerId);
    if (!entry || (!entry.serverId && !entry.endpoint)) return;

    if (!background) {
      setLayers((prev) => ({
        ...prev,
        [layerId]: { ...prev[layerId], loading: true },
      }));
    }

    try {
      let geojson;
      if (entry.serverId) {
        geojson = await fetchLayerGeojsonPaged(entry.serverId, {
          onPage: (partial) => {
            // Paint what has arrived so far; the collection carries
            // _meta.truncated=true until the last page, so the panel states
            // the bound while the rest is in flight.
            if (!partial._meta?.truncated) return;
            partial._meta = { ...partial._meta, client_stored_at: Date.now(), client_loading: true };
            setLayerData((prev) => ({ ...prev, [layerId]: partial }));
          },
        });
      } else {
        // Client-only layer (no server layer with this id): legacy per-source path.
        const res = await fetch(apiUrl(entry.endpoint));
        if (!res.ok) throw new HttpError(res.status);
        geojson = toFeatureCollection(await res.json());
      }

      // Stamp receipt time so the cache can age (isCachedFresh) and so the
      // map's "last update" can report the data's age instead of the clock.
      geojson._meta = { ...(geojson._meta || {}), client_stored_at: Date.now() };

      cacheRef.current[layerId] = geojson;
      persistToSession(layerId, geojson);
      setLayerData((prev) => ({ ...prev, [layerId]: geojson }));
    } catch (err) {
      console.warn(`[useMapLayers] Failed to fetch ${layerId}:`, err.message);
      if (!background) {
        // Only surface the failure placeholder on foreground failure; a
        // background-refresh error leaves the cached copy in place.
        // The failure travels WITH the collection: an empty FC on its own is
        // pixel-identical to a source that legitimately returned zero
        // records, so the panel could never tell them apart. A 404 is a
        // third, distinct fact ("no such layer") and is flagged as such.
        const status = Number.isFinite(err?.status) ? err.status : null;
        setLayerData((prev) => ({
          ...prev,
          [layerId]: {
            type: 'FeatureCollection',
            features: [],
            _error: err.message || 'request failed',
            _status: status,
            _notFound: status === 404,
          },
        }));
      }
    } finally {
      if (!background) {
        setLayers((prev) => ({
          ...prev,
          [layerId]: { ...prev[layerId], loading: false },
        }));
      }
    }
  }, [resolveEntry]);

  const fetchLayerData = useCallback(async (layerId) => {
    const cached = cacheRef.current[layerId];
    if (cached) {
      // Render cached copy immediately so the layer appears with no spinner
      // flash. If stale (past half-TTL), kick a background refresh.
      setLayerData((prev) => ({ ...prev, [layerId]: cached }));
      if (!isCachedFresh(cached)) doFetch(layerId, { background: true });
      return;
    }

    // No cache yet — do a foreground fetch with spinner.
    await doFetch(layerId, { background: false });
  }, [doFetch]);

  const toggleLayer = useCallback((layerId) => {
    const def = catalogRef.current[layerId] || LAYER_DEFINITIONS[layerId];
    // Sensitivity gate: layers carrying PII (wanted-person photos, suspect
    // details) require an explicit one-shot user opt-in. Acceptance is
    // persisted in localStorage so the prompt fires once per browser, not
    // on every toggle.
    if (def?.sensitive) {
      try {
        const ok = typeof window !== 'undefined'
          && (window.localStorage?.getItem('japanosint.sensitive_acknowledged') === '1'
              || window.confirm('This layer contains sensitive content (suspect photos / personal details). Show anyway?'));
        if (!ok) return;
        if (typeof window !== 'undefined') window.localStorage?.setItem('japanosint.sensitive_acknowledged', '1');
      } catch { /* fail open in non-browser envs */ }
    }
    const current = layersRef.current[layerId];
    if (!current) return;
    const newVisible = !current.visible;

    // Mirror into the ref immediately so two toggles in the same tick don't
    // both read the pre-toggle value.
    layersRef.current = {
      ...layersRef.current,
      [layerId]: { ...current, visible: newVisible },
    };
    setLayers((prev) => (prev[layerId]
      ? { ...prev, [layerId]: { ...prev[layerId], visible: newVisible } }
      : prev));

    if (newVisible) {
      // Always call fetchLayerData — it decides whether to render cached,
      // background-refresh, or foreground-fetch based on cache state.
      fetchLayerData(layerId);
    }
  }, [fetchLayerData]);

  const setLayerOpacity = useCallback((layerId, opacity) => {
    setLayers((prev) => ({
      ...prev,
      [layerId]: { ...prev[layerId], opacity },
    }));
  }, []);

  /**
   * Set the time window for a temporal layer. Pass `null` to clear the filter
   * (show all features). `window` is a `[startYM, endYM]` pair where each is
   * a `'YYYY-MM'` string (or `'YYYY'` to mean the whole year).
   */
  const setLayerTemporalWindow = useCallback((layerId, window) => {
    setLayers((prev) => {
      const current = prev[layerId];
      if (!current) return prev;
      return {
        ...prev,
        [layerId]: { ...current, temporalWindow: window },
      };
    });
  }, []);

  const setAllLayers = useCallback((visible) => {
    const keys = Object.keys(layersRef.current);
    const mirrored = {};
    for (const key of keys) mirrored[key] = { ...layersRef.current[key], visible };
    layersRef.current = mirrored;

    setLayers((prev) => {
      const updated = {};
      for (const key of Object.keys(prev)) updated[key] = { ...prev[key], visible };
      return updated;
    });

    // fetchLayerData handles its own cache / staleness decision. Kept out of
    // the updater so StrictMode's double-invoke can't double every request.
    if (visible) for (const key of keys) fetchLayerData(key);
  }, [fetchLayerData]);

  // Auto-follow: unifiedSubways visibility mirrors unifiedTrains (subways
  // are folded under the Trains toggle in the panel). unifiedStations +
  // unifiedStationFootprints mirror whether any transit mode is on. MapView
  // filters features by current mode_set, so a cross-mode station only
  // shows pins/footprint for the modes that are enabled.
  const trainsOn = !!layers.unifiedTrains?.visible;
  useEffect(() => {
    const subway = layers.unifiedSubways;
    if (!subway) return;
    if (subway.visible !== trainsOn) {
      if (trainsOn) fetchLayerData('unifiedSubways');
      setLayers((prev) => ({
        ...prev,
        unifiedSubways: { ...prev.unifiedSubways, visible: trainsOn },
      }));
    }
  }, [trainsOn, fetchLayerData, layers]);

  const transitModesOn = !!(
    layers.unifiedTrains?.visible
    || layers.unifiedSubways?.visible
    || layers.unifiedBuses?.visible
  );
  useEffect(() => {
    for (const followerId of ['unifiedStations', 'unifiedStationFootprints']) {
      const current = layers[followerId];
      if (!current) continue;
      if (current.visible !== transitModesOn) {
        if (transitModesOn) fetchLayerData(followerId);
        setLayers((prev) => ({
          ...prev,
          [followerId]: { ...prev[followerId], visible: transitModesOn },
        }));
      }
    }
  }, [transitModesOn, fetchLayerData, layers]);

  const activeCount = Object.values(layers).filter((l) => l.visible).length;

  // Merge server-driven loading into each layer's `loading` flag via OR so
  // the LayerPanel spinner fires on either client-fetch-in-flight or
  // server-collector-in-progress (the latter matters when the cache misses
  // and the server does real work on our behalf).
  const mergedLayers = useMemo(() => {
    const out = {};
    for (const [id, state] of Object.entries(layers)) {
      const serverBusy = !!serverLoadingByLayerId[id];
      out[id] = serverBusy && !state.loading
        ? { ...state, loading: true }
        : state;
    }
    return out;
  }, [layers, serverLoadingByLayerId]);

  // The Window selector had no consumer: `temporalWindow` was written into
  // layer state and read back only by the <select>, so a user who picked
  // 2024-03 saw the control confirm that month while the map kept plotting
  // every month it held. `layerData` stays whole — the panel needs the full
  // month list and the true total to state "showing N of M" — and the map
  // draws this filtered view.
  const layerDataView = useMemo(() => {
    const out = {};
    for (const [id, fc] of Object.entries(layerData)) {
      const entry = catalog[id];
      const win = layers[id]?.temporalWindow;
      const key = entry?.temporalKey || null;
      if (!key || !win || !Array.isArray(fc?.features)) {
        out[id] = fc;
        continue;
      }
      const start = String(win[0]);
      const end = String(win[1]);
      // Reuse the previous filtered collection when neither the source data
      // nor the window moved, so an unrelated layer toggle doesn't hand
      // MapView a brand-new object and force a needless repaint.
      const memo = viewCacheRef.current[id];
      if (memo && memo.fc === fc && memo.start === start && memo.end === end) {
        out[id] = memo.result;
        continue;
      }
      const result = {
        ...fc,
        features: fc.features.filter((f) => {
          const v = f?.properties?.[key];
          if (v == null) return false;
          const sv = String(v);
          return sv >= start && sv <= end;
        }),
      };
      viewCacheRef.current[id] = { fc, start, end, result };
      out[id] = result;
    }
    return out;
  }, [layerData, layers, catalog]);

  return {
    layers: mergedLayers,
    catalog,
    catalogStatus,
    catalogError,
    toggleLayer,
    setLayerOpacity,
    setLayerTemporalWindow,
    setAllLayers,
    layerData,
    layerDataView,
    activeCount,
  };
}
