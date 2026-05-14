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
const GEOCODE_CACHE = new Map(); // key -> { value, expiresAt }
const CACHE_TTL_MS = 24 * 60 * 60 * 1000; // 24h

function cacheGet(key) {
  const hit = GEOCODE_CACHE.get(key);
  if (!hit) return null;
  if (hit.expiresAt < Date.now()) {
    GEOCODE_CACHE.delete(key);
    return null;
  }
  return hit.value;
}
function cacheSet(key, value) {
  GEOCODE_CACHE.set(key, { value, expiresAt: Date.now() + CACHE_TTL_MS });
}

async function fetchJson(url, { timeoutMs = 6000, headers = {} } = {}) {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeoutMs);
  try {
    const res = await fetch(url, {
      headers: { 'User-Agent': USER_AGENT, Accept: 'application/json', ...headers },
      signal: ctrl.signal,
    });
    if (!res.ok) return null;
    return await res.json();
  } catch {
    return null;
  } finally {
    clearTimeout(timer);
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
  // Run JA + EN in parallel — Nominatim's `accept-language` controls the
  // localised label and is the only difference between the two requests, so
  // the cost of doing both is one extra round-trip.
  const base = `https://nominatim.openstreetmap.org/reverse?format=json&addressdetails=1&zoom=18&lat=${lat}&lon=${lon}`;
  const [ja, en] = await Promise.all([
    fetchJson(`${base}&accept-language=ja`),
    fetchJson(`${base}&accept-language=en`),
  ]);
  // Pick whichever one we got back as the canonical hit; if both failed, bail.
  const primary = ja || en;
  if (!primary || primary.error) return null;
  return {
    lat: parseFloat(primary.lat),
    lon: parseFloat(primary.lon),
    display_name: ja?.display_name || en?.display_name || null,
    display_name_ja: ja?.display_name || null,
    display_name_en: en?.display_name || null,
    address: primary.address || null,
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

// Forward-geocode one query through the provider fallback chain. Returns
// { hits, provider } or { hits: [], provider: null } if every provider missed.
// Cached per-query (24h TTL) so repeat queries skip the network entirely.
async function forwardGeocodeOne(q) {
  const cacheKey = `fwd:${q}`;
  const cached = cacheGet(cacheKey);
  if (cached) return { hits: cached, provider: cached[0]?.source ?? null, fromCache: true };

  const providers = [forwardNominatim, forwardPhoton, forwardGsi];
  for (const provider of providers) {
    try {
      const hits = await provider(q);
      if (hits && hits.length > 0) {
        cacheSet(cacheKey, hits);
        return { hits, provider: hits[0].source, fromCache: false };
      }
    } catch (err) {
      console.warn(`[geocode] ${provider.name} failed:`, err.message);
    }
  }
  return { hits: [], provider: null, fromCache: false };
}

// GET /api/geocode?q=...&qAlt=...  — forward geocoding with fallback chain.
// When qAlt is provided alongside q, run both queries in parallel and merge
// hits, tagging qAlt-only hits with via_translation:true. Used by the iOS
// bilingual search UX so the user sees both English and Japanese matches.
router.get('/', async (req, res) => {
  const q    = (req.query.q    || '').toString().trim();
  const qAlt = (req.query.qAlt || '').toString().trim();
  if (!q) return res.status(400).json({ error: 'missing q parameter' });

  // Single-query path — unchanged response shape.
  if (!qAlt || qAlt === q) {
    const { hits, provider, fromCache } = await forwardGeocodeOne(q);
    return res.json({ results: hits, provider, ...(fromCache ? { fromCache: true } : {}) });
  }

  const [primary, secondary] = await Promise.all([
    forwardGeocodeOne(q),
    forwardGeocodeOne(qAlt),
  ]);
  // Merge by (lat, lon) rounded to 5 decimals (~1.1 m). Same point from both
  // queries is one row — keep the primary-query attribution.
  const key = (h) => `${h.lat.toFixed(5)},${h.lon.toFixed(5)}`;
  const seen = new Set();
  const merged = [];
  for (const h of primary.hits) {
    const k = key(h);
    if (seen.has(k)) continue;
    seen.add(k);
    merged.push({ ...h, via_translation: false });
  }
  for (const h of secondary.hits) {
    const k = key(h);
    if (seen.has(k)) continue;
    seen.add(k);
    merged.push({ ...h, via_translation: true, matched_alt: qAlt });
  }
  res.json({
    results: merged,
    provider: primary.provider || secondary.provider || null,
  });
});

// GET /api/geocode/reverse?lat=...&lon=...  — reverse geocoding with fallback
router.get('/reverse', async (req, res) => {
  const lat = parseFloat(req.query.lat);
  const lon = parseFloat(req.query.lon);
  if (!Number.isFinite(lat) || !Number.isFinite(lon)) {
    return res.status(400).json({ error: 'lat and lon required' });
  }

  // Round to 5 decimals (~1.1 m) for cache-key stability. The `rev2:` prefix
  // invalidates older entries that pre-date the JA+EN parallel fetch — those
  // only have `display_name`, so reusing them would surface the JA string in
  // the EN slot.
  const cacheKey = `rev2:${lat.toFixed(5)},${lon.toFixed(5)}`;
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
