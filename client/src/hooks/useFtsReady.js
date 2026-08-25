/**
 * Subscribe to /ws and surface readiness for one or more FTS mirror tables.
 *
 *   const ftsReady = useFtsReady('intel_items_fts');           // boolean
 *   const ftsReady = useFtsReady(['intel_items_fts', 'cameras_fts']); // boolean — true iff all ready
 *
 * Returns null until at least one server message lands (so callers can
 * distinguish "unknown" from "warming"). Server emits
 *   { type: 'fts_ready', table, rows, base_count, duration_ms, ts }
 * once per table when its boot rebuild finishes.
 */

import { useCallback, useMemo, useState } from 'react';
import useWebSocket from './useWebSocket.js';

export default function useFtsReady(tables) {
  const target = useMemo(() => (Array.isArray(tables) ? tables : [tables]), [tables]);
  const [readyMap, setReadyMap] = useState({});

  const onMessage = useCallback((msg) => {
    if (msg?.type !== 'fts_ready' || !msg.table) return;
    setReadyMap((prev) => (prev[msg.table] ? prev : { ...prev, [msg.table]: true }));
  }, []);

  useWebSocket('/ws', {
    onMessage,
    backoffBaseMs: 500,
    backoffMaxMs: 10_000,
    deps: [target.join('|')],
  });

  if (Object.keys(readyMap).length === 0) return null;
  return target.every((t) => readyMap[t] === true);
}
