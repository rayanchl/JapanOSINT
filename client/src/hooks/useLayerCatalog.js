import { useSyncExternalStore } from 'react';
import apiUrl from '../utils/apiUrl.js';
import { buildCatalog } from './layerCatalog.js';
import { LAYER_DEFINITIONS } from './layerDefinitions.js';
import { sessionStore } from '../auth/session.js';

// One fetch of GET /api/layers per page load, shared by every consumer
// (map page, layer panel, source dashboard). Stale-while-revalidate against
// sessionStorage like useMapLayers' collection cache: a cached copy renders
// immediately and a background refresh replaces it when the server answers.
// Keyed by the active tenant, and wiped on sign-out (clearLayerCatalogCache),
// so the next account in this tab never renders the previous one's taxonomy.
const SS_KEY_BASE = 'useLayerCatalog:v2';
const ssKey = () => `${SS_KEY_BASE}:${sessionStore.tenantId || 'none'}`;

let cachedServerLayers = null;   // last good array from the server (or storage)
let cachedForKey = null;         // the storage key (tenant) that array belongs to
let catalogError = null;         // last fetch failure message, if any
let inflight = null;             // Promise for the current fetch
let fetchedThisLoad = false;     // one revalidation per page load, not per mount
const listeners = new Set();

function readSession() {
  if (typeof window === 'undefined' || !window.sessionStorage) return null;
  try {
    const raw = sessionStorage.getItem(ssKey());
    if (!raw) return null;
    const parsed = JSON.parse(raw);
    return Array.isArray(parsed) ? parsed : null;
  } catch { return null; }
}

function writeSession(arr) {
  if (typeof window === 'undefined' || !window.sessionStorage) return;
  try { sessionStorage.setItem(ssKey(), JSON.stringify(arr)); } catch { /* quota */ }
}

function notify() { for (const fn of listeners) fn(); }

/**
 * Fetch the server catalogue (once; concurrent callers share the promise).
 * Resolves to the array or null on failure — never throws, so a consumer
 * that must decide "which endpoint do I fetch" can await it and fall back.
 */
export function loadServerLayers({ force = false } = {}) {
  // A different tenant than the one the cached copy was read for: drop it.
  if (cachedServerLayers && cachedForKey !== ssKey()) { cachedServerLayers = null; fetchedThisLoad = false; }
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
      cachedForKey = ssKey();
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

/** Sign-out / account change: forget the cached taxonomy, in memory and in storage. */
export function clearLayerCatalogCache() {
  cachedServerLayers = null; cachedForKey = null; fetchedThisLoad = false;
  if (typeof window === 'undefined' || !window.sessionStorage) return;
  try {
    const drop = [];
    for (let i = 0; i < sessionStorage.length; i += 1) {
      const k = sessionStorage.key(i);
      if (k && k.startsWith(SS_KEY_BASE)) drop.push(k);
    }
    drop.forEach((k) => sessionStorage.removeItem(k));
  } catch { /* storage unavailable */ }
}

/** Test hook: reset module state. */
export function _resetLayerCatalogForTests() {
  cachedServerLayers = null; catalogError = null; inflight = null; fetchedThisLoad = false; listeners.clear();
}

// useSyncExternalStore needs a referentially stable snapshot: rebuild only
// when the module state it is derived from changed identity.
let snapState = null;
let snapLayers = null;
let snapError = null;
function snapshot() {
  // Hydrate from sessionStorage the first time anyone reads (module state, not component state).
  if (!cachedServerLayers) {
    const fromSession = readSession();
    if (fromSession) { cachedServerLayers = fromSession; cachedForKey = ssKey(); }
  }
  if (!snapState || snapLayers !== cachedServerLayers || snapError !== catalogError) {
    snapLayers = cachedServerLayers;
    snapError = catalogError;
    snapState = {
      catalog: buildCatalog(cachedServerLayers, LAYER_DEFINITIONS),
      serverLayers: cachedServerLayers,
      status: cachedServerLayers ? 'ready' : (catalogError ? 'error' : 'loading'),
      error: catalogError,
    };
  }
  return snapState;
}

function subscribe(onChange) {
  listeners.add(onChange);
  // Revalidate once per page load: a session copy may be stale.
  if (!fetchedThisLoad) loadServerLayers({ force: true });
  return () => { listeners.delete(onChange); };
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
  return useSyncExternalStore(subscribe, snapshot, snapshot);
}
