import { useState, useEffect, useCallback } from 'react';
import useWebSocket from './useWebSocket.js';
import apiUrl from '../utils/apiUrl.js';

/**
 * The shell's fleet figure: /api/sources/stats polled every 60 s, plus the
 * WebSocket's stats_update. It used to ALSO fetch the full /api/sources dump
 * (18,883 rows, every 60 s, on every page) for a dashboard that has since
 * moved to the server-searched /console/sources page; nothing in the shell
 * read those rows. `error` names a failed poll so the nav figure is never a
 * stale number presented as fresh.
 */
export default function useDataSources() {
  const [stats, setStats] = useState(null);
  const [lastUpdate, setLastUpdate] = useState(null);
  const [error, setError] = useState(null);

  const fetchStats = useCallback(async () => {
    try {
      const res = await fetch(apiUrl('/api/sources/stats'));
      if (!res.ok) { setError(`/api/sources/stats HTTP ${res.status}`); return; }
      setStats(await res.json());
      setError(null);
      setLastUpdate(new Date().toISOString());
    } catch (err) {
      console.warn('[useDataSources] Failed to fetch source stats:', err.message);
      setError(err.message || 'request failed');
    }
  }, []);

  const onMessage = useCallback((message) => {
    if (message.type === 'stats_update') { setStats(message.data); setLastUpdate(new Date().toISOString()); }
  }, []);

  const { connected } = useWebSocket('/ws', { onMessage, respondToOnline: true });

  useEffect(() => {
    fetchStats();
    const pollInterval = setInterval(fetchStats, 60000);
    return () => clearInterval(pollInterval);
  }, [fetchStats]);

  return { stats, isConnected: connected, lastUpdate, error };
}
