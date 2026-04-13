/**
 * Shared helpers for live data fetching across collectors.
 *
 * Provides:
 *  - `fetchOverpass`       : single query to Japan area (ISO3166-1=JP)
 *  - `fetchOverpassTiled`  : same query split across 12 sub-regions (tiles)
 *                            so that nationwide result sets exceeding a single
 *                            Overpass instance's timeout / element limit are
 *                            still fully returned. Deduplicates by OSM id.
 *  - `fetchJson` / `fetchText` : generic JSON / text helpers with retries.
 *  - A global per-host queue that caps concurrent requests and enforces a
 *    small inter-request delay so we never hammer the same endpoint.
 *
 * All Overpass results are cached per-query-string in memory for 6h (configurable)
 * so the scheduler can run collectors frequently without re-hitting upstream.
 */

export const OVERPASS_ENDPOINTS = [
  'https://overpass-api.de/api/interpreter',
  'https://overpass.kumi.systems/api/interpreter',
  'https://overpass.openstreetmap.ru/api/interpreter',
  'https://overpass.private.coffee/api/interpreter',
];

const overpassCache = new Map(); // key -> { features, expiresAt }
const DEFAULT_CACHE_TTL_MS = 6 * 60 * 60 * 1000;

// -----------------------------------------------------------------------------
// Per-host request queue: rate-limits concurrent fetches to the same upstream.
// Overpass and most government APIs don't publish strict quotas but will return
// 429 / 503 under burst load. We throttle conservatively to stay polite.
// -----------------------------------------------------------------------------
const HOST_CONCURRENCY = 2;        // max concurrent requests per host
const HOST_MIN_GAP_MS  = 500;      // minimum gap between dispatches per host
const hostQueues = new Map();      // host -> { active, lastDispatch, queue: [] }

function getHost(url) {
  try { return new URL(url).host; } catch { return 'unknown'; }
}

function hostState(host) {
  let st = hostQueues.get(host);
  if (!st) {
    st = { active: 0, lastDispatch: 0, queue: [] };
    hostQueues.set(host, st);
  }
  return st;
}

function scheduleNext(host) {
  const st = hostState(host);
  while (st.active < HOST_CONCURRENCY && st.queue.length > 0) {
    const gap = Date.now() - st.lastDispatch;
    const wait = Math.max(0, HOST_MIN_GAP_MS - gap);
    const job = st.queue.shift();
    st.active++;
    st.lastDispatch = Date.now() + wait;
    setTimeout(() => {
      job.run().finally(() => {
        st.active--;
        scheduleNext(host);
      });
    }, wait);
  }
}

/**
 * Queue an async function so at most HOST_CONCURRENCY run per host at once,
 * with a HOST_MIN_GAP_MS gap between dispatches. Safe against 429s under load.
 */
function rateLimitedFetch(url, runner) {
  const host = getHost(url);
  return new Promise((resolve, reject) => {
    const st = hostState(host);
    st.queue.push({
      run: () => Promise.resolve().then(runner).then(resolve, reject),
    });
    scheduleNext(host);
  });
}

// -----------------------------------------------------------------------------
// Overpass cache
// -----------------------------------------------------------------------------
function cacheGet(key) {
  const hit = overpassCache.get(key);
  if (!hit) return null;
  if (hit.expiresAt < Date.now()) {
    overpassCache.delete(key);
    return null;
  }
  return hit.features;
}
function cacheSet(key, features, ttlMs) {
  overpassCache.set(key, { features, expiresAt: Date.now() + ttlMs });
}

function mapElements(elements, mapFn) {
  const out = [];
  for (let i = 0; i < elements.length; i++) {
    const el = elements[i];
    const coords = el.lat != null
      ? [el.lon, el.lat]
      : [el.center?.lon, el.center?.lat];
    if (coords[0] == null || coords[1] == null) continue;
    out.push(mapFn(el, i, coords));
  }
  return out;
}

/**
 * Execute an Overpass QL query against the whole of Japan.
 *
 * @param {string} overpassBody - Overpass body referring to area `.jp`,
 *        e.g. `node["amenity"="bus_station"](area.jp);`
 * @param {(el, i, coords) => any} mapFn - maps raw OSM element to GeoJSON feature
 * @param {number} timeoutMs - per-endpoint HTTP timeout (ms)
 * @param {object} [options]
 * @param {number|null} [options.limit=0]       - `out` element cap. 0/null = unlimited.
 * @param {number} [options.queryTimeout=180]   - Overpass [timeout:] seconds
 * @param {number} [options.cacheTtlMs]
 * @param {boolean} [options.useCache=true]
 * @returns {Promise<any[]|null>}
 */
export async function fetchOverpass(overpassBody, mapFn, timeoutMs = 60_000, options = {}) {
  const {
    limit = 0,
    queryTimeout = 180,
    cacheTtlMs = DEFAULT_CACHE_TTL_MS,
    useCache = true,
  } = options;
  const outClause = limit && limit > 0 ? `out center ${limit};` : 'out center;';
  const query = `[out:json][timeout:${queryTimeout}];area["ISO3166-1"="JP"][admin_level=2]->.jp;(${overpassBody});${outClause}`;

  if (useCache) {
    const cached = cacheGet(query);
    if (cached) return mapElements(cached, mapFn);
  }

  for (const endpoint of OVERPASS_ENDPOINTS) {
    const els = await rateLimitedFetch(endpoint, () => overpassRequest(endpoint, query, timeoutMs));
    if (els && els.length) {
      if (useCache) cacheSet(query, els, cacheTtlMs);
      return mapElements(els, mapFn);
    }
  }
  return null;
}

async function overpassRequest(endpoint, query, timeoutMs) {
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), timeoutMs);
    const res = await fetch(endpoint, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/x-www-form-urlencoded',
        'User-Agent': 'JapanOSINT/1.0 (+https://github.com/rayanchl/JapanOSINT)',
      },
      body: 'data=' + encodeURIComponent(query),
      signal: ctrl.signal,
    });
    clearTimeout(timer);
    if (!res.ok) return null;
    const data = await res.json();
    return (data.elements || []).filter((e) => {
      if (e.lat != null && e.lon != null) return true;
      if (e.center && e.center.lat != null && e.center.lon != null) return true;
      return false;
    });
  } catch { return null; }
}

/**
 * Japan divided into 12 large bbox tiles. Covers the four main islands,
 * Hokkaido, Okinawa, Amami/Ogasawara. Overlapping edges get deduped by OSM id.
 * Format: [south, west, north, east] — Overpass bbox order.
 */
const JAPAN_TILES = [
  [41.35, 139.33, 45.55, 145.85], // N Hokkaido
  [41.35, 139.33, 45.55, 148.00], // NE Hokkaido + Kurils fringe
  [37.73, 139.33, 41.60, 142.05], // N Tohoku
  [34.55, 138.50, 37.80, 141.30], // Kanto + S Tohoku
  [33.80, 135.20, 37.80, 138.60], // Chubu + Kansai + Hokuriku (Niigata/Toyama/Ishikawa/Fukui)
  [33.10, 131.90, 35.50, 135.30], // Chugoku + Kansai
  [31.50, 129.80, 34.40, 132.40], // Kyushu W
  [31.20, 130.20, 33.80, 132.30], // Kyushu E + Shikoku S
  [32.50, 132.00, 34.60, 134.80], // Shikoku
  [24.00, 122.80, 28.50, 131.60], // Okinawa + Ryukyu
  [26.00, 140.80, 35.00, 142.30], // Ogasawara + Izu
  [33.00, 128.00, 36.00, 130.50], // Tsushima + W Kyushu offshore
];

/**
 * Run an Overpass body across Japan in 12 tile bboxes, deduping by OSM id.
 * Use this when a nationwide dataset is too large for a single Overpass call
 * (e.g. all convenience stores, all power towers, all shrine/temple nodes).
 *
 * @param {(bbox:string) => string} bodyFn - returns Overpass body given a bbox
 *        string "s,w,n,e". Do NOT reference `area.jp` — use (bbox) instead.
 * @param {(el, i, coords) => any} mapFn
 * @param {object} [options]
 * @param {number} [options.queryTimeout=180]
 * @param {number} [options.timeoutMs=60000]
 * @param {number} [options.concurrency=2]
 * @param {boolean} [options.useCache=true]
 * @returns {Promise<any[]|null>}
 */
export async function fetchOverpassTiled(bodyFn, mapFn, options = {}) {
  const {
    queryTimeout = 180,
    timeoutMs = 60_000,
    useCache = true,
    cacheTtlMs = DEFAULT_CACHE_TTL_MS,
  } = options;

  const queries = JAPAN_TILES.map((t) => {
    const bbox = `${t[0]},${t[1]},${t[2]},${t[3]}`;
    return `[out:json][timeout:${queryTimeout}];(${bodyFn(bbox)});out center;`;
  });

  // Cache whole tile set under one key (each tile deterministic)
  const cacheKey = queries.join('|');
  if (useCache) {
    const cached = cacheGet(cacheKey);
    if (cached) return mapElements(cached, mapFn);
  }

  const allElements = [];
  const seen = new Set();

  // Serialize by tile to avoid hammering a single endpoint. Different tiles
  // may be dispatched to different endpoints round-robin.
  for (let i = 0; i < queries.length; i++) {
    const q = queries[i];
    let got = null;
    for (let ei = 0; ei < OVERPASS_ENDPOINTS.length; ei++) {
      const endpoint = OVERPASS_ENDPOINTS[(ei + i) % OVERPASS_ENDPOINTS.length];
      got = await rateLimitedFetch(endpoint, () => overpassRequest(endpoint, q, timeoutMs));
      if (got && got.length) break;
    }
    if (!got) continue;
    for (const el of got) {
      const key = el.type && el.id != null ? `${el.type}/${el.id}` : null;
      if (key && seen.has(key)) continue;
      if (key) seen.add(key);
      allElements.push(el);
    }
  }

  if (allElements.length === 0) return null;
  if (useCache) cacheSet(cacheKey, allElements, cacheTtlMs);
  return mapElements(allElements, mapFn);
}

/**
 * Fetch JSON from any public endpoint with a timeout + optional retries.
 * Rate-limited per host.
 */
export async function fetchJson(url, { timeoutMs = 15000, headers = {}, retries = 1 } = {}) {
  return rateLimitedFetch(url, async () => {
    let lastErr = null;
    for (let attempt = 0; attempt <= retries; attempt++) {
      try {
        const ctrl = new AbortController();
        const timer = setTimeout(() => ctrl.abort(), timeoutMs);
        const res = await fetch(url, {
          headers: {
            'User-Agent': 'JapanOSINT/1.0 (+https://github.com/rayanchl/JapanOSINT)',
            Accept: 'application/json',
            ...headers,
          },
          signal: ctrl.signal,
        });
        clearTimeout(timer);
        if (res.status === 429 || res.status === 503) {
          // Back off exponentially on rate-limit signals
          await new Promise((r) => setTimeout(r, 1000 * Math.pow(2, attempt)));
          continue;
        }
        if (!res.ok) return null;
        return await res.json();
      } catch (e) { lastErr = e; }
    }
    return null;
  });
}

/**
 * Fetch raw text (HTML, CSV, XML). Rate-limited per host.
 */
export async function fetchText(url, { timeoutMs = 15000, headers = {}, retries = 1 } = {}) {
  return rateLimitedFetch(url, async () => {
    for (let attempt = 0; attempt <= retries; attempt++) {
      try {
        const ctrl = new AbortController();
        const timer = setTimeout(() => ctrl.abort(), timeoutMs);
        const res = await fetch(url, {
          headers: {
            'User-Agent': 'Mozilla/5.0 (compatible; JapanOSINT/1.0)',
            ...headers,
          },
          signal: ctrl.signal,
        });
        clearTimeout(timer);
        if (res.status === 429 || res.status === 503) {
          await new Promise((r) => setTimeout(r, 1000 * Math.pow(2, attempt)));
          continue;
        }
        if (!res.ok) return null;
        return await res.text();
      } catch { /* retry */ }
    }
    return null;
  });
}
