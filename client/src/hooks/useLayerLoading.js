import { useCallback, useState } from 'react';
import useWebSocket from './useWebSocket.js';

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

  const onMessage = useCallback((msg) => {
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
  }, []);

  useWebSocket('/ws', { onMessage });
  return loading;
}
