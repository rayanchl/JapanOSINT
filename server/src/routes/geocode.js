/**
 * Geocoding routes (forward + reverse) with multi-provider fallback.
 *
 * Providers tried in order:
 *   1. Nominatim (OSM, primary)
 *   2. Photon (Komoot, OSM-backed)
 *   3. GSI Address API (Japan government, reverse only; forward via MunicipalityMap)
 *
 * All providers return normalised hits: { lat, lon, display_name, source }.
 */

import { Router } from 'express';

const router = Router();

const USER_AGENT = 'JapanOSINT/1.0 (github.com/rayanchl/JapanOSINT)';

// ── Simple in-memory TTL cache ─────────────────────────────────────────────
const geocodeCache = new Map(); // key -> { value, expiresAt }
const CACHE_TTL_MS = 24 * 60 * 60 * 1000; // 24h

function cacheGet(key) {
  const hit = geocodeCache.get(key);
  if (!hit) return null;
  if (hit.expiresAt < Date.now()) {
    geocodeCache.delete(key);
    return null;
  }
  return hit.value;
}
function cacheSet(key, value) {
  geocodeCache.set(key, { value, expiresAt: Date.now() + CACHE_TTL_MS });
}

async function fetchJson(url, { timeoutMs = 6000, headers = {} } = {}) {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeoutMs);
  try {
    const res = await fetch(url, {
      headers: { 'User-Agent': USER_AGENT, Accept: 'application/json', ...headers },
      signal: ctrl.signal,
    });
    clearTimeout(timer);
    if (!res.ok) return null;
    return await res.json();
  } catch {
    return null;
  }
}

// ── Forward geocoding providers ────────────────────────────────────────────
async function forwardNominatim(q) {
  const url = `https://nominatim.openstreetmap.org/search?format=json&addressdetails=1&limit=5&countrycodes=jp&q=${encodeURIComponent(q)}`;
  const data = await fetchJson(url);
  if (!Array.isArray(data) || data.length === 0) return null;
  return data.map((r) => ({
    lat: parseFloat(r.lat),
    lon: parseFloat(r.lon),
    display_name: r.display_name,
    type: r.type || r.class,
    source: 'nominatim',
  }));
}

async function forwardPhoton(q) {
  // Photon is OSM-backed. Filter to Japan by bbox (approx).
  const url = `https://photon.komoot.io/api/?q=${encodeURIComponent(q)}&limit=5&bbox=122,24,154,46`;
  const data = await fetchJson(url);
  const feats = data?.features;
  if (!Array.isArray(feats) || feats.length === 0) return null;
  return feats
    .filter((f) => (f.properties?.countrycode || '').toUpperCase() === 'JP')
    .map((f) => ({
      lat: f.geometry.coordinates[1],
      lon: f.geometry.coordinates[0],
      display_name: [f.properties?.name, f.properties?.city, f.properties?.state]
        .filter(Boolean)
        .join(', '),
      type: f.properties?.osm_value || null,
      source: 'photon',
    }));
}

// GSI Municipality Map / SearchByAddress API (Japan-specific, free).
async function forwardGsi(q) {
  const url = `https://msearch.gsi.go.jp/address-search/AddressSearch?q=${encodeURIComponent(q)}`;
  const data = await fetchJson(url);
  if (!Array.isArray(data) || data.length === 0) return null;
  return data.slice(0, 5).map((r) => ({
    lat: r.geometry?.coordinates?.[1],
    lon: r.geometry?.coordinates?.[0],
    display_name: r.properties?.title || q,
    type: 'address',
    source: 'gsi',
  })).filter((r) => Number.isFinite(r.lat) && Number.isFinite(r.lon));
}

// ── Reverse geocoding providers ────────────────────────────────────────────
async function reverseNominatim(lat, lon) {
  const url = `https://nominatim.openstreetmap.org/reverse?format=json&addressdetails=1&zoom=18&lat=${lat}&lon=${lon}&accept-language=ja`;
  const data = await fetchJson(url);
  if (!data || data.error) return null;
  return {
    lat: parseFloat(data.lat),
    lon: parseFloat(data.lon),
    display_name: data.display_name,
    address: data.address || null,
    source: 'nominatim',
  };
}

async function reversePhoton(lat, lon) {
  const url = `https://photon.komoot.io/reverse?lat=${lat}&lon=${lon}`;
  const data = await fetchJson(url);
  const f = data?.features?.[0];
  if (!f) return null;
  return {
    lat: f.geometry.coordinates[1],
    lon: f.geometry.coordinates[0],
    display_name: [f.properties?.name, f.properties?.city, f.properties?.state]
      .filter(Boolean)
      .join(', '),
    address: f.properties || null,
    source: 'photon',
  };
}

// GSI reverse: returns the municipality (muniCd, lv01Nm) for a given lat/lon.
async function reverseGsi(lat, lon) {
  const url = `https://mreversegeocoder.gsi.go.jp/reverse-geocoder/LonLatToAddress?lat=${lat}&lon=${lon}`;
  const data = await fetchJson(url);
  const r = data?.results;
  if (!r) return null;
  return {
    lat,
    lon,
    display_name: r.lv01Nm || r.muniCd || null,
    address: { muniCd: r.muniCd, lv01Nm: r.lv01Nm },
    source: 'gsi',
  };
}

// ── Routes ─────────────────────────────────────────────────────────────────

// GET /api/geocode?q=...  — forward geocoding with fallback chain
router.get('/', async (req, res) => {
  const q = (req.query.q || '').toString().trim();
  if (!q) return res.status(400).json({ error: 'missing q parameter' });

  const cacheKey = `fwd:${q}`;
  const cached = cacheGet(cacheKey);
  if (cached) return res.json({ results: cached, fromCache: true });

  const providers = [forwardNominatim, forwardPhoton, forwardGsi];
  for (const provider of providers) {
    try {
      const hits = await provider(q);
      if (hits && hits.length > 0) {
        cacheSet(cacheKey, hits);
        return res.json({ results: hits, provider: hits[0].source });
      }
    } catch (err) {
      console.warn(`[geocode] ${provider.name} failed:`, err.message);
    }
  }
  res.json({ results: [], provider: null });
});

// GET /api/geocode/reverse?lat=...&lon=...  — reverse geocoding with fallback
router.get('/reverse', async (req, res) => {
  const lat = parseFloat(req.query.lat);
  const lon = parseFloat(req.query.lon);
  if (!Number.isFinite(lat) || !Number.isFinite(lon)) {
    return res.status(400).json({ error: 'lat and lon required' });
  }

  // Round to 5 decimals (~1.1 m) for cache-key stability
  const cacheKey = `rev:${lat.toFixed(5)},${lon.toFixed(5)}`;
  const cached = cacheGet(cacheKey);
  if (cached) return res.json({ ...cached, fromCache: true });

  const providers = [reverseNominatim, reversePhoton, reverseGsi];
  for (const provider of providers) {
    try {
      const hit = await provider(lat, lon);
      if (hit && hit.display_name) {
        cacheSet(cacheKey, hit);
        return res.json(hit);
      }
    } catch (err) {
      console.warn(`[geocode] ${provider.name} failed:`, err.message);
    }
  }
  res.json({ lat, lon, display_name: null, source: null });
});

export default router;
