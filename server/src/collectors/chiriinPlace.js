/**
 * GSI (国土地理院) place-name search — chiriin-place.
 *
 * Real, key-free: GSI's AddressSearch endpoint
 *   https://msearch.gsi.go.jp/address-search/AddressSearch?q=<term>
 * returns a GeoJSON-style array of matching place names with coordinates.
 *
 * There is no single "all place names" download, so we resolve a bounded set
 * of representative geographic-feature query terms (mountains, capes, lakes,
 * peninsulas, plateaus). Every returned point is real GSI data. On total
 * failure we return an honest empty FeatureCollection (no fabrication).
 */

const BASE = 'https://msearch.gsi.go.jp/address-search/AddressSearch';
const TIMEOUT_MS = 8000;

// Representative GSI 地名 search terms — real geographic features across Japan.
const QUERIES = [
  '岳', '山', '岬', '湖', '半島', '高原', '平野', '盆地', '峠', '島',
];

async function fetchTerm(term) {
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(`${BASE}?q=${encodeURIComponent(term)}`, { signal: ctrl.signal });
    clearTimeout(timer);
    if (!res.ok) return [];
    const text = await res.text();
    let data;
    try { data = JSON.parse(text); } catch { return []; }
    return Array.isArray(data) ? data : [];
  } catch {
    return [];
  }
}

function emptyResult(source) {
  return {
    type: 'FeatureCollection',
    features: [],
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: 0,
      description: 'GSI (国土地理院) place-name search results',
    },
  };
}

export default async function collectChiriinPlace() {
  let raw;
  try {
    raw = await Promise.all(QUERIES.map(fetchTerm));
  } catch {
    return emptyResult('chiriin_place_unavailable');
  }

  const seen = new Set();
  const features = [];
  for (const hits of raw) {
    for (const h of hits) {
      const coords = h?.geometry?.coordinates;
      if (!Array.isArray(coords) || coords.length < 2) continue;
      const lon = Number(coords[0]);
      const lat = Number(coords[1]);
      if (!Number.isFinite(lat) || !Number.isFinite(lon)) continue;
      const title = h?.properties?.title ?? null;
      const key = `${title}|${lon.toFixed(5)}|${lat.toFixed(5)}`;
      if (seen.has(key)) continue;
      seen.add(key);
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [lon, lat] },
        properties: {
          title,
          dataType: h?.properties?.dataType ?? null,
          source: 'gsi_chiriin',
        },
      });
    }
  }

  if (features.length === 0) return emptyResult('chiriin_place_unavailable');

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'gsi_chiriin',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'GSI (国土地理院) place-name search results',
    },
  };
}
