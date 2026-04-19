import { useEffect, useRef, useState } from 'react';

/**
 * Subscribe to the camera discovery stream on the server's /ws endpoint.
 *
 * Emits:
 *   - events: reverse-chronological list (cap 500) of {camera, kind, channel, ts}
 *   - activeRun: {run_id, started_at, run_counts, elapsed_ms} | null
 *   - lastRun:   snapshot of the most recent camera_run_end payload | null
 *   - connected: WS connection state
 */
const MAX_EVENTS = 500;

export default function useCameraDiscoveryStream() {
  const [events, setEvents] = useState([]);
  const [activeRun, setActiveRun] = useState(null);
  const [lastRun, setLastRun] = useState(null);
  const [connected, setConnected] = useState(false);
  const wsRef = useRef(null);
  const retryRef = useRef(0);
  const timerRef = useRef(null);

  useEffect(() => {
    let cancelled = false;

    const tick = setInterval(() => {
      setActiveRun((r) => (r ? { ...r, elapsed_ms: Date.now() - r.started_ms } : r));
    }, 1000);
    timerRef.current = tick;

    const connect = () => {
      if (cancelled) return;
      try {
        const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const ws = new WebSocket(`${proto}//localhost:4000/ws`);
        wsRef.current = ws;

        ws.onopen = () => {
          setConnected(true);
          retryRef.current = 0;
        };

        ws.onmessage = (ev) => {
          let msg;
          try { msg = JSON.parse(ev.data); } catch { return; }

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

            case 'camera_discovered':
              setEvents((prev) => {
                const next = [
                  {
                    ts: Date.now(),
                    kind: msg.kind,
                    channel: msg.channel,
                    camera: msg.camera,
                    run_id: msg.run_id,
                  },
                  ...prev,
                ];
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
        };

        ws.onerror = () => { /* handled by onclose */ };

        ws.onclose = () => {
          setConnected(false);
          wsRef.current = null;
          if (cancelled) return;
          const delay = Math.min(1000 * Math.pow(2, retryRef.current), 30000);
          retryRef.current += 1;
          setTimeout(connect, delay);
        };
      } catch {
        setTimeout(connect, 2000);
      }
    };

    connect();

    return () => {
      cancelled = true;
      clearInterval(tick);
      if (wsRef.current) {
        wsRef.current.onclose = null;
        wsRef.current.close();
      }
    };
  }, []);

  const clearEvents = () => setEvents([]);

  return { events, activeRun, lastRun, connected, clearEvents };
}
