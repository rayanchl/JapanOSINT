/**
 * MLIT N07 — National Land Numerical Information: Bus Routes
 * 国土数値情報 バスルートデータ N07
 *
 * Canonical nationwide bus route geometry from MLIT:
 *   https://nlftp.mlit.go.jp/ksj/gml/datalist/KsjTmplt-N07-v1_1.html
 *
 * Like N02, the canonical distribution is GML/Shapefile. We accept a
 * pre-converted GeoJSON URL via env (MLIT_N07_GEOJSON_URL) and fall back
 * to a small list of known mirror URLs, then OSM route=bus relations.
 */

import { fetchJson, fetchOverpassTiled } from './_liveHelpers.js';

const ENV_URL = process.env.MLIT_N07_GEOJSON_URL || null;

const MIRROR_URLS = [
  'https://nlftp.mlit.go.jp/ksj/gml/data/N07/N07-11/N07-11_BusRoute.geojson',
];

function normalizeFeature(f, i) {
  if (!f || !f.geometry) return null;
  const props = f.properties || {};
  const routeName =
    props.routeName || props.route_name || props.name
    || props.N07_002 || null;
  const operator =
    props.operatorName || props.operator || props.company
    || props.N07_003 || null;

  return {
    type: 'Feature',
    geometry: f.geometry,
    properties: {
      route_id: `MLIT_N07_${i}`,
      name: routeName,
      operator,
      route_type: props.routeType || props.N07_001 || 'bus',
      source: 'mlit_n07',
    },
  };
}

async function tryUrl(url) {
  const data = await fetchJson(url, { timeoutMs: 30000 });
  if (!data) return null;
  const raw = Array.isArray(data) ? data
    : Array.isArray(data.features) ? data.features
    : null;
  if (!raw) return null;
  const features = raw.map(normalizeFeature).filter(Boolean);
  return features.length > 0 ? features : null;
}

async function tryOsmBusRoutes() {
  return fetchOverpassTiled(
    (bbox) => [
      `relation["route"="bus"](${bbox});`,
    ].join(''),
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        route_id: `OSM_REL_${el.id}`,
        name: el.tags?.name || el.tags?.ref || null,
        operator: el.tags?.operator || el.tags?.network || null,
        ref: el.tags?.ref || null,
        from: el.tags?.from || null,
        to: el.tags?.to || null,
        route_type: 'bus',
        source: 'osm_overpass_bus_route',
      },
    }),
    { queryTimeout: 180, timeoutMs: 90_000 },
  );
}

export default async function collectMlitN07BusRoutes() {
  const urls = [ENV_URL, ...MIRROR_URLS].filter(Boolean);
  let features = null;
  let usedUrl = null;
  let liveSrc = null;

  for (const url of urls) {
    const result = await tryUrl(url);
    if (result) {
      features = result;
      usedUrl = url;
      liveSrc = 'mlit_n07_live';
      break;
    }
  }

  if (!features) {
    features = await tryOsmBusRoutes();
    if (features && features.length) liveSrc = 'osm_overpass_bus_route';
  }

  const live = !!(features && features.length);
  features = features || [];

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: liveSrc || 'mlit_n07_empty',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      source_url: usedUrl,
      env_hint: live ? null : 'Set MLIT_N07_GEOJSON_URL to a hosted GeoJSON copy of MLIT KSJ N07 for nationwide bus route geometry.',
      description: 'MLIT KSJ N07 — nationwide bus routes (government dataset, ~11 release)',
    },
    metadata: {},
  };
}
