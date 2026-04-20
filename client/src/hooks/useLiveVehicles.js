// Live-transit vehicle simulator. Fetches /api/transit/routes for the given
// mode, spawns vehicles along each route, advances them once per second via
// requestAnimationFrame, and returns a GeoJSON FeatureCollection the MapView
// can push to a map source via `setData`.
//
// Slice A (constant-speed) and Slice C (schedule-grounded) BOTH feed the
// same emitted FeatureCollection. The tick loop advances Slice-A vehicles;
// a parallel 20-second poll against /api/transit/active-trips fills
// `scheduleTripsRef` with real GTFS-backed positions. When a schedule-backed
// trip covers a given route, its position takes priority over the simulator.

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
// Render a fresh frame at most every RENDER_MS; motion math runs every rAF
// so the vehicles move smoothly, but we only re-serialize the GeoJSON at
// this cadence to keep setState / source.setData cheap.
const RENDER_MS = 100;

// Darken a #rrggbb color by multiplying each channel so the rendered
// vehicle reads a bit deeper than the line itself (user request: trains
// should be visibly darker than their line color).
function darkenHex(hex, factor = 0.65) {
  if (typeof hex !== 'string') return hex;
  const m = hex.trim().match(/^#?([0-9a-f]{6})$/i);
  if (!m) return hex;
  const n = parseInt(m[1], 16);
  const r = Math.round(((n >> 16) & 0xff) * factor);
  const g = Math.round(((n >> 8) & 0xff) * factor);
  const b = Math.round((n & 0xff) * factor);
  const to2 = (v) => v.toString(16).padStart(2, '0');
  return `#${to2(r)}${to2(g)}${to2(b)}`;
}

function spawnVehiclesForRoute(feature, mode) {
  const coords = feature?.geometry?.coordinates;
  if (!Array.isArray(coords) || coords.length < 2) return [];
  const segLens = segmentLengthsMeters(coords);
  const total = segLens.reduce((a, b) => a + b, 0);
  if (total < MIN_ROUTE_LENGTH_M) return [];
  const spacing = VEHICLE_SPACING_M[mode];
  const n = Math.min(MAX_VEHICLES_PER_ROUTE, Math.max(1, Math.floor(total / spacing)));
  const rawColor = feature.properties?.line_color || null;
  const color = rawColor ? darkenHex(rawColor, 0.8) : null;
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

export default function useLiveVehicles(mode, enabled, viewport = null) {
  const [geojson, setGeojson] = useState({ type: 'FeatureCollection', features: [] });
  const vehiclesRef = useRef([]);
  const scheduleTripsRef = useRef([]);
  const rafRef = useRef(null);
  const lastTickRef = useRef(null);

  // Stable viewport key — the poll effect only re-fires when the string
  // representation actually changes, not on every rerender.
  const viewportKey = viewport
    ? `${viewport.minLng.toFixed(3)},${viewport.minLat.toFixed(3)},${viewport.maxLng.toFixed(3)},${viewport.maxLat.toFixed(3)}`
    : null;

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

  // Slice C — poll /api/transit/active-trips every 20s for schedule-backed
  // positions. Today only the bus mode has any GTFS coverage via gtfs-data.jp,
  // but we hit the endpoint for every mode and just merge whatever comes
  // back; as train/subway GTFS feeds are added the same code picks them up.
  useEffect(() => {
    if (!enabled) { scheduleTripsRef.current = []; return; }
    let cancelled = false;
    let timer = null;
    async function poll() {
      if (cancelled) return;
      try {
        const qs = new URLSearchParams();
        if (viewportKey) qs.set('bbox', viewportKey);
        qs.set('limit', '500');
        const res = await fetch(`/api/transit/active-trips?${qs.toString()}`);
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const body = await res.json();
        if (!cancelled) scheduleTripsRef.current = body.trips || [];
      } catch (err) {
        if (!cancelled) console.warn('[useLiveVehicles] active-trips', err?.message);
      } finally {
        if (!cancelled) timer = setTimeout(poll, 20_000);
      }
    }
    poll();
    return () => {
      cancelled = true;
      if (timer) clearTimeout(timer);
      scheduleTripsRef.current = [];
    };
  }, [mode, enabled, viewportKey]);

  // rAF tick loop — advance motion every frame using real dt so movement is
  // smooth. Serialize GeoJSON at most every RENDER_MS to avoid thrashing
  // setState / source.setData at 60 Hz.
  useEffect(() => {
    if (!enabled) return;
    const speed = MODE_SPEED_MPS[mode];
    let lastRenderAt = 0;
    const tick = (t) => {
      if (lastTickRef.current === null) lastTickRef.current = t;
      const dt = (t - lastTickRef.current) / 1000; // seconds since last frame
      lastTickRef.current = t;
      const delta = speed * dt;

      // Always advance positions — motion stays smooth at display refresh.
      for (const v of vehiclesRef.current) {
        const next = advanceAlongLine(
          v.coords,
          v.segLens,
          { segIdx: v.segIdx, segOffset: v.segOffset },
          delta,
        );
        v.segIdx = next.segIdx;
        v.segOffset = next.segOffset;
        v.lng = next.lng;
        v.lat = next.lat;
        v.bearing = next.bearing;
      }

      // Throttle the GeoJSON rebuild. 100 ms = 10 Hz is smooth enough to
      // look continuous without hammering MapLibre's parser.
      if (t - lastRenderAt >= RENDER_MS) {
        lastRenderAt = t;
        // Schedule-backed trips override the sim for their route_id so we
        // don't double-render a bus (simulator + real position) on the
        // same route. Any route_id we haven't seen in schedule data falls
        // back to the simulator vehicles.
        const scheduledRouteIds = new Set();
        for (const st of scheduleTripsRef.current) {
          if (st.route_id) scheduledRouteIds.add(st.route_id);
        }
        const simVehicles = vehiclesRef.current;
        const estimatedCount = scheduleTripsRef.current.length + simVehicles.length;
        const features = [];
        features.length = 0;
        // (a) simulator vehicles for routes with no schedule data
        for (const v of simVehicles) {
          if (v.routeId && scheduledRouteIds.has(v.routeId)) continue;
          features.push({
            type: 'Feature',
            geometry: { type: 'Point', coordinates: [v.lng, v.lat] },
            properties: {
              route_id: v.routeId,
              mode: v.mode,
              line_color: v.color,
              bearing: v.bearing,
            },
          });
        }
        // (b) schedule-backed positions
        for (const st of scheduleTripsRef.current) {
          features.push({
            type: 'Feature',
            geometry: { type: 'Point', coordinates: [st.lon, st.lat] },
            properties: {
              trip_id: st.trip_id,
              route_id: st.route_id,
              mode,
              line_color: st.route_color,
              headsign: st.headsign || null,
              schedule_backed: true,
            },
          });
        }
        if (features.length > 0 || estimatedCount > 0) {
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
