import { useCallback, useEffect, useMemo, useState } from 'react';
import { api } from '../api/client.js';

/**
 * Areas of interest (core/aoiapi.c).
 *   GET  /api/aoi?limit&cursor → {data:[{id,name,kind,geometry,bbox,…}], page:{next_cursor,…}}
 *   POST /api/aoi { name, kind: 'bbox'|'polygon'|'circle', geometry }
 *     bbox    → [w, s, e, n]
 *     polygon → [[lon,lat], …]  (≥3 points; the server auto-closes the ring)
 *     circle  → { lat, lon, radius_m }
 * Every page is walked so the map draws the full set, not the first 50.
 */
export function aoiToGeoJSON(aoi) {
  const g = aoi?.geometry;
  const props = { aoi_id: aoi.id, name: aoi.name, kind: aoi.kind };
  if (!g) return null;
  if (aoi.kind === 'bbox' && Array.isArray(g) && g.length === 4) {
    const [w, s, e, n] = g.map(Number);
    return { type: 'Feature', properties: props, geometry: { type: 'Polygon', coordinates: [[[w, s], [e, s], [e, n], [w, n], [w, s]]] } };
  }
  if (aoi.kind === 'polygon' && Array.isArray(g)) {
    const ring = g.map((p) => [Number(p[0]), Number(p[1])]);
    if (ring.length && (ring[0][0] !== ring[ring.length - 1][0] || ring[0][1] !== ring[ring.length - 1][1])) ring.push(ring[0]);
    return { type: 'Feature', properties: props, geometry: { type: 'Polygon', coordinates: [ring] } };
  }
  if (aoi.kind === 'circle' && g && Number.isFinite(Number(g.lat))) {
    return { type: 'Feature', properties: { ...props, radius_m: Number(g.radius_m) }, geometry: circlePolygon(Number(g.lat), Number(g.lon), Number(g.radius_m)) };
  }
  return null;
}

/** 64-gon approximation of a geodesic circle, as a GeoJSON Polygon. */
export function circlePolygon(lat, lon, radiusM, n = 64) {
  const ring = [];
  const dLat = radiusM / 111320;
  const dLon = radiusM / (111320 * Math.cos((lat * Math.PI) / 180) || 1e-9);
  for (let i = 0; i <= n; i++) {
    const a = (i / n) * 2 * Math.PI;
    ring.push([lon + dLon * Math.cos(a), lat + dLat * Math.sin(a)]);
  }
  return { type: 'Polygon', coordinates: [ring] };
}

export default function useAoi({ enabled = true } = {}) {
  const [rows, setRows] = useState([]);
  const [error, setError] = useState(null);
  const [loading, setLoading] = useState(Boolean(enabled));
  // `enabled` turning on starts a load: flag it during render, not in the effect.
  const [prevEnabled, setPrevEnabled] = useState(enabled);
  if (prevEnabled !== enabled) { setPrevEnabled(enabled); if (enabled) setLoading(true); }
  const [truncated, setTruncated] = useState(false);

  // Read every page of /api/aoi; pure (no state), so the mount effect and the
  // imperative reload share it.
  const fetchAll = useCallback(async () => {
    const all = [];
    let cursor = null;
    let guard = 0;
    do {
      const j = await api.get('/api/aoi', { query: { limit: 200, cursor: cursor || undefined } });
      const data = Array.isArray(j?.data) ? j.data : [];
      all.push(...data);
      cursor = j?.page?.next_cursor || null;
      guard++;
    } while (cursor && guard < 50);
    return { all, truncated: Boolean(cursor) };
  }, []);

  const load = useCallback(async () => {
    if (!enabled) return;
    setLoading(true); setError(null);
    try {
      const { all, truncated: t } = await fetchAll();
      setTruncated(t);
      setRows(all);
    } catch (e) { setError(e); }
    finally { setLoading(false); }
  }, [enabled, fetchAll]);

  // State is set only from the async continuation, with cancellation.
  useEffect(() => {
    if (!enabled) return undefined;
    let alive = true;
    (async () => {
      try {
        const { all, truncated: t } = await fetchAll();
        if (!alive) return;
        setTruncated(t);
        setRows(all);
        setError(null);
      } catch (e) { if (alive) setError(e); }
      finally { if (alive) setLoading(false); }
    })();
    return () => { alive = false; };
  }, [enabled, fetchAll]);

  const create = useCallback(async ({ name, kind, geometry }) => {
    const j = await api.post('/api/aoi', { name, kind, geometry });
    await load();
    return j?.data ?? j;
  }, [load]);

  const geojson = useMemo(() => ({
    type: 'FeatureCollection',
    features: rows.map(aoiToGeoJSON).filter(Boolean),
  }), [rows]);

  return { rows, geojson, error, loading, truncated, reload: load, create };
}
