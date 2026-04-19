/**
 * MLIT C02 — National Land Numerical Information: Ports / Harbours
 * 国土数値情報 港湾データ C02
 *
 *   https://nlftp.mlit.go.jp/ksj/gml/datalist/KsjTmplt-C02-v2_2.html
 *
 * Covers all MLIT-designated ports: International Strategic Ports,
 * International Hub Ports, Important Ports, Local Ports, and Fishing Ports.
 * Falls back to OSM seamark:type=harbour + harbour=yes if MLIT unreachable.
 */

import { fetchJson, fetchOverpassTiled } from './_liveHelpers.js';

const ENV_URL = process.env.MLIT_C02_GEOJSON_URL || null;

const MIRROR_URLS = [
  'https://nlftp.mlit.go.jp/ksj/gml/data/C02/C02-22/C02-22.geojson',
  'https://nlftp.mlit.go.jp/ksj/gml/data/C02/C02-06/C02-06.geojson',
];

function centroidOf(geom) {
  if (!geom) return null;
  if (geom.type === 'Point') return geom.coordinates;
  if (geom.type === 'LineString') {
    const mid = geom.coordinates[Math.floor(geom.coordinates.length / 2)];
    return mid;
  }
  if (geom.type === 'Polygon') {
    const ring = geom.coordinates?.[0];
    if (!ring || !ring.length) return null;
    const [sx, sy] = ring.reduce(([ax, ay], [x, y]) => [ax + x, ay + y], [0, 0]);
    return [sx / ring.length, sy / ring.length];
  }
  return null;
}

function normalizeFeature(f, i) {
  if (!f || !f.geometry) return null;
  const coords = centroidOf(f.geometry);
  if (!coords) return null;
  const props = f.properties || {};
  const name =
    props.portName || props.name
    || props.C02_003 || props.C02_001 || null;
  const classification =
    props.portClass || props.classification
    || props.C02_002 || null;

  return {
    type: 'Feature',
    geometry: { type: 'Point', coordinates: coords },
    properties: {
      port_id: `MLIT_C02_${i}`,
      name,
      name_ja: name,
      classification,
      administrator: props.administrator || props.C02_004 || null,
      source: 'mlit_c02',
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

async function tryOsmHarbours() {
  return fetchOverpassTiled(
    (bbox) => [
      `node["seamark:type"="harbour"](${bbox});`,
      `node["harbour"="yes"](${bbox});`,
      `way["harbour"="yes"](${bbox});`,
      `node["leisure"="marina"](${bbox});`,
      `way["leisure"="marina"](${bbox});`,
    ].join(''),
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        port_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || null,
        name_ja: el.tags?.name || el.tags?.['name:ja'] || null,
        classification: el.tags?.['seamark:harbour:category'] || el.tags?.leisure || null,
        administrator: el.tags?.operator || null,
        source: 'osm_overpass_harbour',
      },
    }),
    { queryTimeout: 120, timeoutMs: 60_000 },
  );
}

export default async function collectMlitC02Ports() {
  const urls = [ENV_URL, ...MIRROR_URLS].filter(Boolean);
  let features = null;
  let usedUrl = null;
  let liveSrc = null;

  for (const url of urls) {
    const result = await tryUrl(url);
    if (result) {
      features = result;
      usedUrl = url;
      liveSrc = 'mlit_c02_live';
      break;
    }
  }

  if (!features) {
    features = await tryOsmHarbours();
    if (features && features.length) liveSrc = 'osm_overpass_harbour';
  }

  const live = !!(features && features.length);
  features = features || [];

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: liveSrc || 'mlit_c02_empty',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      source_url: usedUrl,
      env_hint: live ? null : 'Set MLIT_C02_GEOJSON_URL to a hosted GeoJSON copy of MLIT KSJ C02.',
      description: 'MLIT KSJ C02 — ports and harbours (International Strategic / Important / Local / Fishing)',
    },
    metadata: {},
  };
}
