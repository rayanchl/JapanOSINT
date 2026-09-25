import { useCallback, useEffect, useRef, useState } from 'react';
import { api, ApiError } from '../api/client.js';

/**
 * GET /api/isochrone — GTFS travel-time reachability (roadmap 32).
 * Params the server reads (core/isochrone.c): lat, lon, max_min (5…ceil),
 * max_walk_m (100…2000), walk_kmh (2…7), max_transfers (0…4), bands (csv of
 * minutes), depart (ISO, JST service day). Rate-limited 12/min per client:
 * a 429 carries `retry_after_sec`, which becomes a countdown here exactly as
 * the iOS overlay does.
 *
 * Response: a FeatureCollection of nested MultiPolygon bands
 * (`properties.band_min`, `stops_within`) + `meta` (origin, depart_at,
 * service_date, weekday, params, stops_*, truncated, note…). An origin with
 * no stops in reach is an EMPTY collection with `meta.note` — walking-only —
 * not an error.
 */
export default function useIsochrone() {
  const [result, setResult] = useState(null);
  const [error, setError] = useState(null);
  const [running, setRunning] = useState(false);
  const [retryAfter, setRetryAfter] = useState(null);
  const timer = useRef(null);

  useEffect(() => () => { if (timer.current) clearInterval(timer.current); }, []);

  const countDown = useCallback((seconds) => {
    if (timer.current) clearInterval(timer.current);
    let s = Math.max(1, seconds | 0);
    setRetryAfter(s);
    timer.current = setInterval(() => {
      s -= 1;
      if (s <= 0) { clearInterval(timer.current); timer.current = null; setRetryAfter(null); }
      else setRetryAfter(s);
    }, 1000);
  }, []);

  const compute = useCallback(async ({ lat, lon, maxMin, maxWalkM, walkKmh, maxTransfers, bands, depart }) => {
    setRunning(true); setError(null);
    try {
      const j = await api.get('/api/isochrone', {
        query: {
          lat, lon,
          max_min: maxMin,
          max_walk_m: maxWalkM,
          walk_kmh: walkKmh,
          max_transfers: maxTransfers,
          bands: Array.isArray(bands) && bands.length ? bands.join(',') : undefined,
          depart: depart || undefined,
        },
      });
      const features = Array.isArray(j?.features) ? j.features : [];
      // Widest band first is the server's order; sort defensively by band_min desc.
      const bandsOut = features
        .filter((f) => f?.geometry && Number.isFinite(Number(f?.properties?.band_min)))
        .sort((a, b) => Number(b.properties.band_min) - Number(a.properties.band_min));
      setResult({ fc: { type: 'FeatureCollection', features: bandsOut }, bands: bandsOut, meta: j?.meta || {} });
      return j;
    } catch (e) {
      if (e instanceof ApiError && e.status === 429) {
        const s = Number(e.body?.retry_after_sec) || 10;
        countDown(s);
      }
      setError(e);
      return null;
    } finally { setRunning(false); }
  }, [countDown]);

  const clear = useCallback(() => { setResult(null); setError(null); }, []);

  return { result, error, running, retryAfter, compute, clear };
}
