/**
 * Social Media Collector
 *
 * Real geotagged data only. Queries the Wikipedia GeoSearch API over a set of
 * Japanese urban hubs and returns Wikipedia articles that carry real
 * coordinates. No Instagram/Twitter/Flickr synthetic seed generation.
 *
 * Returns an empty FeatureCollection when the live call yields nothing.
 */

import { fetchJson } from './_liveHelpers.js';

// Geo hubs we sample to pull nearby real Wikipedia articles with coords.
// Coordinates used only for the GeoSearch request — each returned article
// carries its own real lat/lon.
const GEO_HUBS = [
  { area: 'Tokyo',    lat: 35.6812, lon: 139.7671 },
  { area: 'Osaka',    lat: 34.6937, lon: 135.5023 },
  { area: 'Kyoto',    lat: 35.0116, lon: 135.7681 },
  { area: 'Nagoya',   lat: 35.1815, lon: 136.9066 },
  { area: 'Fukuoka',  lat: 33.5902, lon: 130.4017 },
  { area: 'Sapporo',  lat: 43.0621, lon: 141.3544 },
  { area: 'Yokohama', lat: 35.4437, lon: 139.6380 },
  { area: 'Kobe',     lat: 34.6901, lon: 135.1955 },
  { area: 'Hiroshima',lat: 34.3853, lon: 132.4553 },
  { area: 'Sendai',   lat: 38.2682, lon: 140.8694 },
  { area: 'Naha',     lat: 26.2124, lon: 127.6809 },
];

async function fetchGeoArticles(hub) {
  const url =
    `https://en.wikipedia.org/w/api.php?action=query&format=json&origin=*` +
    `&list=geosearch&gscoord=${hub.lat}|${hub.lon}&gsradius=10000&gslimit=30`;
  const data = await fetchJson(url, { timeoutMs: 7000 });
  const pages = data?.query?.geosearch;
  if (!Array.isArray(pages)) return [];
  return pages
    .filter((p) => Number.isFinite(p.lat) && Number.isFinite(p.lon))
    .map((p) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [p.lon, p.lat] },
      properties: {
        post_id: `WIKI_${p.pageid}`,
        platform: 'wikipedia',
        content_type: 'article',
        area_name: p.title,
        hub: hub.area,
        url: `https://en.wikipedia.org/?curid=${p.pageid}`,
        timestamp: new Date().toISOString(),
        has_location: true,
        source: 'wikipedia_geosearch',
      },
    }));
}

export default async function collectSocialMedia() {
  const results = await Promise.all(GEO_HUBS.map((h) => fetchGeoArticles(h)));
  const seen = new Set();
  const features = [];
  for (const batch of results) {
    for (const f of batch) {
      const id = f.properties.post_id;
      if (seen.has(id)) continue;
      seen.add(id);
      features.push(f);
    }
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'social_media',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: features.length > 0,
      live_source: features.length > 0 ? 'wikipedia_geosearch' : null,
      description: 'Geolocated Wikipedia articles around Japanese urban hubs (live only)',
    },
    metadata: {},
  };
}
