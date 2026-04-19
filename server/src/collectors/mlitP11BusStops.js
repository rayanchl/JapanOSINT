/**
 * MLIT P11 — National Land Numerical Information: Bus Stops
 * 国土数値情報 バス停留所 P11
 *
 * Canonical nationwide bus stop (停留所) point dataset:
 *   https://nlftp.mlit.go.jp/ksj/gml/datalist/KsjTmplt-P11-v2_2.html
 *
 * ~200,000 bus stops across all operators.
 * Falls back to OSM highway=bus_stop if MLIT download is unavailable.
 */

import { fetchJson, fetchOverpassTiled } from './_liveHelpers.js';

const ENV_URL = process.env.MLIT_P11_GEOJSON_URL || null;

const MIRROR_URLS = [
  'https://nlftp.mlit.go.jp/ksj/gml/data/P11/P11-22/P11-22.geojson',
  'https://nlftp.mlit.go.jp/ksj/gml/data/P11/P11-10/P11-10.geojson',
];

function normalizeFeature(f, i) {
  if (!f || !f.geometry) return null;
  const props = f.properties || {};
  const stopName =
    props.busStopName || props.stop_name || props.name
    || props.P11_001 || null;
  const operator =
    props.operator || props.company || props.busOperator
    || props.P11_002 || null;

  return {
    type: 'Feature',
    geometry: f.geometry,
    properties: {
      stop_id: `MLIT_P11_${i}`,
      name: stopName,
      name_ja: stopName,
      operator,
      route: props.routeName || props.P11_003 || null,
      source: 'mlit_p11',
    },
  };
}

async function tryUrl(url) {
  const data = await fetchJson(url, { timeoutMs: 45000 });
  if (!data) return null;
  const raw = Array.isArray(data) ? data
    : Array.isArray(data.features) ? data.features
    : null;
  if (!raw) return null;
  const features = raw.map(normalizeFeature).filter(Boolean);
  return features.length > 0 ? features : null;
}

async function tryOsmBusStops() {
  return fetchOverpassTiled(
    (bbox) => [
      `node["highway"="bus_stop"](${bbox});`,
      `node["public_transport"="platform"]["bus"="yes"](${bbox});`,
      `node["amenity"="bus_station"](${bbox});`,
    ].join(''),
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        stop_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || 'Bus stop',
        name_ja: el.tags?.name || el.tags?.['name:ja'] || null,
        operator: el.tags?.operator || el.tags?.network || null,
        route: el.tags?.route_ref || null,
        shelter: el.tags?.shelter || null,
        wheelchair: el.tags?.wheelchair || null,
        source: 'osm_overpass_bus_stop',
      },
    }),
    { queryTimeout: 180, timeoutMs: 120_000 },
  );
}

export default async function collectMlitP11BusStops() {
  const urls = [ENV_URL, ...MIRROR_URLS].filter(Boolean);
  let features = null;
  let usedUrl = null;
  let liveSrc = null;

  for (const url of urls) {
    const result = await tryUrl(url);
    if (result) {
      features = result;
      usedUrl = url;
      liveSrc = 'mlit_p11_live';
      break;
    }
  }

  if (!features) {
    features = await tryOsmBusStops();
    if (features && features.length) liveSrc = 'osm_overpass_bus_stop';
  }

  const live = !!(features && features.length);
  features = features || [];

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: liveSrc || 'mlit_p11_empty',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      source_url: usedUrl,
      env_hint: live ? null : 'Set MLIT_P11_GEOJSON_URL to a hosted GeoJSON copy of MLIT KSJ P11 for ~200k bus stops.',
      description: 'MLIT KSJ P11 — nationwide bus stops (停留所)',
    },
    metadata: {},
  };
}
