/**
 * Compute a 90-minute forward ground track for a satellite given its TLE.
 * Returns a GeoJSON LineString in [lon, lat] order.
 *
 * Uses satellite.js for SGP4 propagation. Sampled every 30 seconds.
 */
import * as satjs from 'satellite.js';

export function computeGroundTrack(tleLine1, tleLine2, { minutes = 90, stepSec = 30 } = {}) {
  const satrec = satjs.twoline2satrec(tleLine1, tleLine2);
  // satellite.js sets `error` to a non-zero SGP4 error code on malformed
  // TLE input. Bail early instead of looping 180 times with `propagate`
  // returning empty results.
  if (!satrec || satrec.error !== 0) {
    return { type: 'LineString', coordinates: [] };
  }
  const start = new Date();
  const end = new Date(start.getTime() + minutes * 60 * 1000);
  const coords = [];
  let prevLon = null;
  for (let t = start.getTime(); t <= end.getTime(); t += stepSec * 1000) {
    const when = new Date(t);
    let pv;
    try {
      pv = satjs.propagate(satrec, when);
    } catch {
      continue;
    }
    if (!pv?.position) continue;
    const gmst = satjs.gstime(when);
    const geo = satjs.eciToGeodetic(pv.position, gmst);
    const lon = satjs.degreesLong(geo.longitude);
    const lat = satjs.degreesLat(geo.latitude);
    if (!Number.isFinite(lat) || !Number.isFinite(lon)) continue;
    // Split the polyline at the antimeridian to avoid the
    // "line wraps halfway around the world" artefact.
    if (prevLon !== null && Math.abs(lon - prevLon) > 180) {
      coords.push(null); // segment break
    }
    coords.push([lon, lat]);
    prevLon = lon;
  }

  // Convert into a MultiLineString if there were segment breaks.
  const segments = [];
  let current = [];
  for (const c of coords) {
    if (c === null) {
      if (current.length > 1) segments.push(current);
      current = [];
    } else {
      current.push(c);
    }
  }
  if (current.length > 1) segments.push(current);

  if (segments.length === 1) {
    return { type: 'LineString', coordinates: segments[0] };
  }
  return { type: 'MultiLineString', coordinates: segments };
}
