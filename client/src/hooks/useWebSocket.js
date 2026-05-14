import { useEffect, useRef, useState } from 'react';
import wsUrl from '../utils/wsUrl.js';

/**
 * Shared WebSocket lifecycle hook.
 *
 * Handles the parts every consumer was reimplementing: opening the socket,
 * exponential-backoff reconnect on close, cancellation, and `connected` state.
 * Callers supply only the message handler and any side effects on open/close.
 *
 * Five hooks now use this: useDataSources, useLayerLoading,
 * useCameraDiscoveryStream, useCollectorFollowStream, useFtsReady.
 *
 * @param {string} path                       e.g. '/ws'
 * @param {object} opts
 * @param {(msg: any, ev: MessageEvent) => void} opts.onMessage
 *        Called for every message after JSON.parse. Parse errors are silently
 *        ignored (every consumer treated them this way already).
 * @param {() => void} [opts.onOpen]
 * @param {() => void} [opts.onClose]
 * @param {number} [opts.backoffBaseMs=1000]  initial retry delay
 * @param {number} [opts.backoffMaxMs=30000]  ceiling on retry delay
 * @param {boolean} [opts.enabled=true]
 * @param {boolean} [opts.respondToOnline=false]  re-connect on `online` /
 *        visibilitychange events (useful for long-lived dashboard hooks
 *        whose retry budget can otherwise be exhausted by laptop sleep)
 * @param {Array} [opts.deps=[]]              extra dependency array — when
 *        any value changes, the socket is closed and re-established
 *
 * @returns {{ connected: boolean, getSocket: () => WebSocket|null }}
 */
export default function useWebSocket(path, {
  onMessage,
  onOpen = null,
  onClose = null,
  backoffBaseMs = 1000,
  backoffMaxMs = 30000,
  enabled = true,
  respondToOnline = false,
  deps = [],
} = {}) {
  const [connected, setConnected] = useState(false);
  const wsRef = useRef(null);
  const retryRef = useRef(0);
  const timerRef = useRef(null);

  // Pin the latest handler closures so we don't tear down the socket on every
  // render. Identity changes of onMessage/onOpen/onClose don't reconnect.
  const onMessageRef = useRef(onMessage);
  const onOpenRef = useRef(onOpen);
  const onCloseRef = useRef(onClose);
  useEffect(() => { onMessageRef.current = onMessage; }, [onMessage]);
  useEffect(() => { onOpenRef.current = onOpen; }, [onOpen]);
  useEffect(() => { onCloseRef.current = onClose; }, [onClose]);

  useEffect(() => {
    if (!enabled) return undefined;
    let cancelled = false;

    const connect = () => {
      if (cancelled) return;
      let ws;
      try {
        const url = wsUrl(path);
        if (!url) return;
        ws = new WebSocket(url);
      } catch {
        timerRef.current = setTimeout(connect, 2000);
        return;
      }
      wsRef.current = ws;

      ws.onopen = () => {
        if (cancelled) return;
        retryRef.current = 0;
        setConnected(true);
        if (onOpenRef.current) onOpenRef.current();
      };

      ws.onmessage = (ev) => {
        if (cancelled) return;
        let msg;
        try { msg = JSON.parse(ev.data); } catch { return; }
        if (onMessageRef.current) onMessageRef.current(msg, ev);
      };

      ws.onerror = () => { /* onclose schedules retry */ };

      ws.onclose = () => {
        wsRef.current = null;
        setConnected(false);
        if (onCloseRef.current) onCloseRef.current();
        if (cancelled) return;
        const delay = Math.min(backoffBaseMs * 2 ** retryRef.current, backoffMaxMs);
        retryRef.current += 1;
        timerRef.current = setTimeout(connect, delay);
      };
    };

    connect();

    // Optional wake-up: on online/visibility, drop the retry counter and
    // try again immediately. Without this, sustained outages can exhaust
    // the budget and leave a hook stranded until reload.
    let onlineHandler = null;
    let visHandler = null;
    if (respondToOnline) {
      const wake = () => {
        const ws = wsRef.current;
        if (ws && ws.readyState === WebSocket.OPEN) return;
        retryRef.current = 0;
        if (timerRef.current) {
          clearTimeout(timerRef.current);
          timerRef.current = null;
        }
        connect();
      };
      onlineHandler = wake;
      visHandler = () => { if (!document.hidden) wake(); };
      window.addEventListener('online', onlineHandler);
      document.addEventListener('visibilitychange', visHandler);
    }

    return () => {
      cancelled = true;
      if (timerRef.current) {
        clearTimeout(timerRef.current);
        timerRef.current = null;
      }
      if (onlineHandler) window.removeEventListener('online', onlineHandler);
      if (visHandler) document.removeEventListener('visibilitychange', visHandler);
      const ws = wsRef.current;
      if (ws) {
        ws.onclose = null;
        try { ws.close(); } catch { /* ignore */ }
        wsRef.current = null;
      }
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [path, enabled, backoffBaseMs, backoffMaxMs, respondToOnline, ...deps]);

  return { connected, getSocket: () => wsRef.current };
}
