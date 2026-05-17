/**
 * TikTok geotagged-content collector (tiktok-geo).
 *
 * TikTok exposes no stable key-free geo API. With TIKTOK_MS_TOKEN set we
 * query the discover endpoint; without it (or on failure) the collector
 * returns an honest empty result — never fabricated points. The missing
 * credential surfaces via the apiCredentials map in the Sources tab.
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

function result(features, source) {
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Geotagged TikTok content across Japan (requires TIKTOK_MS_TOKEN)',
    },
  };
}

export default async function collectTiktokGeo() {
  const tokenVal = envFor('TIKTOK_MS_TOKEN');
  if (!tokenVal) return result([], 'tiktok_no_token');

  const url = `https://www.tiktok.com/api/discover/place/?region=JP&msToken=${encodeURIComponent(tokenVal)}`;
  const data = await fetchJson(url, { timeoutMs: 12000 });
  const places = data?.body?.[0]?.exploreList;
  if (!Array.isArray(places)) return result([], 'tiktok_unavailable');

  const features = places
    .filter((p) => p?.cardItem?.geo?.lon != null && p?.cardItem?.geo?.lat != null)
    .map((p, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [+p.cardItem.geo.lon, +p.cardItem.geo.lat] },
      properties: {
        post_id: p.cardItem.id || `tiktok-${i}`,
        hashtag: p.cardItem.title || null,
        city: p.cardItem.subTitle || null,
        url: p.cardItem.link || null,
        source: 'tiktok_api',
      },
    }));
  return result(features, 'tiktok_api');
}
