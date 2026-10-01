import { useEffect, useState } from 'react';
import apiUrl from '../utils/apiUrl.js';
import { buildCatalog } from './layerCatalog.js';
import { LAYER_DEFINITIONS } from './layerDefinitions.js';

// One fetch of GET /api/layers per page load, shared by every consumer
// (map page, layer panel, source dashboard). Stale-while-revalidate against
// sessionStorage like useMapLayers' collection cache: a cached copy renders
// immediately and a background refresh replaces it when the server answers.
const SS_KEY = 'useLayerCatalog:v2';

let cachedServerLayers = null;   // last good array from the server (or storage)
let catalogError = null;         // last fetch failure message, if any
let inflight = null;             // Promise for the current fetch
let fetchedThisLoad = false;     // one revalidation per page load, not per mount
const listeners = new Set();

function readSession() {
  if (typeof window === 'undefined' || !window.sessionStorage) return null;
  try {
    const raw = sessionStorage.getItem(SS_KEY);
    if (!raw) return null;
    const parsed = JSON.parse(raw);
    return Array.isArray(parsed) ? parsed : null;
  } catch { return null; }
}

function writeSession(arr) {
  if (typeof window === 'undefined' || !window.sessionStorage) return;
  try { sessionStorage.setItem(SS_KEY, JSON.stringify(arr)); } catch { /* quota */ }
}

function notify() { for (const fn of listeners) fn(); }

/**
 * Fetch the server catalogue (once; concurrent callers share the promise).
 * Resolves to the array or null on failure — never throws, so a consumer
 * that must decide "which endpoint do I fetch" can await it and fall back.
 */
export function loadServerLayers({ force = false } = {}) {
  if (inflight) return inflight;
  if (cachedServerLayers && !force) return Promise.resolve(cachedServerLayers);
  inflight = (async () => {
    try {
      const res = await fetch(apiUrl('/api/layers'));
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const body = await res.json();
      const arr = Array.isArray(body) ? body : (Array.isArray(body?.layers) ? body.layers : null);
      if (!arr) throw new Error('unexpected /api/layers shape');
      cachedServerLayers = arr;
      catalogError = null;
      fetchedThisLoad = true;
      writeSession(arr);
      return arr;
    } catch (err) {
      catalogError = err?.message || 'request failed';
      console.warn('[useLayerCatalog] /api/layers failed:', catalogError);
      return cachedServerLayers; // null if we never had one
    } finally {
      inflight = null;
      notify();
    }
  })();
  return inflight;
}

/** Test hook: reset module state. */
export function _resetLayerCatalogForTests() {
  cachedServerLayers = null; catalogError = null; inflight = null; fetchedThisLoad = false; listeners.clear();
}

function snapshot() {
  return {
    catalog: buildCatalog(cachedServerLayers, LAYER_DEFINITIONS),
    serverLayers: cachedServerLayers,
    status: cachedServerLayers ? 'ready' : (catalogError ? 'error' : 'loading'),
    error: catalogError,
  };
}

/**
 * id -> { modality, data_type, kind, category, records_geocoded, sources,
 *         serverId, color, icon, endpoint, hidden, ... }
 *
 * Until /api/layers answers the catalogue is the client presentation table
 * alone (every entry `clientOnly`, status 'loading'); if it never answers the
 * status is 'error' and `error` says why, so the panel can state that the
 * server's taxonomy was not obtained rather than pretending the static table
 * is it.
 */
export default function useLayerCatalog() {
  const [state, setState] = useState(() => {
    if (!cachedServerLayers) {
      const fromSession = readSession();
      if (fromSession) cachedServerLayers = fromSession;
    }
    return snapshot();
  });

  useEffect(() => {
    const onChange = () => setState(snapshot());
    listeners.add(onChange);
    // Revalidate once per page load: a session copy may be stale.
    if (!fetchedThisLoad) loadServerLayers({ force: true });
    else setState(snapshot());
    return () => { listeners.delete(onChange); };
  }, []);

  return state;
}
