/**
 * Flickr geotagged-photos collector (flickr-geo).
 *
 * Live: Flickr REST `flickr.photos.search` with a Japan bbox and has_geo=1
 * when FLICKR_API_KEY is set. There is no key-free fallback for Flickr
 * search, so without a key (or on failure) the collector returns an honest
 * empty result — never fabricated points. The missing-key state surfaces
 * via the apiCredentials map ("needs API key" badge in the Sources tab).
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

// Japan bounding box: minLon,minLat,maxLon,maxLat
const JP_BBOX = '122.0,24.0,146.0,46.0';

function result(features, source) {
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Geotagged Flickr photos across Japan (requires FLICKR_API_KEY)',
    },
  };
}

export default async function collectFlickrGeo() {
  const key = envFor('FLICKR_API_KEY');
  if (!key) return result([], 'flickr_no_key');

  const url = 'https://api.flickr.com/services/rest/'
    + '?method=flickr.photos.search&format=json&nojsoncallback=1'
    + `&api_key=${encodeURIComponent(key)}`
    + `&bbox=${encodeURIComponent(JP_BBOX)}&has_geo=1&content_type=1`
    + '&extras=geo,url_s,owner_name,date_taken&per_page=250&sort=date-posted-desc';
  const data = await fetchJson(url, { timeoutMs: 15000 });
  const photos = data?.photos?.photo;
  if (!Array.isArray(photos)) return result([], 'flickr_unavailable');

  const features = photos
    .filter((p) => p.longitude != null && p.latitude != null
      && +p.longitude !== 0 && +p.latitude !== 0)
    .map((p) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [+p.longitude, +p.latitude] },
      properties: {
        photo_id: p.id,
        title: p.title || null,
        owner: p.ownername || p.owner || null,
        url: p.url_s || `https://www.flickr.com/photos/${p.owner}/${p.id}`,
        taken: p.datetaken || null,
        source: 'flickr_api',
      },
    }));
  return result(features, 'flickr_api');
}
