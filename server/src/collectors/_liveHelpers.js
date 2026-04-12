/**
 * Shared helpers for live data fetching across collectors.
 * Centralizes OSM Overpass + generic HTTP fetch with timeout + retries.
 */

export const OVERPASS_ENDPOINTS = [
  'https://overpass-api.de/api/interpreter',
  'https://overpass.kumi.systems/api/interpreter',
  'https://overpass.openstreetmap.ru/api/interpreter',
];

/**
 * Execute an Overpass QL query against Japan and map elements into GeoJSON features.
 * @param {string} overpassBody - the body of the query, e.g. `node["amenity"="bus_station"](area.jp);`
 * @param {(el: any, i: number) => any} mapFn - maps raw OSM element to GeoJSON feature
 * @param {number} timeoutMs
 * @returns {Promise<any[]|null>}
 */
export async function fetchOverpass(overpassBody, mapFn, timeoutMs = 15000) {
  const query = `[out:json][timeout:40];area["ISO3166-1"="JP"][admin_level=2]->.jp;(${overpassBody});out center 800;`;
  for (const endpoint of OVERPASS_ENDPOINTS) {
    try {
      const ctrl = new AbortController();
      const timer = setTimeout(() => ctrl.abort(), timeoutMs);
      const res = await fetch(endpoint, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded',
          'User-Agent': 'JapanOSINT/1.0 (github.com/rayanchl/JapanOSINT)',
        },
        body: 'data=' + encodeURIComponent(query),
        signal: ctrl.signal,
      });
      clearTimeout(timer);
      if (!res.ok) continue;
      const data = await res.json();
      const els = (data.elements || []).filter((e) => {
        if (e.lat != null && e.lon != null) return true;
        if (e.center && e.center.lat != null && e.center.lon != null) return true;
        return false;
      });
      if (els.length === 0) continue;
      return els.map((el, i) => {
        const coords = el.lat != null
          ? [el.lon, el.lat]
          : [el.center.lon, el.center.lat];
        return mapFn(el, i, coords);
      });
    } catch { /* try next endpoint */ }
  }
  return null;
}

/**
 * Fetch JSON from any public endpoint with a timeout.
 */
export async function fetchJson(url, { timeoutMs = 10000, headers = {} } = {}) {
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), timeoutMs);
    const res = await fetch(url, {
      headers: { 'User-Agent': 'JapanOSINT/1.0', Accept: 'application/json', ...headers },
      signal: ctrl.signal,
    });
    clearTimeout(timer);
    if (!res.ok) return null;
    return await res.json();
  } catch { return null; }
}

/**
 * Fetch raw text (HTML, CSV, XML) from any public endpoint with a timeout.
 */
export async function fetchText(url, { timeoutMs = 10000, headers = {} } = {}) {
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), timeoutMs);
    const res = await fetch(url, {
      headers: { 'User-Agent': 'Mozilla/5.0 (compatible; JapanOSINT/1.0)', ...headers },
      signal: ctrl.signal,
    });
    clearTimeout(timer);
    if (!res.ok) return null;
    return await res.text();
  } catch { return null; }
}
