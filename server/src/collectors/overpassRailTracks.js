/**
 * OSM rail tracks (mainline + light rail) as LineString features for the
 * unified-trains layer. Companion to osmTransportTrains (which emits the
 * stations as Points).
 *
 * Subway tunnels and tram tracks are excluded — they belong to the subway
 * unified layer.
 */

import { fetchOverpassWaysTiled } from './_liveHelpers.js';
import { computeLineColor } from './_lineColor.js';

export default async function collectOverpassRailTracks() {
  const features = await fetchOverpassWaysTiled(
    (bbox) => [
      `way["railway"="rail"](${bbox});`,
      `way["railway"="light_rail"](${bbox});`,
      `way["railway"="narrow_gauge"](${bbox});`,
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
        electrified: el.tags?.electrified || null,
        gauge: el.tags?.gauge || null,
        usage: el.tags?.usage || null,
        colour: el.tags?.colour || null,
        line_ref: el.tags?.ref || null,
        line_color: computeLineColor(el.tags),
        country: 'JP',
        source: 'osm_overpass_rail_track',
      },
    }),
    { queryTimeout: 240, timeoutMs: 180_000 },
  );

  const list = features || [];
  return {
    type: 'FeatureCollection',
    features: list,
    _meta: {
      source: 'osm_overpass_rail_track',
      fetchedAt: new Date().toISOString(),
      recordCount: list.length,
      live: list.length > 0,
      description: 'OSM way[railway=rail|light_rail|narrow_gauge] tracks for the unified-trains layer',
    },
  };
}
