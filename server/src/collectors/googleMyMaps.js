/**
 * Google My Maps Collector
 * Pulls user-created Google My Maps as KML via the public export endpoint:
 *   https://www.google.com/maps/d/kml?mid=<MAP_ID>&forcekml=1
 * Multiple map IDs can be configured via the GOOGLE_MYMAPS_IDS env var
 * (comma-separated). Parses Placemark points and filters to Japan bbox.
 * Falls back to a curated seed of well-known Tokyo/Kyoto/Osaka POIs
 * when no map IDs are configured or the exports are unreachable.
 *
 * Env: GOOGLE_MYMAPS_IDS (comma-separated list of My Maps mids)
 */

import { fetchText, fetchOverpassTiled } from './_liveHelpers.js';

const MAP_IDS = (process.env.GOOGLE_MYMAPS_IDS || '')
  .split(',')
  .map((s) => s.trim())
  .filter(Boolean);

function inJapanBbox(lon, lat) {
  return lon >= 122 && lon <= 154 && lat >= 24 && lat <= 46;
}

function parseKmlPlacemarks(kml, mapId) {
  const out = [];
  const placemarkRe = /<Placemark\b[^>]*>([\s\S]*?)<\/Placemark>/g;
  let m, i = 0;
  while ((m = placemarkRe.exec(kml)) !== null) {
    const block = m[1];
    const nameMatch = block.match(/<name>([\s\S]*?)<\/name>/);
    const descMatch = block.match(/<description>([\s\S]*?)<\/description>/);
    const coordMatch = block.match(/<coordinates>\s*([-0-9.,\s]+?)\s*<\/coordinates>/);
    if (!coordMatch) continue;
    const firstTuple = coordMatch[1].split(/\s+/)[0] || coordMatch[1];
    const [lonStr, latStr] = firstTuple.split(',');
    const lon = parseFloat(lonStr);
    const lat = parseFloat(latStr);
    if (Number.isNaN(lon) || Number.isNaN(lat)) continue;
    if (!inJapanBbox(lon, lat)) continue;
    out.push({
      id: `MYMAP_${mapId}_${i++}`,
      name: nameMatch ? nameMatch[1].replace(/<\!\[CDATA\[|\]\]>/g, '').trim() : null,
      description: descMatch ? descMatch[1].replace(/<\!\[CDATA\[|\]\]>/g, '').replace(/<[^>]*>/g, '').trim().slice(0, 300) : null,
      lon,
      lat,
      mapId,
    });
  }
  return out;
}

async function tryMyMaps() {
  if (!MAP_IDS.length) return null;
  const results = await Promise.all(
    MAP_IDS.map(async (mid) => {
      const url = `https://www.google.com/maps/d/kml?mid=${encodeURIComponent(mid)}&forcekml=1`;
      const kml = await fetchText(url, { timeoutMs: 12000 });
      if (!kml) return [];
      return parseKmlPlacemarks(kml, mid);
    }),
  );
  const all = results.flat();
  if (!all.length) return null;
  return all.map((p) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [p.lon, p.lat] },
    properties: {
      id: p.id,
      name: p.name || 'Placemark',
      description: p.description,
      my_map_id: p.mapId,
      country: 'JP',
      source: 'google_mymaps_kml',
    },
  }));
}

// Curated seed: iconic Japanese travel POIs commonly shared on My Maps
const SEED_POIS = [
  { name: 'Tokyo Skytree', lat: 35.7101, lon: 139.8107, category: 'landmark' },
  { name: 'Shibuya Crossing', lat: 35.6595, lon: 139.7006, category: 'landmark' },
  { name: 'Senso-ji Temple', lat: 35.7148, lon: 139.7967, category: 'temple' },
  { name: 'Meiji Jingu', lat: 35.6764, lon: 139.6993, category: 'shrine' },
  { name: 'Tsukiji Outer Market', lat: 35.6654, lon: 139.7707, category: 'market' },
  { name: 'Akihabara', lat: 35.7022, lon: 139.7744, category: 'district' },
  { name: 'Shinjuku Gyoen', lat: 35.6852, lon: 139.7100, category: 'park' },
  { name: 'Harajuku Takeshita St', lat: 35.6716, lon: 139.7050, category: 'district' },
  { name: 'Odaiba', lat: 35.6270, lon: 139.7758, category: 'district' },
  { name: 'Tokyo Tower', lat: 35.6586, lon: 139.7454, category: 'landmark' },
  { name: 'Fushimi Inari Taisha', lat: 34.9671, lon: 135.7727, category: 'shrine' },
  { name: 'Kinkaku-ji', lat: 35.0394, lon: 135.7292, category: 'temple' },
  { name: 'Arashiyama Bamboo Grove', lat: 35.0167, lon: 135.6719, category: 'nature' },
  { name: 'Nijo Castle', lat: 35.0142, lon: 135.7481, category: 'castle' },
  { name: 'Kiyomizu-dera', lat: 34.9949, lon: 135.7850, category: 'temple' },
  { name: 'Osaka Castle', lat: 34.6873, lon: 135.5259, category: 'castle' },
  { name: 'Dotonbori', lat: 34.6687, lon: 135.5023, category: 'district' },
  { name: 'Universal Studios Japan', lat: 34.6655, lon: 135.4323, category: 'theme_park' },
  { name: 'Nara Park Deer', lat: 34.6851, lon: 135.8429, category: 'park' },
  { name: 'Todai-ji', lat: 34.6890, lon: 135.8399, category: 'temple' },
  { name: 'Itsukushima Shrine (Torii)', lat: 34.2959, lon: 132.3200, category: 'shrine' },
  { name: 'Peace Memorial Park Hiroshima', lat: 34.3955, lon: 132.4536, category: 'memorial' },
  { name: 'Hakone Onsen', lat: 35.2330, lon: 139.1069, category: 'onsen' },
  { name: 'Kurobe Gorge', lat: 36.8167, lon: 137.6167, category: 'nature' },
  { name: 'Shirakawa-go', lat: 36.2580, lon: 136.9060, category: 'village' },
  { name: 'Kanazawa Kenrokuen', lat: 36.5621, lon: 136.6622, category: 'garden' },
  { name: 'Mt Koya Okunoin', lat: 34.2136, lon: 135.5944, category: 'temple' },
  { name: 'Himeji Castle', lat: 34.8394, lon: 134.6939, category: 'castle' },
  { name: 'Matsumoto Castle', lat: 36.2383, lon: 137.9686, category: 'castle' },
  { name: 'Nikko Toshogu', lat: 36.7581, lon: 139.5989, category: 'shrine' },
  { name: 'Lake Ashi Hakone', lat: 35.2033, lon: 139.0206, category: 'nature' },
  { name: 'Miyajima Island', lat: 34.2960, lon: 132.3180, category: 'island' },
  { name: 'Sapporo Odori Park', lat: 43.0608, lon: 141.3510, category: 'park' },
  { name: 'Otaru Canal', lat: 43.1991, lon: 141.0034, category: 'canal' },
  { name: 'Okinawa Churaumi Aquarium', lat: 26.6941, lon: 127.8778, category: 'aquarium' },
  { name: 'Shuri Castle', lat: 26.2171, lon: 127.7194, category: 'castle' },
];

function generateSeed() {
  return SEED_POIS.map((p, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [p.lon, p.lat] },
    properties: {
      id: `MYMAP_SEED_${String(i + 1).padStart(4, '0')}`,
      name: p.name,
      category: p.category,
      country: 'JP',
      source: 'google_mymaps_seed',
    },
  }));
}

/**
 * Live nationwide POI fallback: query OSM Overpass for every wikidata-tagged
 * tourism attraction in Japan. This approximates the kind of curated POI
 * collection commonly shared via Google My Maps and gives us thousands of
 * geocoded Japan landmarks that update with OSM contributions.
 */
async function tryOsmTourism() {
  return fetchOverpassTiled(
    (bbox) => [
      `node["tourism"="attraction"]["wikidata"](${bbox});`,
      `node["tourism"="viewpoint"](${bbox});`,
      `node["tourism"="museum"](${bbox});`,
      `node["historic"]["wikidata"](${bbox});`,
      `way["tourism"="attraction"]["wikidata"](${bbox});`,
    ].join(''),
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        id: `OSM_TOUR_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || el.tags?.tourism || 'POI',
        category: el.tags?.tourism || el.tags?.historic || 'attraction',
        wikidata: el.tags?.wikidata || null,
        wikipedia: el.tags?.wikipedia || null,
        country: 'JP',
        source: 'osm_tourism',
      },
    }),
    { queryTimeout: 180, timeoutMs: 90_000 },
  );
}

export default async function collectGoogleMyMaps() {
  let features = await tryMyMaps();
  let liveSource = 'google_mymaps_kml';
  if (!features || !features.length) {
    features = await tryOsmTourism();
    liveSource = 'osm_tourism';
  }
  const live = !!(features && features.length > 0);
  if (!live) {
    features = generateSeed();
    liveSource = 'google_mymaps_seed';
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'google-my-maps',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: liveSource,
      mapIds: MAP_IDS,
      description: 'Google My Maps public KML exports filtered to Japan bbox',
    },
    metadata: {},
  };
}
