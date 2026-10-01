import { useState, useEffect, useCallback } from 'react';
import useWebSocket from './useWebSocket.js';
import apiUrl from '../utils/apiUrl.js';
import { normalizeSource, normalizeSources } from '../utils/normalizeSource.js';

export default function useDataSources() {
  const [sources, setSources] = useState([]);
  const [stats, setStats] = useState(null);
  const [lastUpdate, setLastUpdate] = useState(null);
  // What actually happened on the last poll. Everything downstream (the nav
  // bar's "active sources" figure, the Source Monitor's health dot) reads a
  // number that is only meaningful if the request behind it succeeded, and a
  // silent console.warn left those numbers looking freshly obtained.
  const [error, setError] = useState(null);

  const fetchSources = useCallback(async () => {
    try {
      const [sourcesRes, statsRes] = await Promise.all([
        fetch(apiUrl('/api/sources')),
        fetch(apiUrl('/api/sources/stats')),
      ]);

      const failures = [];

      if (sourcesRes.ok) {
        const data = await sourcesRes.json();
        // /api/sources emits raw sqlite column names — normalise on entry.
        setSources(normalizeSources(Array.isArray(data) ? data : data.sources || []));
      } else {
        failures.push(`/api/sources HTTP ${sourcesRes.status}`);
      }

      if (statsRes.ok) {
        const data = await statsRes.json();
        setStats(data);
      } else {
        failures.push(`/api/sources/stats HTTP ${statsRes.status}`);
      }

      setError(failures.length ? failures.join(', ') : null);
      // lastUpdate is the timestamp of data we actually hold. Stamping it on a
      // failed poll dated stale rows to "now".
      if (failures.length < 2) setLastUpdate(new Date().toISOString());
    } catch (err) {
      console.warn('[useDataSources] Failed to fetch sources:', err.message);
      setError(err.message || 'request failed');
    }
  }, []);

  const onMessage = useCallback((message) => {
    switch (message.type) {
      case 'source_update':
        setSources((prev) => {
          const idx = prev.findIndex((s) => s.id === message.data?.id);
          if (idx >= 0) {
            const updated = [...prev];
            updated[idx] = { ...updated[idx], ...normalizeSource(message.data) };
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
          setSources(normalizeSources(message.data));
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

  return { sources, stats, isConnected: connected, lastUpdate, error };
}
