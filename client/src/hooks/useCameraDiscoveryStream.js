import { useCallback, useEffect, useRef, useState } from 'react';
import useWebSocket from './useWebSocket.js';
import apiUrl from '../utils/apiUrl.js';

/**
 * Subscribe to the camera discovery stream on the server's /ws endpoint,
 * seeded with historical events from /api/data/cameras/discovery-feed so
 * the thread is never empty on cold load.
 *
 * Each event carries:
 *   - ts          ISO timestamp (last_seen_at or fetched_at)
 *   - kind        'new' | 'updated' | 'historical'
 *   - channel     discovery channel name
 *   - camera      GeoJSON Feature
 *   - run_id      WS-only — null for backfill rows
 *   - isLive      true if surfaced via WS during this session (drives NEW
 *                 badge in the UI); false for backfill rows.
 *
 * Dedup rule: events are unique per camera.properties.camera_uid. A live
 * event with a uid that's already in the list *promotes* (removes the old
 * row, prepends the new one with isLive:true) instead of duplicating.
 */
const MAX_EVENTS = 2000;
const SEED_LIMIT = 500;

function camUid(ev) {
  const p = ev?.camera?.properties || {};
  return p.camera_uid || p.camera_id || null;
}

export default function useCameraDiscoveryStream() {
  const [events, setEvents] = useState([]);
  const [activeRun, setActiveRun] = useState(null);
  const [lastRun, setLastRun] = useState(null);
  const [cursor, setCursor] = useState(null);
  const [loadingMore, setLoadingMore] = useState(false);
  const seededRef = useRef(false);

  // Seed from the backfill endpoint on mount — runs once even under React
  // strict-mode double-invoke thanks to the ref guard.
  useEffect(() => {
    if (seededRef.current) return;
    seededRef.current = true;
    let cancelled = false;
    (async () => {
      try {
        const res = await fetch(apiUrl(`/api/data/cameras/discovery-feed?limit=${SEED_LIMIT}`));
        if (!res.ok) return;
        const data = await res.json();
        if (cancelled) return;
        const seed = (data.events || []).map((ev) => ({ ...ev, isLive: false }));
        setEvents((prev) => {
          // If WS events landed before the seed finished, dedup them against
          // the seed by camera_uid and keep the live versions.
          const seen = new Set(prev.map(camUid).filter(Boolean));
          const merged = [...prev];
          for (const ev of seed) {
            const uid = camUid(ev);
            if (uid && seen.has(uid)) continue;
            merged.push(ev);
            if (uid) seen.add(uid);
          }
          merged.sort((a, b) => (b.ts || '').localeCompare(a.ts || ''));
          return merged.slice(0, MAX_EVENTS);
        });
        setCursor(data.cursor || null);
      } catch { /* network error: leave the thread WS-only */ }
    })();
    return () => { cancelled = true; };
  }, []);

  // Tick `activeRun.elapsed_ms` once per second while a run is active.
  useEffect(() => {
    const tick = setInterval(() => {
      setActiveRun((r) => (r ? { ...r, elapsed_ms: Date.now() - r.started_ms } : r));
    }, 1000);
    return () => clearInterval(tick);
  }, []);

  const onMessage = useCallback((msg) => {
    switch (msg.type) {
      case 'camera_run_start':
        setActiveRun({
          run_id: msg.run_id,
          started_ms: Date.now(),
          elapsed_ms: 0,
          run_counts: {},
          new_count: 0,
          updated_count: 0,
          channels_done: [],
        });
        break;

      case 'camera_discovered': {
        const incoming = {
          ts: new Date().toISOString(),
          kind: msg.kind,
          channel: msg.channel,
          camera: msg.camera,
          run_id: msg.run_id,
          isLive: true,
        };
        const uid = camUid(incoming);
        setEvents((prev) => {
          const filtered = uid ? prev.filter((e) => camUid(e) !== uid) : prev;
          const next = [incoming, ...filtered];
          return next.length > MAX_EVENTS ? next.slice(0, MAX_EVENTS) : next;
        });
        setActiveRun((r) => {
          if (!r || r.run_id !== msg.run_id) return r;
          return {
            ...r,
            run_counts: msg.run_counts || r.run_counts,
            new_count: r.new_count + (msg.kind === 'new' ? 1 : 0),
            updated_count: r.updated_count + (msg.kind === 'updated' ? 1 : 0),
          };
        });
        break;
      }

      case 'camera_channel_done':
        setActiveRun((r) => {
          if (!r || r.run_id !== msg.run_id) return r;
          return {
            ...r,
            channels_done: [...r.channels_done, { name: msg.channel, ok: msg.ok, count: msg.count }],
          };
        });
        break;

      case 'camera_run_end':
        setLastRun(msg);
        setActiveRun(null);
        break;

      default:
        break;
    }
  }, []);

  const { connected } = useWebSocket('/ws', { onMessage });

  const loadMore = useCallback(async () => {
    if (!cursor || loadingMore) return;
    setLoadingMore(true);
    try {
      const res = await fetch(apiUrl(`/api/data/cameras/discovery-feed?limit=${SEED_LIMIT}&cursor=${encodeURIComponent(cursor)}`));
      if (!res.ok) return;
      const data = await res.json();
      const older = (data.events || []).map((ev) => ({ ...ev, isLive: false }));
      setEvents((prev) => {
        const seen = new Set(prev.map(camUid).filter(Boolean));
        const merged = [...prev];
        for (const ev of older) {
          const uid = camUid(ev);
          if (uid && seen.has(uid)) continue;
          merged.push(ev);
          if (uid) seen.add(uid);
        }
        merged.sort((a, b) => (b.ts || '').localeCompare(a.ts || ''));
        return merged.slice(0, MAX_EVENTS);
      });
      setCursor(data.cursor || null);
    } finally {
      setLoadingMore(false);
    }
  }, [cursor, loadingMore]);

  const clearEvents = () => setEvents([]);
  return { events, activeRun, lastRun, connected, clearEvents, loadMore, hasMore: !!cursor, loadingMore };
}
