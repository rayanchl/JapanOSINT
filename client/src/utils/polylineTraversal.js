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

/**
 * Advance from {segIdx, segOffset} by deltaMeters along `coords`. Wraps to
 * segIdx=0 when the polyline ends so the vehicle loops.
 * Returns { lng, lat, bearing, segIdx, segOffset }.
 *
 * Assumes segLens.length === coords.length - 1 and segLens.length > 0.
 */
export function advanceAlongLine(coords, segLens, state, deltaMeters) {
  let segIdx = state.segIdx;
  let segOffset = state.segOffset + deltaMeters;
  // Walk forward across segments, wrapping at the end.
  while (segOffset > segLens[segIdx]) {
    segOffset -= segLens[segIdx];
    segIdx = (segIdx + 1) % segLens.length;
  }
  const a = coords[segIdx];
  const b = coords[segIdx + 1];
  const t = segLens[segIdx] === 0 ? 0 : segOffset / segLens[segIdx];
  return {
    lng: a[0] + t * (b[0] - a[0]),
    lat: a[1] + t * (b[1] - a[1]),
    bearing: bearingDeg(a, b),
    segIdx,
    segOffset,
  };
}
