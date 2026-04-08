import { useState, useEffect, useRef, useCallback } from 'react';

export default function useDataSources() {
  const [sources, setSources] = useState([]);
  const [stats, setStats] = useState(null);
  const [isConnected, setIsConnected] = useState(false);
  const [lastUpdate, setLastUpdate] = useState(null);
  const wsRef = useRef(null);
  const reconnectTimeout = useRef(null);
  const reconnectAttempts = useRef(0);
  const maxReconnectAttempts = 10;

  const fetchSources = useCallback(async () => {
    try {
      const [sourcesRes, statsRes] = await Promise.all([
        fetch('/api/sources'),
        fetch('/api/sources/stats'),
      ]);

      if (sourcesRes.ok) {
        const data = await sourcesRes.json();
        setSources(Array.isArray(data) ? data : data.sources || []);
      }

      if (statsRes.ok) {
        const data = await statsRes.json();
        setStats(data);
      }

      setLastUpdate(new Date().toISOString());
    } catch (err) {
      console.warn('[useDataSources] Failed to fetch sources:', err.message);
    }
  }, []);

  const connectWebSocket = useCallback(() => {
    if (wsRef.current?.readyState === WebSocket.OPEN) return;

    try {
      const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
      const wsUrl = `${protocol}//localhost:4000`;
      const ws = new WebSocket(wsUrl);

      ws.onopen = () => {
        console.log('[WebSocket] Connected');
        setIsConnected(true);
        reconnectAttempts.current = 0;
      };

      ws.onmessage = (event) => {
        try {
          const message = JSON.parse(event.data);

          switch (message.type) {
            case 'source_update':
              setSources((prev) => {
                const idx = prev.findIndex((s) => s.id === message.data?.id);
                if (idx >= 0) {
                  const updated = [...prev];
                  updated[idx] = { ...updated[idx], ...message.data };
                  return updated;
                }
                return prev;
              });
              setLastUpdate(new Date().toISOString());
              break;

            case 'stats_update':
              setStats(message.data);
              break;

            case 'sources_refresh':
              if (Array.isArray(message.data)) {
                setSources(message.data);
              }
              setLastUpdate(new Date().toISOString());
              break;

            default:
              break;
          }
        } catch (err) {
          console.warn('[WebSocket] Failed to parse message:', err.message);
        }
      };

      ws.onerror = (err) => {
        console.warn('[WebSocket] Error:', err);
      };

      ws.onclose = () => {
        console.log('[WebSocket] Disconnected');
        setIsConnected(false);
        wsRef.current = null;

        if (reconnectAttempts.current < maxReconnectAttempts) {
          const delay = Math.min(1000 * Math.pow(2, reconnectAttempts.current), 30000);
          reconnectAttempts.current += 1;
          reconnectTimeout.current = setTimeout(connectWebSocket, delay);
        }
      };

      wsRef.current = ws;
    } catch (err) {
      console.warn('[WebSocket] Connection failed:', err.message);
    }
  }, []);

  useEffect(() => {
    fetchSources();
    connectWebSocket();

    const pollInterval = setInterval(fetchSources, 60000);

    return () => {
      clearInterval(pollInterval);
      if (reconnectTimeout.current) clearTimeout(reconnectTimeout.current);
      if (wsRef.current) {
        wsRef.current.onclose = null;
        wsRef.current.close();
      }
    };
  }, [fetchSources, connectWebSocket]);

  return { sources, stats, isConnected, lastUpdate };
}
