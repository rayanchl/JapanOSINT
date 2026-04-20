// Geodesic helpers for advancing a vehicle along a polyline.
//
// Used by the live-transit simulator: given a LineString's [lng, lat]
// coordinates, precompute per-segment lengths once, then call
// advanceAlongLine each tick to move a vehicle forward by deltaMeters. At
// the end of the polyline the vehicle wraps back to segment 0 so it loops.

const EARTH_R = 6_371_000; // meters

function toRad(d) { return (d * Math.PI) / 180; }
function toDeg(r) { return (r * 180) / Math.PI; }

function haversineMeters(a, b) {
  const dLat = toRad(b[1] - a[1]);
  const dLon = toRad(b[0] - a[0]);
  const lat1 = toRad(a[1]);
  const lat2 = toRad(b[1]);
  const h =
    Math.sin(dLat / 2) ** 2 +
    Math.cos(lat1) * Math.cos(lat2) * Math.sin(dLon / 2) ** 2;
  return 2 * EARTH_R * Math.asin(Math.sqrt(h));
}

function bearingDeg(a, b) {
  const lat1 = toRad(a[1]);
  const lat2 = toRad(b[1]);
  const dLon = toRad(b[0] - a[0]);
  const y = Math.sin(dLon) * Math.cos(lat2);
  const x = Math.cos(lat1) * Math.sin(lat2) -
            Math.sin(lat1) * Math.cos(lat2) * Math.cos(dLon);
  return (toDeg(Math.atan2(y, x)) + 360) % 360;
}

/** One length per segment (length N-1 for N coords). */
export function segmentLengthsMeters(coords) {
  const out = new Array(coords.length - 1);
  for (let i = 0; i < out.length; i++) {
    out[i] = haversineMeters(coords[i], coords[i + 1]);
  }
  return out;
}

// Catmull-Rom interpolation between p1 and p2 using p0 and p3 as tangent
// controls. `t` is 0..1 within the [p1, p2] segment. Produces a curve that
// passes through p1 and p2, bending toward the neighboring vertices —
// vehicles no longer visibly snap between chord endpoints on a coarse
// polyline.
function catmullRom(p0, p1, p2, p3, t) {
  const t2 = t * t;
  const t3 = t2 * t;
  const lng = 0.5 * (
    (2 * p1[0]) +
    (-p0[0] + p2[0]) * t +
    (2 * p0[0] - 5 * p1[0] + 4 * p2[0] - p3[0]) * t2 +
    (-p0[0] + 3 * p1[0] - 3 * p2[0] + p3[0]) * t3
  );
  const lat = 0.5 * (
    (2 * p1[1]) +
    (-p0[1] + p2[1]) * t +
    (2 * p0[1] - 5 * p1[1] + 4 * p2[1] - p3[1]) * t2 +
    (-p0[1] + 3 * p1[1] - 3 * p2[1] + p3[1]) * t3
  );
  return [lng, lat];
}

/**
 * Advance from {segIdx, segOffset} by deltaMeters along `coords`. Wraps to
 * segIdx=0 when the polyline ends so the vehicle loops.
 *
 * Position is interpolated via Catmull-Rom using the two neighboring
 * vertices as curve controls — at polyline endpoints the tangents are
 * reflected so start/end remain well-defined.
 *
 * `deltaMeters` must be >= 0 — negative values produce undefined output.
 *
 * Returns { lng, lat, bearing, segIdx, segOffset }.
 *
 * Assumes segLens.length === coords.length - 1 and segLens.length > 0.
 */
export function advanceAlongLine(coords, segLens, state, deltaMeters) {
  const n = segLens.length;
  let segIdx = state.segIdx;
  let segOffset = state.segOffset + deltaMeters;
  // Walk forward across segments, wrapping at the end. Break if we've done
  // a full lap without consuming any distance (all segments are zero-length,
  // e.g. a polyline of duplicated coordinates).
  let safetySteps = 0;
  while (segOffset > segLens[segIdx]) {
    segOffset -= segLens[segIdx];
    segIdx = (segIdx + 1) % n;
    safetySteps++;
    if (safetySteps > n) break;
  }
  const p1 = coords[segIdx];
  const p2 = coords[segIdx + 1];
  const t = segLens[segIdx] === 0 ? 0 : segOffset / segLens[segIdx];
  // Reflect p1/p2 to keep the curve well-defined at the polyline endpoints.
  const p0 = coords[segIdx - 1] || [2 * p1[0] - p2[0], 2 * p1[1] - p2[1]];
  const p3 = coords[segIdx + 2] || [2 * p2[0] - p1[0], 2 * p2[1] - p1[1]];
  const [lng, lat] = catmullRom(p0, p1, p2, p3, t);
  return {
    lng,
    lat,
    bearing: bearingDeg(p1, p2),
    segIdx,
    segOffset,
  };
}
