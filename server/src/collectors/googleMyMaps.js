/**
 * Google My Maps Collector
 *
 * Two-tier source chain:
 *   1. Google My Maps KML export — runs when GOOGLE_MYMAPS_IDS is set.
 *      Endpoint: https://www.google.com/maps/d/kml?mid=<MAP_ID>&forcekml=1
 *   2. OSM Overpass fallback — tiled nationwide sweep using the EXACT same
 *      query body and feature mapper as the unified famousPlaces collector,
 *      so the fallback features are indistinguishable from that layer's
 *      (tourism/historic/worship/theatre/arts_centre/leisure/natural, no
 *      wikidata filter, nodes + ways).
 *
 * Returns an empty FeatureCollection only when both tiers fail.
 *
 * Env: GOOGLE_MYMAPS_IDS (comma-separated list of My Maps mids)
 */

import { fetchText, fetchOverpassTiled } from './_liveHelpers.js';
import { osmPoiOverpassBody, osmPoiMapFeature } from './famousPlaces.js';

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

// OSM fallback — identical query + feature shape as famousPlaces collector.
async function tryOsmPois() {
  return fetchOverpassTiled(
    osmPoiOverpassBody,
    osmPoiMapFeature,
    { queryTimeout: 180, timeoutMs: 90_000 },
  );
}

export default async function collectGoogleMyMaps() {
  // Without GOOGLE_MYMAPS_IDS the KML tier has nothing to fetch, and the
  // Overpass fallback duplicates the famousPlaces collector while running a
  // ~90s nationwide query — every cold request looked like a TIMEOUT in the
  // iOS app. Short-circuit to a fast empty response with a hint so the layer
  // toggles cleanly; users with their own KMLs still get them via the env
  // var path below.
  if (!MAP_IDS.length) {
    return {
      type: 'FeatureCollection',
      features: [],
      _meta: {
        source: 'google-my-maps',
        fetchedAt: new Date().toISOString(),
        recordCount: 0,
        live: false,
        live_source: null,
        mapIds: [],
        env_hint: 'Set GOOGLE_MYMAPS_IDS to a comma-separated list of public Google My Maps mids to populate this layer.',
        description: 'Google My Maps public KML — no map IDs configured.',
      },
    };
  }

  let features = await tryMyMaps();
  let liveSource = 'google_mymaps_kml';
  if (!features || !features.length) {
    features = await tryOsmPois();
    liveSource = features && features.length > 0 ? 'osm_overpass' : null;
  }
  const list = features || [];
  const live = list.length > 0;
  return {
    type: 'FeatureCollection',
    features: list,
    _meta: {
      source: 'google-my-maps',
      fetchedAt: new Date().toISOString(),
      recordCount: list.length,
      live,
      live_source: live ? liveSource : null,
      mapIds: MAP_IDS,
      description: 'Google My Maps public KML (primary) with OSM POI fallback — matches famousPlaces scope when KML unavailable',
    },
  };
}
