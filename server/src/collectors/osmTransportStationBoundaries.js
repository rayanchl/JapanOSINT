/**
 * Nationwide OSM station-building footprints.
 *
 * Pulls every closed way tagged `railway=station` + `building|area=yes` or
 * `public_transport=station` + `building|area=yes` across Japan via the
 * standard tiled-Overpass helper. The output is one Polygon per station
 * building; large interchanges (Shinjuku, Tokyo, Umeda, Shibuya, …) render
 * as translucent floor-plan fills at high zoom.
 *
 * Was previously scoped to four metro bboxes; the clusterer now links each
 * footprint to the cluster whose centroid falls inside its bbox, so
 * nationwide coverage matters.
 */

import { fetchOverpassWaysTiled } from './_liveHelpers.js';

function isClosed(coords) {
  if (!Array.isArray(coords) || coords.length < 4) return false;
  const a = coords[0];
  const b = coords[coords.length - 1];
  return a && b && a[0] === b[0] && a[1] === b[1];
}

export default async function collectOsmTransportStationBoundaries() {
  const features = await fetchOverpassWaysTiled(
    (bbox) => [
      `way["railway"="station"]["building"](${bbox});`,
      `way["railway"="station"]["area"="yes"](${bbox});`,
      `way["public_transport"="station"]["building"](${bbox});`,
      `way["public_transport"="station"]["area"="yes"](${bbox});`,
    ].join(''),
    (el, _i, coords) => {
      if (!isClosed(coords)) return null;
      return {
        type: 'Feature',
        geometry: { type: 'Polygon', coordinates: [coords] },
        properties: {
          footprint_id: `OSM_WAY_${el.id}`,
          name: el.tags?.['name:en'] || el.tags?.name || 'Station',
          name_ja: el.tags?.name || el.tags?.['name:ja'] || null,
          operator: el.tags?.operator || null,
          railway: el.tags?.railway || null,
          building: el.tags?.building || null,
          country: 'JP',
          source: 'osm_station_boundary',
        },
      };
    },
    { queryTimeout: 180, timeoutMs: 180_000 },
  );

  const list = (features || []).filter(Boolean);
  return {
    type: 'FeatureCollection',
    features: list,
    _meta: {
      source: 'osm_transport_station_boundaries',
      fetchedAt: new Date().toISOString(),
      recordCount: list.length,
      live: list.length > 0,
      description: 'Nationwide OSM station-building polygons (railway=station + building/area).',
    },
    metadata: {},
  };
}
