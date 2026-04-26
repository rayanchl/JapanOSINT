/**
 * Reusable GSI address-search helper.
 * https://msearch.gsi.go.jp/address-search/AddressSearch?q=...
 *
 * Returns the top hit as `{ lat, lon, title }`, or `null` on miss / failure.
 * Never throws.
 */

const DEFAULT_BASE = 'https://msearch.gsi.go.jp';
const DEFAULT_TIMEOUT_MS = 8000;

/**
 * Resolve a Japanese place-name to coordinates via GSI's address-search.
 * Returns `{ lat, lon, title }` for the top hit, or `null` on miss / failure.
 * Never throws.
 *
 * @param {string} query      Place-name or address to look up.
 * @param {object} [opts]
 * @param {string} [opts.baseUrl]     Override base URL (useful in tests).
 * @param {number} [opts.timeoutMs]   Request timeout in ms (default 8000).
 * @returns {Promise<{lat:number, lon:number, title:string|null}|null>}
 */
export async function gsiAddressSearch(query, opts = {}) {
  if (!query || typeof query !== 'string') return null;
  const baseUrl = opts.baseUrl || DEFAULT_BASE;
  const timeoutMs = opts.timeoutMs ?? DEFAULT_TIMEOUT_MS;
  const url = `${baseUrl}/address-search/AddressSearch?q=${encodeURIComponent(query)}`;

  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const res = await fetch(url, { signal: controller.signal });
    clearTimeout(timer);
    if (!res.ok) return null;
    const text = await res.text();
    let data;
    try { data = JSON.parse(text); } catch { return null; }
    if (!Array.isArray(data) || data.length === 0) return null;
    const first = data[0];
    const coords = first?.geometry?.coordinates;
    if (!Array.isArray(coords) || coords.length < 2) return null;
    const lon = Number(coords[0]);
    const lat = Number(coords[1]);
    if (!Number.isFinite(lat) || !Number.isFinite(lon)) return null;
    return { lat, lon, title: first?.properties?.title ?? null };
  } catch {
    clearTimeout(timer);
    return null;
  }
}
