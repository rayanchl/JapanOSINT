/**
 * Subscribe to the /ws stream and surface every collector HTTP hit.
 *
 * Hits arrive in two phases:
 *   - phase:'start' — the request is in-flight (status null, latency null)
 *   - phase:'end'   — completed (or errored, with `error` set)
 * We collapse the pair into a single row keyed by `${run_id}:${request_seq}`
 * and update its status/latency/bytes/data_type when the end phase lands.
 *
 * There is no history route: only live traffic appears.
 *
 * `collector_hit_annotate` events arrive after the run finishes (server
 * route handler attaches feature counts) and patch `record_count` onto the
 * matching row.
 */

import { useCallback, useEffect, useRef, useState } from 'react';
import useWebSocket from './useWebSocket.js';

const HIT_CAP = 1000;
const RUN_CAP = 200;

export default function useCollectorFollowStream() {
  const [hits, setHits] = useState([]);            // newest first
  const [runs, setRuns] = useState([]);            // newest first
  const [paused, setPaused] = useState(false);

  // ── Coalescer ─────────────────────────────────────────────────────────
  // Burst WS traffic (every collector request emits start+end frames) used
  // to fire one setHits per message. With up to 1000 buffered hits and no
  // virtualization, that pinned the main thread. We buffer events in a ref
  // and flush them once per animation frame.
  const pendingRef = useRef([]);
  const rafRef = useRef(null);

  const flush = useCallback(() => {
    rafRef.current = null;
    const batch = pendingRef.current;
    if (batch.length === 0) return;
    pendingRef.current = [];
    setHits((prev) => {
      let next = prev;
      for (const msg of batch) {
        if (msg.type === 'collector_request_hit') {
          next = upsertHit(next, msg);
        } else if (msg.type === 'collector_hit_annotate') {
          next = next.map((h) => (
            h.run_id === msg.run_id && h.request_seq === msg.request_seq
              ? {
                  ...h,
                  record_count: msg.record_count ?? h.record_count,
                  data_type: msg.data_type ?? h.data_type,
                }
              : h
          ));
        }
      }
      return next;
    });
    setRuns((prev) => {
      let next = prev;
      for (const msg of batch) {
        if (msg.type === 'collector_run_start') {
          next = upsertRun(next, {
            run_id: msg.run_id,
            collector_key: msg.collector_key,
            source_id: msg.source_id,
            source_name: msg.source_name,
            trigger: msg.trigger,
            started_ms: Date.now(),
            ended_ms: null,
            status: null,
            hit_count: 0,
          });
        } else if (msg.type === 'collector_run_end') {
          next = next.map((r) => (
            r.run_id === msg.run_id
              ? {
                  ...r,
                  ended_ms: Date.now(),
                  status: msg.status,
                  hit_count: msg.hit_count,
                  error: msg.error,
                  duration_ms: msg.duration_ms,
                }
              : r
          ));
        }
      }
      return next;
    });
  }, []);

  const enqueue = useCallback((msg) => {
    pendingRef.current.push(msg);
    if (rafRef.current == null) rafRef.current = requestAnimationFrame(flush);
  }, [flush]);

  // There is no history seed: GET /api/follow/recent is a permanent HTTP 501
  // on this server (collector-tap history is not implemented), so it is not
  // called at all. The panel shows live traffic from the moment it opens.

  const onMessage = useCallback((msg) => {
    if (paused) return;
    // Only event types we actually consume — drop everything else
    // (camera_*, source_update, heartbeat, …) before queueing so the
    // buffer doesn't grow during idle bursts.
    if (
      msg.type === 'collector_request_hit'
      || msg.type === 'collector_run_start'
      || msg.type === 'collector_run_end'
      || msg.type === 'collector_hit_annotate'
    ) {
      enqueue(msg);
    }
  }, [paused, enqueue]);

  // `available` is false unless the build turned /ws on (utils/wsUrl.js) —
  // and the native server has neither /ws nor follow history (/api/follow/
  // recent answers 501), so on it this panel has no live source at all and
  // must say so rather than "reconnecting…" forever.
  const { connected, available: liveAvailable } = useWebSocket('/ws', { onMessage });

  // Cleanup pending rAF when unmounting.
  useEffect(() => () => {
    if (rafRef.current != null) cancelAnimationFrame(rafRef.current);
    pendingRef.current = [];
  }, []);

  const clear = () => { setHits([]); setRuns([]); };

  return {
    hits,
    runs,
    activeRuns: runs.filter((r) => !r.ended_ms),
    connected,
    liveAvailable,
    paused,
    setPaused,
    clear,
  };
}

// ─── Reducers ────────────────────────────────────────────────────────────

function upsertHit(prev, msg) {
  const key = `${msg.run_id}:${msg.request_seq}`;
  const idx = prev.findIndex((h) => h.key === key);
  if (idx >= 0) {
    const existing = prev[idx];
    const merged = {
      ...existing,
      // Phase 'end' fills the in-flight row
      status: msg.status ?? existing.status,
      latency_ms: msg.latency_ms ?? existing.latency_ms,
      response_bytes: msg.response_bytes ?? existing.response_bytes,
      data_type: msg.data_type ?? existing.data_type,
      error: msg.error ?? existing.error,
      phase: msg.phase,
      end_ts: msg.phase === 'end' ? msg.timestamp : existing.end_ts,
    };
    const next = prev.slice();
    next[idx] = merged;
    return next;
  }
  // New row — push newest-first
  const row = {
    key,
    run_id: msg.run_id,
    request_seq: msg.request_seq,
    collector_key: msg.collector_key,
    source_id: msg.source_id,
    source_name: msg.source_name,
    method: msg.method,
    url: msg.url,
    host: msg.host,
    path: msg.path,
    status: msg.status,
    latency_ms: msg.latency_ms,
    response_bytes: msg.response_bytes,
    data_type: msg.data_type,
    record_count: null,
    error: msg.error,
    phase: msg.phase,
    start_ts: msg.timestamp,
    end_ts: msg.phase === 'end' ? msg.timestamp : null,
  };
  const next = [row, ...prev];
  return next.length > HIT_CAP ? next.slice(0, HIT_CAP) : next;
}

function upsertRun(prev, run) {
  const idx = prev.findIndex((r) => r.run_id === run.run_id);
  if (idx >= 0) {
    const next = prev.slice();
    next[idx] = { ...next[idx], ...run };
    return next;
  }
  const next = [run, ...prev];
  return next.length > RUN_CAP ? next.slice(0, RUN_CAP) : next;
}
