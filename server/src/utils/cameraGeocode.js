/**
 * Geocode camera feed titles to real lat/lon.
 *
 * Strategy:
 *   1. GSI (Japan government address-search) — fast, unlimited, Japan-only.
 *      Best for Japanese titles ("新宿区歌舞伎町") and exact place-name hits.
 *   2. Nominatim (OSM) — used only when GSI misses. Hard 1 req/sec rate-limit
 *      per OSM policy; requests are queued through a shared serializer.
 *
 * All queries are memoized in a file-backed JSON cache so subsequent runs
 * only pay for cameras we haven't seen before. Both hits and misses are
 * cached (a miss = `{ lat: null, lon: null }`) so we don't re-hammer the API
 * for titles that are genuinely unlocatable.
 */

import fs from 'node:fs';
import path from 'node:path';
import { fetchOverpass } from '../collectors/_liveHelpers.js';

const CACHE_FILE = path.resolve(process.cwd(), 'data', 'camera-geocode-cache.json');
const USER_AGENT = 'JapanOSINT/1.0 (github.com/rayanchl/JapanOSINT)';
const NOMINATIM_MIN_INTERVAL_MS = 1100;  // keep a tiny margin over 1 req/sec

// Japan bounding box — reject hits that land outside it (prevents Nominatim
// from returning e.g. Tokyo, Ontario when a title is ambiguous).
const JP_BBOX = { minLat: 20, maxLat: 50, minLon: 120, maxLon: 155 };

function inJapan(lat, lon) {
  return (
    Number.isFinite(lat) && Number.isFinite(lon) &&
    lat >= JP_BBOX.minLat && lat <= JP_BBOX.maxLat &&
    lon >= JP_BBOX.minLon && lon <= JP_BBOX.maxLon
  );
}

// ─── Persistent cache ───────────────────────────────────────────────────────
let _cache = null;
let _cacheDirty = false;

function loadCache() {
  if (_cache) return _cache;
  try {
    const raw = fs.readFileSync(CACHE_FILE, 'utf8');
    _cache = JSON.parse(raw);
  } catch {
    _cache = {};
  }
  return _cache;
}

function saveCache() {
  if (!_cacheDirty || !_cache) return;
  try {
    fs.mkdirSync(path.dirname(CACHE_FILE), { recursive: true });
    fs.writeFileSync(CACHE_FILE, JSON.stringify(_cache), 'utf8');
    _cacheDirty = false;
  } catch (err) {
    console.warn('[cameraGeocode] cache write failed:', err.message);
  }
}

// ─── Nominatim serializer ───────────────────────────────────────────────────
let _nominatimLastCall = 0;
let _nominatimChain = Promise.resolve();

function runNominatim(query) {
  // Chain through _nominatimChain so requests run strictly sequentially.
  _nominatimChain = _nominatimChain.then(async () => {
    const wait = Math.max(0, _nominatimLastCall + NOMINATIM_MIN_INTERVAL_MS - Date.now());
    if (wait > 0) await new Promise((r) => setTimeout(r, wait));
    _nominatimLastCall = Date.now();
    return nominatimFetch(query);
  });
  return _nominatimChain;
}

// Return values:
//   { lat, lon, source } — real hit
//   null                 — verified miss (empty result); safe to cache
//   'throttled'          — 429/5xx/invalid JSON; skip cache, let caller retry later
async function nominatimFetch(q) {
  const url = `https://nominatim.openstreetmap.org/search?format=json&limit=1&countrycodes=jp&q=${encodeURIComponent(q)}`;
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), 8000);
    const res = await fetch(url, {
      headers: { 'User-Agent': USER_AGENT, 'Accept': 'application/json' },
      signal: ctrl.signal,
    });
    clearTimeout(timer);
    if (res.status === 429 || res.status >= 500) return 'throttled';
    if (!res.ok) return null;
    const ct = res.headers.get('content-type') || '';
    const text = await res.text();
    if (!ct.includes('application/json') && text.trimStart().startsWith('<')) {
      // HTML/XML error page instead of JSON — also a throttle signal.
      return 'throttled';
    }
    let data;
    try { data = JSON.parse(text); } catch { return 'throttled'; }
    if (!Array.isArray(data) || data.length === 0) return null;
    const lat = parseFloat(data[0].lat);
    const lon = parseFloat(data[0].lon);
    if (!inJapan(lat, lon)) return null;
    return { lat, lon, source: 'nominatim' };
  } catch {
    return 'throttled';
  }
}

// ─── OSM Overpass (primary) ─────────────────────────────────────────────────
// Query by name across the categories most relevant to camera titles:
// named places/neighborhoods, tourist landmarks, stations, shops, buildings.
// We escape regex specials in the title and use the case-insensitive match.
function escapeOverpassRegex(s) {
  return String(s).replace(/[.*+?^${}()|[\]\\"]/g, '\\$&');
}

async function overpassFetch(title) {
  if (!title || title.length < 2) return null;
  const esc = escapeOverpassRegex(title);
  // Search `name`, `name:en`, `name:ja` across a handful of tag categories.
  // `nwr` = node/way/relation; `out center 1` returns just the first hit.
  const body = [
    `nwr["name"~"^${esc}$",i](area.jp);`,
    `nwr["name:en"~"^${esc}$",i](area.jp);`,
    `nwr["name:ja"~"^${esc}$",i](area.jp);`,
  ].join('');
  try {
    const els = await fetchOverpass(body, (el, i, coords) => ({
      lat: coords[1], lon: coords[0], name: el.tags?.name,
    }), 20000, { limit: 1, queryTimeout: 25, cacheTtlMs: 7 * 24 * 60 * 60 * 1000 });
    if (!els || !els.length) return null;
    const { lat, lon } = els[0];
    if (!inJapan(lat, lon)) return null;
    return { lat, lon, source: 'overpass' };
  } catch {
    return 'throttled';
  }
}

// ─── GSI (Japan government) ─────────────────────────────────────────────────
async function gsiFetch(q) {
  const url = `https://msearch.gsi.go.jp/address-search/AddressSearch?q=${encodeURIComponent(q)}`;
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), 5000);
    const res = await fetch(url, {
      headers: { 'User-Agent': USER_AGENT, 'Accept': 'application/json' },
      signal: ctrl.signal,
    });
    clearTimeout(timer);
    if (res.status === 429 || res.status >= 500) return 'throttled';
    if (!res.ok) return null;
    const text = await res.text();
    let data;
    try { data = JSON.parse(text); } catch { return 'throttled'; }
    if (!Array.isArray(data) || data.length === 0) return null;
    const c = data[0].geometry?.coordinates;
    if (!Array.isArray(c) || c.length < 2) return null;
    const [lon, lat] = c;
    if (!inJapan(lat, lon)) return null;
    return { lat, lon, source: 'gsi' };
  } catch {
    return 'throttled';
  }
}

// ─── Query builders ─────────────────────────────────────────────────────────
// Turn a camera feature's free-text name + known city into a compact query.
// We strip boilerplate + descriptions, keep only the place-name phrase, and
// append ", Japan" so Nominatim restricts to the right country.
function cleanName(name) {
  if (!name) return '';
  let s = name
    // Drop everything after an English description marker. Aggregators now
    // respond in Japanese thanks to our Accept-Language header, so French
    // phrase-stripping is no longer needed.
    .split(/\b(?:View\s+from|Overlooking|Live\s+view|Watch\s+live|Webcam\s+online)\b/i)[0]
    .split('.')[0]
    .split('|')[0]
    // Drop LIVE/webcam/livecam/cam label noise.
    .replace(/[\[【]\s*LIVE\s*[\]】]/gi, '')
    .replace(/\blive(?:cam)?\b/gi, '')
    .replace(/\bwebcam\b/gi, '')
    .replace(/\bcam\b/gi, '')
    // Drop country suffix (we re-append "Japan" later when querying).
    .replace(/,?\s*(?:Japan|日本)\b/gi, '')
    // Collapse whitespace / trailing punctuation.
    .replace(/[-–—:,;]+\s*$/g, '')
    .replace(/\s{2,}/g, ' ')
    .trim();
  // Skyline names often look like "Tokyo - Shinjuku Kabukicho" — after the
  // stripping above the last dash-separated segment is usually the most
  // specific place-name (neighborhood). Prefer it.
  const dashParts = s.split(/\s*-\s*/).map((p) => p.trim()).filter(Boolean);
  if (dashParts.length > 1) {
    // Use the longest segment — most specific is usually the longest.
    dashParts.sort((a, b) => b.length - a.length);
    s = dashParts[0];
  }
  return s;
}

function cleanCity(city) {
  return (city || '')
    .replace(/-/g, ' ')
    .replace(/\s*prefecture\s*/i, '')
    .trim();
}

function buildQuery({ name, city }) {
  const n = cleanName(name);
  const c = cleanCity(city);
  if (!n && !c) return '';
  // "Place, City, Japan" — omit city if it duplicates the name.
  const parts = [n];
  if (c && !n.toLowerCase().includes(c.toLowerCase())) parts.push(c);
  parts.push('Japan');
  return parts.filter(Boolean).join(', ');
}

// ─── Public API ─────────────────────────────────────────────────────────────

/**
 * Look up coordinates for a single camera title. Returns `{ lat, lon }` on
 * a real hit, or `null` on a verified miss. Caches both outcomes.
 */
export async function geocodeCameraTitle({ name, city }) {
  const cache = loadCache();
  const q = buildQuery({ name, city });
  if (!q || q === 'Japan') return null;
  if (cache[q] !== undefined) {
    const hit = cache[q];
    return hit && hit.lat != null ? hit : null;
  }

  // Overpass query uses just the cleaned place-name (no trailing ", city,
  // Japan") since it searches exact name tags within the Japan area.
  const placeName = cleanName(name);
  let result = placeName ? await overpassFetch(placeName) : null;
  if (result === 'throttled' || !result) {
    result = await gsiFetch(q);
  }
  if (result === 'throttled' || !result) {
    result = await runNominatim(q);
  }

  if (result === 'throttled') {
    // Don't poison the cache with rate-limit noise — try again next run.
    return null;
  }
  // Record verified hits AND verified misses (empty result set).
  cache[q] = result || { lat: null, lon: null };
  _cacheDirty = true;
  return result && result.lat != null ? result : null;
}

/**
 * Bulk-geocode a list of features, mutating their `geometry.coordinates` and
 * `properties.coord_source` when a real hit lands. Runs with a small
 * concurrency cap for GSI; Nominatim is naturally serialized by its queue.
 *
 * @param {object[]} features  GeoJSON Feature objects from a camera channel
 * @param {object}   [opts]
 * @param {number}   [opts.concurrency=4]  parallel GSI requests
 * @param {number}   [opts.limit]          cap on features to process (debug)
 * @returns {Promise<{hit:number, miss:number}>}
 */
export async function geocodeFeatures(features, opts = {}) {
  const { concurrency = 4, limit } = opts;
  const targets = limit ? features.slice(0, limit) : features;
  let hit = 0;
  let miss = 0;

  const queue = targets.slice();
  async function worker() {
    while (queue.length) {
      const f = queue.shift();
      const props = f.properties || {};
      // Skip features that already have real coords (not jittered centroids).
      if (props.coord_source === 'geocoded') continue;
      const r = await geocodeCameraTitle({
        name: props.name,
        city: props.city,
      });
      if (r) {
        f.geometry.coordinates = [r.lon, r.lat];
        props.coord_source = 'geocoded';
        props.geocode_provider = r.source;
        hit += 1;
      } else {
        miss += 1;
      }
    }
  }
  await Promise.all(Array.from({ length: concurrency }, worker));
  saveCache();
  return { hit, miss };
}
