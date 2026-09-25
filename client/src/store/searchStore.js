// Lightweight search store — a module singleton + a React subscription hook.
// Behavioural port of the OSINTsaas Zustand searchStore (start search ->
// EventSource progress stream -> live snapshot updates -> completed list),
// without adding zustand as a dependency. Snapshots are the verbatim
// progress_tracker shape the backend emits (native/core/progress.c).
//
// Auth: `/api/search/analyze` goes through the global fetch interceptor
// (auth/session.js) and carries the bearer. The SSE stream is pre-auth on the
// server precisely because EventSource cannot send headers.

import apiUrl from '../utils/apiUrl.js';
import { isTerminal } from '../components/search/pipeline.js';

const state = {
  active: [],     // [{ request_id, query, snapshot, status }]
  completed: [],  // finished snapshots (most recent first)
  lastError: null,
};
const listeners = new Set();
const streams = new Map(); // request_id -> EventSource

function snapshotState() {
  return { active: [...state.active], completed: [...state.completed], lastError: state.lastError };
}

function emit() {
  const snap = snapshotState();
  listeners.forEach((l) => l(snap));
}

function upsertActive(requestId, patch) {
  const i = state.active.findIndex((s) => s.request_id === requestId);
  if (i >= 0) state.active[i] = { ...state.active[i], ...patch };
  else state.active.unshift({ request_id: requestId, ...patch });
  emit();
}

function finishActive(requestId, snapshot) {
  state.active = state.active.filter((s) => s.request_id !== requestId);
  state.completed = state.completed.filter((s) => s.request_id !== requestId);
  state.completed.unshift({ request_id: requestId, query: snapshot.query, snapshot, status: snapshot.phase === 'error' ? 'error' : 'completed' });
  state.completed = state.completed.slice(0, 30);
  emit();
  dropStream(requestId);
}

// A dead stream must be torn down after a bounded number of failures. The
// server's not_found path writes `event: error` and then drains *without* a
// `close` frame, and per spec the browser silently reconnects a closed 200
// text/event-stream — so without this the request id reconnect-loops every
// ~3s forever and its entry in `streams` permanently blocks a re-subscribe.
const MAX_STREAM_ERRORS = 3;

function dropStream(requestId) {
  const es = streams.get(requestId);
  if (es) { es.close(); streams.delete(requestId); }
}

/**
 * The stream ended without a terminal frame (dropped connection, server
 * drained early). Reconcile ONCE through the results endpoint — the same
 * thing the iOS store does — so a run can never wedge on "Queued" when the
 * live channel did not deliver its final state. Anything short of a terminal
 * snapshot leaves the entry marked `error`, which the UI renders as such.
 */
async function reconcile(requestId) {
  if (!state.active.some((s) => s.request_id === requestId)) return;
  try {
    const res = await fetch(apiUrl(`/api/search/results/${encodeURIComponent(requestId)}`));
    if (res.ok) {
      const snap = await res.json();
      if (isTerminal(snap)) { finishActive(requestId, snap); return; }
      upsertActive(requestId, { query: snap.query, snapshot: snap, status: 'error',
        error: 'live progress stream lost; the run may still be working on the server' });
      return;
    }
    upsertActive(requestId, { status: 'error', error: `results lookup failed (HTTP ${res.status})` });
  } catch (e) {
    upsertActive(requestId, { status: 'error', error: e?.message || 'results lookup failed' });
  }
}

function connectStream(requestId) {
  if (streams.has(requestId)) return;
  const es = new EventSource(apiUrl(`/api/search/stream/${encodeURIComponent(requestId)}`));
  streams.set(requestId, es);
  let errorCount = 0;
  es.addEventListener('progress', (ev) => {
    try {
      const snap = JSON.parse(ev.data);
      errorCount = 0; // a good frame means the stream is alive again
      if (isTerminal(snap)) {
        finishActive(requestId, snap);
      } else {
        upsertActive(requestId, { query: snap.query, snapshot: snap, status: 'running', error: null });
      }
    } catch { /* ignore malformed frame */ }
  });
  es.addEventListener('error', () => {
    errorCount += 1;
    // CLOSED = the browser gave up; otherwise it is silently reconnecting,
    // so cap the retries ourselves.
    if (es.readyState === EventSource.CLOSED || errorCount >= MAX_STREAM_ERRORS) {
      dropStream(requestId);
      // Only touch an entry that is still active — a finished run has already
      // been moved to `completed` and must not be resurrected.
      reconcile(requestId);
    }
  });
  es.addEventListener('close', () => { dropStream(requestId); reconcile(requestId); });
}

export async function startSearch(query) {
  const q = String(query || '').trim();
  if (!q) return null;
  state.lastError = null;
  let res;
  try {
    res = await fetch(apiUrl('/api/search/analyze'), {
      method: 'POST',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ query: q }),
    });
  } catch (e) {
    state.lastError = `could not reach the backend (${e?.message || 'network error'})`;
    emit();
    throw new Error(state.lastError);
  }
  if (!res.ok) {
    let detail = '';
    try { const j = await res.json(); detail = j.detail || j.error || ''; } catch { /* no body */ }
    state.lastError = `analyze failed (HTTP ${res.status}${detail ? `: ${detail}` : ''})`;
    emit();
    throw new Error(state.lastError);
  }
  const { request_id } = await res.json();
  upsertActive(request_id, { query: q, snapshot: null, status: 'starting', startedAt: Date.now() });
  connectStream(request_id);
  return request_id;
}

/** Re-attach to a run by id (a permalink or a history row). Fetches the
 *  current snapshot; a still-running one gets its stream re-opened. */
export async function attachRun(requestId) {
  const id = String(requestId || '').trim();
  if (!id) return null;
  if (state.active.some((s) => s.request_id === id) || state.completed.some((s) => s.request_id === id)) return id;
  const res = await fetch(apiUrl(`/api/search/results/${encodeURIComponent(id)}`));
  if (!res.ok) throw new Error(`run ${id} not found (HTTP ${res.status})`);
  const snap = await res.json();
  if (isTerminal(snap)) finishActive(id, snap);
  else { upsertActive(id, { query: snap.query, snapshot: snap, status: 'running' }); connectStream(id); }
  return id;
}

export async function fetchSuggestions(query) {
  try {
    const res = await fetch(apiUrl(`/api/search/suggest?q=${encodeURIComponent(query)}`));
    if (!res.ok) return [];
    const j = await res.json();
    return Array.isArray(j.suggestions) ? j.suggestions : [];
  } catch { return []; }
}

export function getRun(requestId) {
  return state.active.find((s) => s.request_id === requestId)
    || state.completed.find((s) => s.request_id === requestId) || null;
}

export function subscribe(listener) {
  listeners.add(listener);
  listener(snapshotState());
  return () => listeners.delete(listener);
}
