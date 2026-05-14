import { useState, useEffect, useCallback } from 'react';
import useWebSocket from './useWebSocket.js';
import apiUrl from '../utils/apiUrl.js';

export default function useDataSources() {
  const [sources, setSources] = useState([]);
  const [stats, setStats] = useState(null);
  const [lastUpdate, setLastUpdate] = useState(null);

  const fetchSources = useCallback(async () => {
    try {
      const [sourcesRes, statsRes] = await Promise.all([
        fetch(apiUrl('/api/sources')),
        fetch(apiUrl('/api/sources/stats')),
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

  const onMessage = useCallback((message) => {
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
  }, []);

  // 10 reconnect attempts at base/max -> map onto useWebSocket's 30s cap.
  // Long-lived dashboard hook benefits from online/visibility wake-ups.
  const { connected } = useWebSocket('/ws', {
    onMessage,
    respondToOnline: true,
  });

  useEffect(() => {
    fetchSources();
    const pollInterval = setInterval(fetchSources, 60000);
    return () => clearInterval(pollInterval);
  }, [fetchSources]);

  return { sources, stats, isConnected: connected, lastUpdate };
}
