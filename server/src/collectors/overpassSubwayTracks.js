/**
 * OSM subway / metro / tram tracks as LineString features for the
 * unified-subways layer. Companion to osmTransportSubways (which emits the
 * stations as Points).
 */

import { fetchOverpassWaysTiled } from './_liveHelpers.js';

export default async function collectOverpassSubwayTracks() {
  const features = await fetchOverpassWaysTiled(
    (bbox) => [
      `way["railway"="subway"](${bbox});`,
      `way["railway"="tram"](${bbox});`,
      `way["railway"="monorail"](${bbox});`,
    ].join(''),
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'LineString', coordinates: coords },
      properties: {
        line_id: `OSM_WAY_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || null,
        name_ja: el.tags?.name || el.tags?.['name:ja'] || null,
        operator: el.tags?.operator || el.tags?.network || null,
        railway: el.tags?.railway || null,
        line_ref: el.tags?.ref || null,
        country: 'JP',
        source: 'osm_overpass_subway_track',
      },
    }),
    { queryTimeout: 240, timeoutMs: 180_000 },
  );

  const list = features || [];
  return {
    type: 'FeatureCollection',
    features: list,
    _meta: {
      source: 'osm_overpass_subway_track',
      fetchedAt: new Date().toISOString(),
      recordCount: list.length,
      live: list.length > 0,
      description: 'OSM way[railway=subway|tram|monorail] tracks for the unified-subways layer',
    },
    metadata: {},
  };
}
