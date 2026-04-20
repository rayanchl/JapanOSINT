/**
 * Japan Post Offices (郵便局) — nationwide OSM-backed locations.
 *
 * Japan Post's own map.japanpost.jp portal does not expose a documented
 * public GeoJSON endpoint; its ArcGIS backend needs browser-side XHR
 * inspection to extract. Until that's done, we sourcemap from OSM via
 * our existing tiled Overpass helper, which has good coverage
 * (>20k post offices tagged `amenity=post_office` nationwide).
 *
 * When a Japan Post authoritative feed becomes available, swap the
 * implementation without touching any caller.
 *
 * No auth. No rate limit (Overpass is rate-limited separately by the
 * shared `fetchOverpassTiled` helper).
 */

import { fetchOverpassTiled } from './_liveHelpers.js';

export default async function collectJapanPostOffices() {
  const features = await fetchOverpassTiled(
    (bbox) => `node["amenity"="post_office"](${bbox});way["amenity"="post_office"](${bbox});`,
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        id: `PO_OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || 'Post Office',
        name_ja: el.tags?.['name:ja'] || el.tags?.name || null,
        operator: el.tags?.operator || null,
        addr_postcode: el.tags?.['addr:postcode'] || null,
        addr_city: el.tags?.['addr:city'] || null,
        opening_hours: el.tags?.opening_hours || null,
        phone: el.tags?.phone || null,
        website: el.tags?.website || el.tags?.['contact:website'] || null,
        source: 'osm_overpass_post_office',
      },
    }),
    { queryTimeout: 180, timeoutMs: 90_000 },
  );

  const live = Array.isArray(features) && features.length > 0;

  return {
    type: 'FeatureCollection',
    features: live ? features : [],
    _meta: {
      source: live ? 'osm_overpass_post_office' : 'osm_unavailable',
      fetchedAt: new Date().toISOString(),
      recordCount: live ? features.length : 0,
      description: 'Japan Post office locations (郵便局) from OSM amenity=post_office',
    },
    metadata: {},
  };
}
