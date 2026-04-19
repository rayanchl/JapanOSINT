/**
 * MLIT P02 — National Land Numerical Information: Airports
 * 国土数値情報 空港データ P02
 *
 *   https://nlftp.mlit.go.jp/ksj/gml/datalist/KsjTmplt-P02-v1_2.html
 *
 * All civilian and joint-use airfields in Japan as polygons.
 * Falls back to OSM aeroway=aerodrome if MLIT file not reachable.
 */

import { fetchJson, fetchOverpassTiled } from './_liveHelpers.js';

const ENV_URL = process.env.MLIT_P02_GEOJSON_URL || null;

const MIRROR_URLS = [
  'https://nlftp.mlit.go.jp/ksj/gml/data/P02/P02-22/P02-22.geojson',
  'https://nlftp.mlit.go.jp/ksj/gml/data/P02/P02-13/P02-13.geojson',
];

function centroidOf(geom) {
  if (!geom) return null;
  if (geom.type === 'Point') return geom.coordinates;
  if (geom.type === 'Polygon') {
    const ring = geom.coordinates?.[0];
    if (!ring || !ring.length) return null;
    const [sx, sy] = ring.reduce(
      ([ax, ay], [x, y]) => [ax + x, ay + y],
      [0, 0],
    );
    return [sx / ring.length, sy / ring.length];
  }
  if (geom.type === 'MultiPolygon') {
    const ring = geom.coordinates?.[0]?.[0];
    if (!ring || !ring.length) return null;
    const [sx, sy] = ring.reduce(
      ([ax, ay], [x, y]) => [ax + x, ay + y],
      [0, 0],
    );
    return [sx / ring.length, sy / ring.length];
  }
  return null;
}

function normalizeFeature(f, i) {
  if (!f || !f.geometry) return null;
  const props = f.properties || {};
  const coords = centroidOf(f.geometry);
  if (!coords) return null;
  const name =
    props.airportName || props.name
    || props.P02_004 || props.P02_001 || null;
  const icao = props.icao || props.P02_002 || null;
  const iata = props.iata || props.P02_003 || null;

  return {
    type: 'Feature',
    geometry: { type: 'Point', coordinates: coords },
    properties: {
      airport_id: `MLIT_P02_${i}`,
      name,
      icao,
      iata,
      classification: props.airportType || props.P02_005 || null,
      source: 'mlit_p02',
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

async function tryOsmAirports() {
  return fetchOverpassTiled(
    (bbox) => [
      `node["aeroway"="aerodrome"](${bbox});`,
      `way["aeroway"="aerodrome"](${bbox});`,
    ].join(''),
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        airport_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || null,
        icao: el.tags?.icao || null,
        iata: el.tags?.iata || null,
        classification: el.tags?.aerodrome || el.tags?.military ? 'joint' : 'civilian',
        source: 'osm_overpass_airport',
      },
    }),
    { queryTimeout: 120, timeoutMs: 60_000 },
  );
}

export default async function collectMlitP02Airports() {
  const urls = [ENV_URL, ...MIRROR_URLS].filter(Boolean);
  let features = null;
  let usedUrl = null;
  let liveSrc = null;

  for (const url of urls) {
    const result = await tryUrl(url);
    if (result) {
      features = result;
      usedUrl = url;
      liveSrc = 'mlit_p02_live';
      break;
    }
  }

  if (!features) {
    features = await tryOsmAirports();
    if (features && features.length) liveSrc = 'osm_overpass_airport';
  }

  const live = !!(features && features.length);
  features = features || [];

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: liveSrc || 'mlit_p02_empty',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      source_url: usedUrl,
      env_hint: live ? null : 'Set MLIT_P02_GEOJSON_URL to a hosted GeoJSON copy of MLIT KSJ P02.',
      description: 'MLIT KSJ P02 — nationwide airports / airfields',
    },
    metadata: {},
  };
}
