/**
 * Coast Guard Stations Collector
 * Japan Coast Guard offices/stations across all 11 RCGH regions.
 * Distinct from `jcgPatrol` (which lists patrol vessel base ports + RCGH HQs)
 * — this layer contains the smaller offices and signal stations.
 */

import { fetchOverpass } from './_liveHelpers.js';

const SEED_STATIONS = [
  // 1st RCGH (Hokkaido)
  { name: '稚内海上保安部', lat: 45.4081, lon: 141.6789, region: '1st', kind: 'office' },
  { name: '紋別海上保安部', lat: 44.3567, lon: 143.3517, region: '1st', kind: 'office' },
  { name: '網走海上保安署', lat: 44.0050, lon: 144.2725, region: '1st', kind: 'station' },
  { name: '苫小牧海上保安署', lat: 42.6333, lon: 141.6133, region: '1st', kind: 'station' },
  { name: '室蘭海上保安部', lat: 42.3344, lon: 140.9747, region: '1st', kind: 'office' },
  { name: '羅臼海上保安署', lat: 44.0214, lon: 145.1922, region: '1st', kind: 'station' },
  // 2nd RCGH (Tohoku)
  { name: '酒田海上保安部', lat: 38.9106, lon: 139.8350, region: '2nd', kind: 'office' },
  { name: '宮古海上保安署', lat: 39.6450, lon: 141.9700, region: '2nd', kind: 'office' },
  { name: '釜石海上保安部', lat: 39.2706, lon: 141.8881, region: '2nd', kind: 'office' },
  { name: '気仙沼海上保安署', lat: 38.9081, lon: 141.5722, region: '2nd', kind: 'station' },
  // 3rd RCGH (Kanto)
  { name: '下田海上保安部', lat: 34.6781, lon: 138.9450, region: '3rd', kind: 'office' },
  { name: '清水海上保安部', lat: 35.0156, lon: 138.5036, region: '3rd', kind: 'office' },
  { name: '田子の浦海上保安署', lat: 35.1294, lon: 138.6797, region: '3rd', kind: 'station' },
  { name: '神津島海上保安署', lat: 34.2103, lon: 139.1389, region: '3rd', kind: 'station' },
  { name: '父島海上保安署', lat: 27.0856, lon: 142.2003, region: '3rd', kind: 'station' },
  // 4th RCGH (Tokai)
  { name: '四日市海上保安部', lat: 34.9472, lon: 136.6422, region: '4th', kind: 'office' },
  { name: '鳥羽海上保安部', lat: 34.4836, lon: 136.8419, region: '4th', kind: 'office' },
  { name: '衣浦海上保安署', lat: 34.8703, lon: 136.9817, region: '4th', kind: 'station' },
  // 5th RCGH (Kinki)
  { name: '神戸海上保安部', lat: 34.6803, lon: 135.1894, region: '5th', kind: 'office' },
  { name: '姫路海上保安部', lat: 34.7783, lon: 134.6961, region: '5th', kind: 'office' },
  { name: '田辺海上保安部', lat: 33.7308, lon: 135.3756, region: '5th', kind: 'office' },
  { name: '関西空港海上保安署', lat: 34.4361, lon: 135.2300, region: '5th', kind: 'station' },
  // 6th RCGH (Setouchi)
  { name: '坂出海上保安署', lat: 34.3192, lon: 133.8606, region: '6th', kind: 'station' },
  { name: '徳島海上保安部', lat: 34.0708, lon: 134.5550, region: '6th', kind: 'office' },
  { name: '今治海上保安部', lat: 34.0639, lon: 132.9956, region: '6th', kind: 'office' },
  { name: '尾道海上保安部', lat: 34.4047, lon: 133.1908, region: '6th', kind: 'office' },
  { name: '徳山海上保安部', lat: 34.0383, lon: 131.8025, region: '6th', kind: 'office' },
  // 7th RCGH (Kyushu N)
  { name: '門司海上保安部', lat: 33.9358, lon: 130.9608, region: '7th', kind: 'office' },
  { name: '若松海上保安部', lat: 33.9056, lon: 130.8053, region: '7th', kind: 'office' },
  { name: '唐津海上保安部', lat: 33.4456, lon: 129.9783, region: '7th', kind: 'office' },
  { name: '伊万里海上保安署', lat: 33.2700, lon: 129.8736, region: '7th', kind: 'station' },
  { name: '比田勝海上保安署', lat: 34.6750, lon: 129.4756, region: '7th', kind: 'station' },
  // 8th RCGH (Sea of Japan)
  { name: '舞鶴海上保安部', lat: 35.4775, lon: 135.3819, region: '8th', kind: 'office' },
  { name: '境海上保安部', lat: 35.5447, lon: 133.2486, region: '8th', kind: 'office' },
  // 9th RCGH (Niigata)
  { name: '直江津海上保安署', lat: 37.1819, lon: 138.2522, region: '9th', kind: 'station' },
  { name: '七尾海上保安部', lat: 37.0414, lon: 136.9622, region: '9th', kind: 'office' },
  { name: '伏木海上保安部', lat: 36.7956, lon: 137.0533, region: '9th', kind: 'office' },
  // 10th RCGH (Kyushu S)
  { name: '指宿海上保安署', lat: 31.2528, lon: 130.6433, region: '10th', kind: 'station' },
  { name: '油津海上保安部', lat: 31.5717, lon: 131.4083, region: '10th', kind: 'office' },
  { name: '細島海上保安署', lat: 32.4292, lon: 131.6736, region: '10th', kind: 'station' },
  { name: '名瀬海上保安部', lat: 28.3739, lon: 129.4944, region: '10th', kind: 'office' },
  // 11th RCGH (Okinawa)
  { name: '中城海上保安部', lat: 26.2961, lon: 127.7822, region: '11th', kind: 'office' },
  { name: '本部海上保安部', lat: 26.6494, lon: 127.8767, region: '11th', kind: 'office' },
  { name: '与那国海上保安署', lat: 24.4567, lon: 122.9858, region: '11th', kind: 'station' },
];

async function tryOverpass() {
  return fetchOverpass(
    [
      'node["operator"~"海上保安"](area.jp);',
      'way["operator"~"海上保安"](area.jp);',
      'node["amenity"="coast_guard"](area.jp);',
      'way["amenity"="coast_guard"](area.jp);',
      'node["office"="government"]["government"="coast_guard"](area.jp);',
    ].join(''),
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        station_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || 'JCG Station',
        name_ja: el.tags?.name || null,
        operator: el.tags?.operator || null,
        kind: el.tags?.amenity === 'coast_guard' ? 'coast_guard' : 'station',
        source: 'osm_overpass',
      },
    }),
    60_000,
    { limit: 0, queryTimeout: 180 },
  );
}

function generateSeedData() {
  return SEED_STATIONS.map((s, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [s.lon, s.lat] },
    properties: {
      station_id: `JCG_S_${String(i + 1).padStart(5, '0')}`,
      name: s.name,
      region: s.region,
      kind: s.kind,
      country: 'JP',
      source: 'jcg_stations_seed',
    },
  }));
}

export default async function collectCoastGuardStations() {
  let features = await tryOverpass();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'coast_guard_stations',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Japan Coast Guard offices and signal stations across 11 regions',
    },
    metadata: {},
  };
}
