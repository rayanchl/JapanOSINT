// Live-transit vehicle simulator. Fetches /api/transit/routes for the given
// mode, spawns vehicles along each route, advances them once per second via
// requestAnimationFrame, and returns a GeoJSON FeatureCollection the MapView
// can push to a map source via `setData`.
//
// This is Slice A: constant-speed simulation without schedule. Slice C will
// swap the tick logic for schedule-grounded interpolation when GTFS is
// hydrated for the operator.

import { useEffect, useRef, useState } from 'react';
import { segmentLengthsMeters, advanceAlongLine } from '../utils/polylineTraversal';

// Mode-specific constants.
const MODE_SPEED_MPS = {
  train: (60 * 1000) / 3600,   // 60 km/h — Shinkansen average including stops
  subway: (40 * 1000) / 3600,  // 40 km/h
  bus: (25 * 1000) / 3600,     // 25 km/h
};
const VEHICLE_SPACING_M = {
  train: 6_000,
  subway: 3_000,
  bus: 4_000,
};
const MAX_VEHICLES_PER_ROUTE = 25;
// Skip tiny branches/connectors; a ~500 m "line" is almost always a stub.
const MIN_ROUTE_LENGTH_M = 500;
const TICK_MS = 1000;

function spawnVehiclesForRoute(feature, mode) {
  const coords = feature?.geometry?.coordinates;
  if (!Array.isArray(coords) || coords.length < 2) return [];
  const segLens = segmentLengthsMeters(coords);
  const total = segLens.reduce((a, b) => a + b, 0);
  if (total < MIN_ROUTE_LENGTH_M) return [];
  const spacing = VEHICLE_SPACING_M[mode];
  const n = Math.min(MAX_VEHICLES_PER_ROUTE, Math.max(1, Math.floor(total / spacing)));
  const color = feature.properties?.line_color || null;
  const routeId = feature.properties?.route_id || null;
  const vehicles = [];
  for (let i = 0; i < n; i++) {
    // Evenly spaced along the total length; walk forward through segments
    // to find the starting (segIdx, segOffset).
    const nSegs = segLens.length;
    let segIdx = 0;
    let segOffset = (total * i) / n;
    let safetySteps = 0;
    while (segOffset > segLens[segIdx]) {
      segOffset -= segLens[segIdx];
      segIdx = (segIdx + 1) % nSegs;
      safetySteps++;
      if (safetySteps > nSegs) break;
    }
    vehicles.push({ routeId, mode, color, coords, segLens, segIdx, segOffset });
  }
  return vehicles;
}

export default function useLiveVehicles(mode, enabled) {
  const [geojson, setGeojson] = useState({ type: 'FeatureCollection', features: [] });
  const vehiclesRef = useRef([]);
  const rafRef = useRef(null);
  const lastTickRef = useRef(null);

  // Fetch routes + spawn vehicles when the hook becomes enabled (or the mode
  // changes). Clears state when disabled.
  useEffect(() => {
    if (!enabled) {
      vehiclesRef.current = [];
      setGeojson({ type: 'FeatureCollection', features: [] });
      return;
    }
    let cancelled = false;
    (async () => {
      try {
        const res = await fetch(`/api/transit/routes?mode=${encodeURIComponent(mode)}`);
        const data = await res.json();
        if (cancelled) return;
        const spawned = [];
        for (const f of data.features || []) {
          const routeVehicles = spawnVehiclesForRoute(f, mode);
          spawned.push(...routeVehicles);
        }
        vehiclesRef.current = spawned;
      } catch (err) {
        console.error('[useLiveVehicles] routes fetch failed', err);
      }
    })();
    return () => { cancelled = true; };
  }, [mode, enabled]);

  // rAF tick loop — throttled to TICK_MS so we don't thrash setState at 60 Hz.
  useEffect(() => {
    if (!enabled) return;
    const speed = MODE_SPEED_MPS[mode];
    const tick = (t) => {
      if (lastTickRef.current === null) lastTickRef.current = t;
      const dt = (t - lastTickRef.current) / 1000;
      if (dt >= TICK_MS / 1000) {
        lastTickRef.current = t;
        const delta = speed * dt;
        const features = [];
        for (const v of vehiclesRef.current) {
          const next = advanceAlongLine(
            v.coords,
            v.segLens,
            { segIdx: v.segIdx, segOffset: v.segOffset },
            delta,
          );
          v.segIdx = next.segIdx;
          v.segOffset = next.segOffset;
          features.push({
            type: 'Feature',
            geometry: { type: 'Point', coordinates: [next.lng, next.lat] },
            properties: {
              route_id: v.routeId,
              mode: v.mode,
              line_color: v.color,
              bearing: next.bearing,
            },
          });
        }
        // Skip setState when there's nothing to render — avoids empty
        // re-renders during the fetch window or when the layer is empty.
        if (features.length > 0 || vehiclesRef.current.length > 0) {
          setGeojson({ type: 'FeatureCollection', features });
        }
      }
      rafRef.current = requestAnimationFrame(tick);
    };
    rafRef.current = requestAnimationFrame(tick);
    return () => {
      if (rafRef.current) cancelAnimationFrame(rafRef.current);
      rafRef.current = null;
      lastTickRef.current = null;
    };
  }, [mode, enabled]);

  return geojson;
}
