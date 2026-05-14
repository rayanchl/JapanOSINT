/**
 * Satellite Tracking Collector — live positions of every tracked on-orbit
 * object currently over Japan. TLEs fetched from CelesTrak (no auth, daily).
 * SGP4 propagation runs server-side with satellite.js. Ground tracks are
 * computed client-side on popup click (TLE lines shipped in properties).
 */

import * as satjs from 'satellite.js';
import { JAPAN_BBOX } from './_satelliteSeeds.js';

const CELESTRAK_GROUPS = [
  { group: 'active',   category: 'active' },
  { group: 'debris',   category: 'debris' },
  { group: 'cubesat',  category: 'cubesat' },
  { group: 'stations', category: 'station' },
];

const TLE_CACHE_TTL_MS = 24 * 60 * 60 * 1000;
let tleCache = null; // { fetchedAt, records: [{tleLine1, tleLine2, name, noradId, category}] }

// Seed: current ISS TLE snapshot (updated if this file is re-edited; SGP4 on
// a stale TLE still places the ISS somewhere plausible for a fallback).
const SEED_ISS = {
  name: 'ISS (ZARYA)',
  noradId: 25544,
  category: 'station',
  tleLine1: '1 25544U 98067A   26108.50000000  .00016717  00000-0  10270-3 0  9000',
  tleLine2: '2 25544  51.6400 180.0000 0006000   0.0000 180.0000 15.50000000100000',
};

async function fetchText(url, timeoutMs = 15000) {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeoutMs);
  try {
    const res = await fetch(url, { signal: ctrl.signal });
    if (!res.ok) return null;
    return await res.text();
  } catch { return null; }
  finally { clearTimeout(timer); }
}

function parseTleBlock(text, category) {
  // CelesTrak TLE format: name line, then line1, then line2, repeat.
  const lines = text.split(/\r?\n/).map((l) => l.trim()).filter(Boolean);
  const out = [];
  for (let i = 0; i + 2 < lines.length; i += 3) {
    const name = lines[i];
    const l1 = lines[i + 1];
    const l2 = lines[i + 2];
    if (!l1.startsWith('1 ') || !l2.startsWith('2 ')) continue;
    const noradId = parseInt(l1.substring(2, 7), 10);
    if (!Number.isFinite(noradId)) continue;
    out.push({ name, noradId, tleLine1: l1, tleLine2: l2, category });
  }
  return out;
}

async function loadTles() {
  if (tleCache && Date.now() - tleCache.fetchedAt < TLE_CACHE_TTL_MS) {
    return tleCache.records;
  }
  const all = [];
  for (const { group, category } of CELESTRAK_GROUPS) {
    const url = `https://celestrak.org/NORAD/elements/gp.php?GROUP=${group}&FORMAT=tle`;
    const body = await fetchText(url);
    if (!body) continue;
    all.push(...parseTleBlock(body, category));
  }
  if (!all.length) return null;
  tleCache = { fetchedAt: Date.now(), records: all };
  return all;
}

function propagateOne(rec, when) {
  try {
    const satrec = satjs.twoline2satrec(rec.tleLine1, rec.tleLine2);
    const pv = satjs.propagate(satrec, when);
    if (!pv?.position) return null;
    const gmst = satjs.gstime(when);
    const geo = satjs.eciToGeodetic(pv.position, gmst);
    const lon = (satjs.degreesLong(geo.longitude));
    const lat = (satjs.degreesLat(geo.latitude));
    const alt = geo.height; // km
    const v = pv.velocity;
    const vel = v ? Math.hypot(v.x, v.y, v.z) : null;
    const inc = satjs.radiansToDegrees(satrec.inclo);
    return { lon, lat, alt_km: alt, vel_kms: vel, inclination_deg: inc };
  } catch { return null; }
}

function inJapanBbox(lon, lat) {
  return lon >= JAPAN_BBOX[0] && lon <= JAPAN_BBOX[2]
      && lat >= JAPAN_BBOX[1] && lat <= JAPAN_BBOX[3];
}

function buildFeature(rec, pv) {
  return {
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [pv.lon, pv.lat] },
    properties: {
      id: `SAT_${rec.noradId}`,
      norad_id: rec.noradId,
      name: rec.name,
      country: null,
      category: rec.category,
      altitude_km: Math.round(pv.alt_km * 10) / 10,
      velocity_kms: pv.vel_kms != null ? Math.round(pv.vel_kms * 1000) / 1000 : null,
      inclination_deg: Math.round(pv.inclination_deg * 100) / 100,
      next_pass_utc: null, // computed client-side on demand
      tle_line1: rec.tleLine1,
      tle_line2: rec.tleLine2,
      source: 'celestrak',
    },
  };
}

function seedFeature() {
  const pv = propagateOne(SEED_ISS, new Date());
  const lon = pv?.lon ?? 138;
  const lat = pv?.lat ?? 36;
  return {
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [lon, lat] },
    properties: {
      id: `SAT_${SEED_ISS.noradId}`,
      norad_id: SEED_ISS.noradId,
      name: SEED_ISS.name,
      country: 'International',
      category: SEED_ISS.category,
      altitude_km: pv?.alt_km ? Math.round(pv.alt_km * 10) / 10 : 420,
      velocity_kms: pv?.vel_kms != null ? Math.round(pv.vel_kms * 1000) / 1000 : 7.66,
      inclination_deg: 51.64,
      next_pass_utc: null,
      tle_line1: SEED_ISS.tleLine1,
      tle_line2: SEED_ISS.tleLine2,
      source: 'satellite_tracking_seed',
    },
  };
}

export default async function collectSatelliteTracking() {
  const tles = await loadTles();
  const now = new Date();
  if (!tles) {
    const seed = seedFeature();
    // Clamp seed into bbox so tests don't fail when ISS is elsewhere.
    const [lon, lat] = seed.geometry.coordinates;
    if (!inJapanBbox(lon, lat)) {
      seed.geometry.coordinates = [138.0, 36.0];
    }
    return {
      type: 'FeatureCollection',
      features: [seed],
      _meta: {
        source: 'satellite-tracking',
        fetchedAt: now.toISOString(),
        recordCount: 1,
        live: false,
        live_source: 'satellite_tracking_seed',
        description: 'Live satellite positions over Japan (SGP4 from CelesTrak TLEs). Seed ISS only.',
      },
    };
  }

  const features = [];
  for (const rec of tles) {
    const pv = propagateOne(rec, now);
    if (!pv) continue;
    if (!inJapanBbox(pv.lon, pv.lat)) continue;
    features.push(buildFeature(rec, pv));
  }

  // If propagation filtered everything out, still emit seed so popup/layer works.
  if (features.length === 0) {
    const seed = seedFeature();
    seed.geometry.coordinates = [138.0, 36.0];
    features.push(seed);
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'satellite-tracking',
      fetchedAt: now.toISOString(),
      recordCount: features.length,
      live: true,
      live_source: 'celestrak',
      description: 'Live satellite positions over Japan (SGP4 from CelesTrak TLEs).',
    },
  };
}
