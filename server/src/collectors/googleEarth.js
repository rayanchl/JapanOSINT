/**
 * Google Earth Layer Collector
 * Aggregates public Google-Earth-style KML overlays with points-of-interest
 * over Japan. The "Google Earth community" layers historically included
 * volcanoes, heritage sites and populated places; we reproduce that by
 * fetching real public KML feeds that are usable inside Google Earth today:
 *   - Smithsonian GVP Holocene volcanoes KML
 *   - Wikipedia GeoSearch (populated places / landmarks, GeoRSS-like JSON)
 * Features are points with lat/lon; the KML is parsed with a minimal regex
 * (coordinates + name) to avoid a heavyweight XML dependency.
 *
 * No env vars - all endpoints are public.
 */

import { fetchText, fetchJson } from './_liveHelpers.js';

const GVP_KML = 'https://volcano.si.edu/ReportKMLs/Holocene.kml';
const WIKI_GEO = 'https://en.wikipedia.org/w/api.php?format=json&action=query&list=geosearch&gsradius=10000&gscoord=36%7C138&gslimit=500';

function inJapanBbox(lon, lat) {
  return lon >= 122 && lon <= 154 && lat >= 24 && lat <= 46;
}

// Parse KML: extract <Placemark> blocks, pick <name> and first <coordinates> point.
function parseKmlPlacemarks(kml) {
  if (!kml) return [];
  const out = [];
  const placemarkRe = /<Placemark\b[^>]*>([\s\S]*?)<\/Placemark>/g;
  let m;
  while ((m = placemarkRe.exec(kml)) !== null) {
    const block = m[1];
    const nameMatch = block.match(/<name>([\s\S]*?)<\/name>/);
    const coordMatch = block.match(/<coordinates>\s*([-0-9.,\s]+?)\s*<\/coordinates>/);
    if (!coordMatch) continue;
    const firstTuple = coordMatch[1].split(/\s+/)[0] || coordMatch[1];
    const [lonStr, latStr] = firstTuple.split(',');
    const lon = parseFloat(lonStr);
    const lat = parseFloat(latStr);
    if (Number.isNaN(lon) || Number.isNaN(lat)) continue;
    out.push({
      name: nameMatch ? nameMatch[1].replace(/<\!\[CDATA\[|\]\]>/g, '').trim() : null,
      lon,
      lat,
    });
  }
  return out;
}

async function tryGvpVolcanoes() {
  const kml = await fetchText(GVP_KML, { timeoutMs: 12000 });
  if (!kml) return null;
  const pts = parseKmlPlacemarks(kml).filter((p) => inJapanBbox(p.lon, p.lat));
  if (!pts.length) return null;
  return pts.map((p, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [p.lon, p.lat] },
    properties: {
      id: `GE_VOLC_${i}`,
      name: p.name || `Volcano ${i + 1}`,
      layer: 'volcano',
      country: 'JP',
      source: 'smithsonian_gvp_kml',
    },
  }));
}

async function tryWikiGeo() {
  const data = await fetchJson(WIKI_GEO, { timeoutMs: 10000 });
  const items = data?.query?.geosearch || [];
  if (!items.length) return null;
  return items
    .filter((it) => inJapanBbox(it.lon, it.lat))
    .map((it, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [it.lon, it.lat] },
      properties: {
        id: `GE_WIKI_${it.pageid || i}`,
        name: it.title,
        layer: 'wikipedia',
        distance_m: it.dist,
        wiki_url: `https://en.wikipedia.org/?curid=${it.pageid}`,
        country: 'JP',
        source: 'wikipedia_geosearch',
      },
    }));
}

// Seed: Mount Fuji, Sakurajima, Ontake etc.
const SEED_POINTS = [
  { name: 'Mount Fuji', lat: 35.3606, lon: 138.7274, layer: 'volcano' },
  { name: 'Sakurajima', lat: 31.5833, lon: 130.6571, layer: 'volcano' },
  { name: 'Mount Ontake', lat: 35.8936, lon: 137.4803, layer: 'volcano' },
  { name: 'Mount Aso', lat: 32.8842, lon: 131.1050, layer: 'volcano' },
  { name: 'Mount Unzen', lat: 32.7608, lon: 130.2994, layer: 'volcano' },
  { name: 'Mount Asama', lat: 36.4061, lon: 138.5228, layer: 'volcano' },
  { name: 'Showa Shinzan', lat: 42.5397, lon: 140.8772, layer: 'volcano' },
  { name: 'Mount Tokachi', lat: 43.4175, lon: 142.6869, layer: 'volcano' },
  { name: 'Sakura-jima Minamidake', lat: 31.5856, lon: 130.6567, layer: 'volcano' },
  { name: 'Suwanosejima', lat: 29.6383, lon: 129.7139, layer: 'volcano' },
  { name: 'Tokyo', lat: 35.6762, lon: 139.6503, layer: 'wikipedia' },
  { name: 'Kyoto', lat: 35.0116, lon: 135.7681, layer: 'wikipedia' },
  { name: 'Osaka', lat: 34.6937, lon: 135.5023, layer: 'wikipedia' },
  { name: 'Sapporo', lat: 43.0618, lon: 141.3545, layer: 'wikipedia' },
  { name: 'Naha', lat: 26.2124, lon: 127.6809, layer: 'wikipedia' },
  { name: 'Sendai', lat: 38.2682, lon: 140.8694, layer: 'wikipedia' },
  { name: 'Nagoya', lat: 35.1815, lon: 136.9066, layer: 'wikipedia' },
  { name: 'Hiroshima', lat: 34.3853, lon: 132.4553, layer: 'wikipedia' },
  { name: 'Fukuoka', lat: 33.5904, lon: 130.4017, layer: 'wikipedia' },
  { name: 'Nagasaki', lat: 32.7503, lon: 129.8777, layer: 'wikipedia' },
];

function generateSeed() {
  return SEED_POINTS.map((p, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [p.lon, p.lat] },
    properties: {
      id: `GE_SEED_${String(i + 1).padStart(4, '0')}`,
      name: p.name,
      layer: p.layer,
      country: 'JP',
      source: 'google_earth_seed',
    },
  }));
}

export default async function collectGoogleEarth() {
  const [volc, wiki] = await Promise.all([tryGvpVolcanoes(), tryWikiGeo()]);
  const combined = [...(volc || []), ...(wiki || [])];
  let liveSource = null;
  if (volc && volc.length && wiki && wiki.length) liveSource = 'gvp_kml+wikipedia';
  else if (volc && volc.length) liveSource = 'smithsonian_gvp_kml';
  else if (wiki && wiki.length) liveSource = 'wikipedia_geosearch';

  const live = combined.length > 0;
  const features = live ? combined : generateSeed();
  if (!live) liveSource = 'google_earth_seed';

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'google-earth',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: liveSource,
      description: 'Google-Earth-style KML/KMZ public layers (Smithsonian volcanoes + Wikipedia GeoSearch) filtered to Japan',
    },
    metadata: {},
  };
}
