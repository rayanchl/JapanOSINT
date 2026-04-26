// server/src/collectors/socialMedia.js
import { fetchJson } from './_liveHelpers.js';
import db from '../utils/database.js';

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

const stmtUpsertPost = db.prepare(`
  INSERT INTO social_posts
    (post_uid, platform, author, text, title, url, media_urls, language,
     posted_at, lat, lon, geo_source, properties)
  VALUES
    (@post_uid, @platform, @author, @text, @title, @url, @media_urls, @language,
     @posted_at, @lat, @lon, @geo_source, @properties)
  ON CONFLICT(post_uid) DO UPDATE SET
    text = excluded.text,
    title = excluded.title,
    url = excluded.url,
    media_urls = excluded.media_urls,
    lat = COALESCE(social_posts.lat, excluded.lat),
    lon = COALESCE(social_posts.lon, excluded.lon),
    geo_source = COALESCE(social_posts.geo_source, excluded.geo_source)
`);

const stmtSelectGeocoded = db.prepare(`
  SELECT post_uid, platform, author, text, title, url, lat, lon, geo_source,
         llm_place_name, fetched_at
  FROM social_posts
  WHERE platform = 'wikipedia' AND lat IS NOT NULL AND lon IS NOT NULL
  ORDER BY fetched_at DESC
  LIMIT 5000
`);

async function fetchGeoArticles(hub) {
  const url =
    `https://en.wikipedia.org/w/api.php?action=query&format=json&origin=*` +
    `&list=geosearch&gscoord=${hub.lat}|${hub.lon}&gsradius=10000&gslimit=30`;
  const data = await fetchJson(url, { timeoutMs: 7000 });
  const pages = data?.query?.geosearch;
  if (!Array.isArray(pages)) return 0;
  let n = 0;
  for (const p of pages) {
    if (!Number.isFinite(p.lat) || !Number.isFinite(p.lon)) continue;
    stmtUpsertPost.run({
      post_uid: `WIKI_${p.pageid}`,
      platform: 'wikipedia',
      author: null,
      text: null,
      title: p.title,
      url: `https://en.wikipedia.org/?curid=${p.pageid}`,
      media_urls: null,
      language: 'en',
      posted_at: null,
      lat: p.lat,
      lon: p.lon,
      geo_source: 'native_geo',
      properties: JSON.stringify({ hub: hub.area }),
    });
    n++;
  }
  return n;
}

export default async function collectSocialMedia() {
  await Promise.all(GEO_HUBS.map((h) => fetchGeoArticles(h).catch(() => 0)));
  const rows = stmtSelectGeocoded.all();
  const features = rows.map((r) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [r.lon, r.lat] },
    properties: {
      post_id: r.post_uid,
      platform: r.platform,
      content_type: 'article',
      area_name: r.title,
      url: r.url,
      timestamp: r.fetched_at,
      has_location: true,
      source: r.geo_source === 'llm_gsi' ? 'wikipedia_geosearch+llm' : 'wikipedia_geosearch',
    },
  }));
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
