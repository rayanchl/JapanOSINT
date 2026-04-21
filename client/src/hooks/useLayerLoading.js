import { useEffect, useRef, useState } from 'react';

/**
 * Subscribe to the /ws stream and surface a per-layer "server is working"
 * flag. Complements the client's own HTTP in-flight flag: when the user
 * toggles a layer ON, the client fetch starts AND (on cache miss) the
 * server emits layer_work_started, so the spinner fires end-to-end.
 *
 * Returns a { [layerId]: boolean } map consumed via the hook's caller,
 * typically merged with the local loading flag in useMapLayers.
 */
export default function useLayerLoading() {
  const [loading, setLoading] = useState({});
  const wsRef = useRef(null);
  const retryRef = useRef(0);

  useEffect(() => {
    let cancelled = false;

    const connect = () => {
      if (cancelled) return;
      try {
        const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const ws = new WebSocket(`${proto}//localhost:4000/ws`);
        wsRef.current = ws;

        ws.onopen = () => { retryRef.current = 0; };

        ws.onmessage = (ev) => {
          let msg;
          try { msg = JSON.parse(ev.data); } catch { return; }
          if (msg.type === 'layer_work_started') {
            if (!msg.layer_id) return;
            setLoading((prev) => (prev[msg.layer_id] ? prev : { ...prev, [msg.layer_id]: true }));
          } else if (msg.type === 'layer_work_finished') {
            if (!msg.layer_id) return;
            setLoading((prev) => {
              if (!prev[msg.layer_id]) return prev;
              const next = { ...prev };
              delete next[msg.layer_id];
              return next;
            });
          }
        };

        ws.onerror = () => { /* onclose handles reconnect */ };

        ws.onclose = () => {
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
      if (wsRef.current) {
        wsRef.current.onclose = null;
        wsRef.current.close();
      }
    };
  }, []);

  return loading;
}
