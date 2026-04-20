/**
 * OSM station building footprints for large interchange stations in major
 * metropolitan areas. Shinjuku, Tokyo, Umeda, Osaka, Shibuya, etc. are
 * mapped in OSM as closed `way`s with `railway=station` + `building=*`
 * (or `area=yes`). We pull those as polygons so the client can render a
 * translucent fill at zoom >= 15.
 *
 * Scope is bounded to four metro bboxes to keep the Overpass load small.
 * Expand METRO_BBOXES as coverage needs grow.
 */

const METRO_BBOXES = [
  // [south, west, north, east]
  [35.50, 139.50, 35.85, 139.95], // Tokyo
  [34.60, 135.40, 34.80, 135.60], // Osaka
  [35.30, 139.50, 35.50, 139.70], // Yokohama
  [35.00, 135.70, 35.10, 135.80], // Kyoto
];

const OVERPASS_ENDPOINTS = [
  'https://overpass-api.de/api/interpreter',
  'https://overpass.kumi.systems/api/interpreter',
  'https://overpass.openstreetmap.ru/api/interpreter',
];

// Closed way polygon → GeoJSON Polygon coordinates. If the way isn't closed
// (first != last), we skip it; Overpass `out geom` gives us the vertex
// sequence even for ways, so we can detect closure by coordinate equality.
function toPolygon(way) {
  const geom = way.geometry;
  if (!Array.isArray(geom) || geom.length < 4) return null;
  const first = geom[0];
  const last = geom[geom.length - 1];
  if (first.lat !== last.lat || first.lon !== last.lon) return null;
  const ring = geom.map((p) => [p.lon, p.lat]);
  return {
    type: 'Feature',
    geometry: { type: 'Polygon', coordinates: [ring] },
    properties: {
      station_id: `OSM_WAY_${way.id}`,
      name: way.tags?.['name:en'] || way.tags?.name || 'Station',
      name_ja: way.tags?.name || way.tags?.['name:ja'] || null,
      operator: way.tags?.operator || null,
      railway: way.tags?.railway || null,
      building: way.tags?.building || null,
      country: 'JP',
      source: 'osm_station_boundary',
    },
  };
}

async function queryOne(bbox, timeoutMs = 90_000) {
  const [s, w, n, e] = bbox;
  const body = `[out:json][timeout:180];(
    way["railway"="station"]["building"](${s},${w},${n},${e});
    way["railway"="station"]["area"="yes"](${s},${w},${n},${e});
    way["public_transport"="station"]["building"](${s},${w},${n},${e});
  );out geom;`;
  for (const endpoint of OVERPASS_ENDPOINTS) {
    try {
      const ctrl = new AbortController();
      const timer = setTimeout(() => ctrl.abort(), timeoutMs);
      const res = await fetch(endpoint, {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: `data=${encodeURIComponent(body)}`,
        signal: ctrl.signal,
      });
      clearTimeout(timer);
      if (!res.ok) continue;
      const json = await res.json();
      return Array.isArray(json.elements) ? json.elements : [];
    } catch { /* try next endpoint */ }
  }
  return [];
}

export default async function collectOsmTransportStationBoundaries() {
  const seen = new Set();
  const features = [];
  for (const bbox of METRO_BBOXES) {
    const elements = await queryOne(bbox);
    for (const el of elements) {
      if (el.type !== 'way' || !el.id) continue;
      if (seen.has(el.id)) continue;
      seen.add(el.id);
      const f = toPolygon(el);
      if (f) features.push(f);
    }
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'osm_station_boundaries',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      bboxes: METRO_BBOXES.length,
      description: 'OSM closed way[railway=station][building/area] polygons for major metros.',
    },
    metadata: {},
  };
}
