// Lightweight search store — a module singleton + a React subscription hook.
// Behavioural port of the OSINTsaas Zustand searchStore (start search ->
// EventSource progress stream -> live snapshot updates -> completed list),
// without adding zustand as a dependency. Snapshots are the verbatim
// progress_tracker shape the backend emits.

import apiUrl from '../utils/apiUrl.js';

const state = {
  active: [],     // [{ request_id, query, snapshot, status }]
  completed: [],  // finished snapshots (most recent first)
};
const listeners = new Set();
const streams = new Map(); // request_id -> EventSource

function emit() {
  const snap = { active: [...state.active], completed: [...state.completed] };
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
  state.completed.unshift({ request_id: requestId, query: snapshot.query, snapshot });
  state.completed = state.completed.slice(0, 30);
  emit();
  const es = streams.get(requestId);
  if (es) { es.close(); streams.delete(requestId); }
}

function connectStream(requestId) {
  if (streams.has(requestId)) return;
  const es = new EventSource(apiUrl(`/api/search/stream/${requestId}`));
  streams.set(requestId, es);
  es.addEventListener('progress', (ev) => {
    try {
      const snap = JSON.parse(ev.data);
      if (snap.done || snap.phase === 'completed' || snap.phase === 'error') {
        finishActive(requestId, snap);
      } else {
        upsertActive(requestId, { query: snap.query, snapshot: snap, status: 'running' });
      }
    } catch { /* ignore malformed frame */ }
  });
  es.addEventListener('error', () => {
    // Network blip: browser auto-reconnects EventSource. If the server sent
    // an explicit error frame the close event ends it.
  });
  es.addEventListener('close', () => { es.close(); streams.delete(requestId); });
}

export async function startSearch(query) {
  const res = await fetch(apiUrl('/api/search/analyze'), {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ query }),
  });
  if (!res.ok) throw new Error(`analyze failed (${res.status})`);
  const { request_id } = await res.json();
  upsertActive(request_id, { query, snapshot: null, status: 'starting' });
  connectStream(request_id);
  return request_id;
}

export async function fetchSuggestions(query) {
  try {
    const res = await fetch(apiUrl(`/api/search/suggest?q=${encodeURIComponent(query)}`));
    if (!res.ok) return [];
    const j = await res.json();
    return Array.isArray(j.suggestions) ? j.suggestions : [];
  } catch { return []; }
}

export function subscribe(listener) {
  listeners.add(listener);
  listener({ active: [...state.active], completed: [...state.completed] });
  return () => listeners.delete(listener);
}
