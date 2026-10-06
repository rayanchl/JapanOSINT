import { useEffect, useState } from 'react';

/**
 * Project a geographic coordinate to screen pixels on a MapLibre map,
 * keeping the result in sync with pan/zoom/rotate. Returns `null` until
 * the map is ready or when `lngLat` is not a finite pair.
 */
export default function useMapProjection(mapRef, lngLat) {
  const [pos, setPos] = useState(null);
  // Depend on the two NUMBERS, never on the array's identity. Callers build
  // `[lon, lat]` inline, and with the array as the dependency every render
  // re-ran this effect, whose synchronous setPos() forced another render —
  // an unbounded render loop (~130k renders per 500 ms behind the reverse-
  // geocode card in MapPage).
  const lng = Array.isArray(lngLat) ? lngLat[0] : undefined;
  const lat = Array.isArray(lngLat) ? lngLat[1] : undefined;

  useEffect(() => {
    const map = mapRef?.current;
    if (!map || lng === undefined || lat === undefined) {
      setPos(null);
      return undefined;
    }
    if (!Number.isFinite(lng) || !Number.isFinite(lat)) {
      setPos(null);
      return undefined;
    }

    let frame = 0;
    const update = () => {
      frame = 0;
      const p = map.project([lng, lat]);
      setPos({ x: p.x, y: p.y });
    };
    const schedule = () => {
      if (frame) return;
      frame = requestAnimationFrame(update);
    };

    update();
    map.on('move', schedule);
    map.on('moveend', update);
    map.on('resize', update);

    return () => {
      if (frame) cancelAnimationFrame(frame);
      map.off('move', schedule);
      map.off('moveend', update);
      map.off('resize', update);
    };
  }, [mapRef, lng, lat]);

  return pos;
}
