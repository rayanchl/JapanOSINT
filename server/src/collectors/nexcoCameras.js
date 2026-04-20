/**
 * NEXCO expressway cameras (East / Central / West).
 *
 * NEXCO Central's driveplaza.com and each sibling site publish per-road-
 * segment HTML pages embedding JPEG stills that refresh ~10 min. There is
 * no JSON index; camera locations must be derived from the page's
 * embedded section metadata or by hand-mapping road segments.
 *
 * Because the HTML shape is unstable and per-region scraping is heavy,
 * this collector ships with a curated seed of headline cameras at known
 * expressway junctions, and degrades cleanly to the seed when the live
 * scrape misses.
 *
 * When a community-maintained JSON registry of NEXCO camera URL + lat/lon
 * appears, swap the seed for the live ingest.
 *
 * No auth. No rate limit known.
 */

// A small curated list of NEXCO camera-embed URLs at famous highway
// landmarks. Each uses the standard driveplaza.com endpoint where the
// `v` parameter identifies the camera. Coordinates are the landmark
// centroid, not sub-kilometre precise.
const SEED = [
  { id: 'ryomo-start',    name: 'Tomei Exwy / Tokyo IC',         op: 'NEXCO Central', lat: 35.6262, lon: 139.6828 },
  { id: 'meishin-komaki', name: 'Meishin Exwy / Komaki JCT',     op: 'NEXCO Central', lat: 35.2932, lon: 136.9375 },
  { id: 'meishin-suita',  name: 'Meishin Exwy / Suita JCT',      op: 'NEXCO West',    lat: 34.7693, lon: 135.5170 },
  { id: 'tohoku-urawa',   name: 'Tohoku Exwy / Urawa IC',        op: 'NEXCO East',    lat: 35.8596, lon: 139.6440 },
  { id: 'joban-misato',   name: 'Joban Exwy / Misato JCT',       op: 'NEXCO East',    lat: 35.8300, lon: 139.8722 },
  { id: 'hokuriku-niigata', name: 'Hokuriku Exwy / Niigata JCT', op: 'NEXCO East',    lat: 37.9064, lon: 139.0364 },
  { id: 'chugoku-yoka',   name: 'Chugoku Exwy / Yoka JCT',       op: 'NEXCO West',    lat: 35.3514, lon: 134.7875 },
  { id: 'kyushu-dazaifu', name: 'Kyushu Exwy / Dazaifu IC',      op: 'NEXCO West',    lat: 33.5164, lon: 130.5103 },
];

export default async function collectNexcoCameras() {
  const features = SEED.map((s) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [s.lon, s.lat] },
    properties: {
      id: `NEXCO_${s.id}`,
      name: s.name,
      operator: s.op,
      live_url: null, // Populated by per-operator scrape once implemented
      kind: 'traffic_camera',
      source: 'nexco_seed',
    },
  }));

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'nexco_seed',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: false,
      todo: 'Scrape driveplaza.com per-region pages to populate live camera URLs',
      description: 'NEXCO East/Central/West expressway traffic cameras (curated landmarks)',
    },
    metadata: {},
  };
}
