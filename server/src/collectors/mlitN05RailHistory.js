/**
 * MLIT N05 — National Land Numerical Information: Rail Network (historical)
 * 国土数値情報 鉄道（時系列）N05
 *
 *   https://nlftp.mlit.go.jp/ksj/gml/datalist/KsjTmplt-N05-v2_0.html
 *
 * Long-term history of railway lines and stations: opening year, operator
 * changes, and abolished lines. Useful for studying network evolution.
 * There is no OSM fallback for historical attributes — if MLIT is unreachable
 * we return an empty FeatureCollection with a hint.
 */

import { fetchJson } from './_liveHelpers.js';

const ENV_URL = process.env.MLIT_N05_GEOJSON_URL || null;

const MIRROR_URLS = [
  'https://nlftp.mlit.go.jp/ksj/gml/data/N05/N05-22/N05-22.geojson',
  'https://nlftp.mlit.go.jp/ksj/gml/data/N05/N05-21/N05-21.geojson',
];

function centroidOf(geom) {
  if (!geom) return null;
  if (geom.type === 'Point') return geom.coordinates;
  if (geom.type === 'LineString') {
    const mid = geom.coordinates[Math.floor(geom.coordinates.length / 2)];
    return mid;
  }
  if (geom.type === 'MultiLineString') {
    const first = geom.coordinates?.[0];
    if (!first || !first.length) return null;
    return first[Math.floor(first.length / 2)];
  }
  return null;
}

function normalizeFeature(f, i) {
  if (!f || !f.geometry) return null;
  const coords = centroidOf(f.geometry);
  if (!coords) return null;
  const props = f.properties || {};
  return {
    type: 'Feature',
    geometry: { type: 'Point', coordinates: coords },
    properties: {
      segment_id: `MLIT_N05_${i}`,
      line: props.lineName || props.N05_002 || null,
      operator: props.operatorCompany || props.operator || props.N05_003 || null,
      classification: props.railwayType || props.N05_001 || null,
      opened_year: props.openedYear || props.N05_004 || null,
      closed_year: props.closedYear || props.N05_005 || null,
      status: props.closedYear || props.N05_005 ? 'abolished' : 'active',
      source: 'mlit_n05',
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

export default async function collectMlitN05RailHistory() {
  const urls = [ENV_URL, ...MIRROR_URLS].filter(Boolean);
  let features = null;
  let usedUrl = null;
  let liveSrc = null;

  for (const url of urls) {
    const result = await tryUrl(url);
    if (result) {
      features = result;
      usedUrl = url;
      liveSrc = 'mlit_n05_live';
      break;
    }
  }

  const live = !!(features && features.length);
  features = features || [];

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: liveSrc || 'mlit_n05_empty',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      source_url: usedUrl,
      env_hint: live ? null : 'Set MLIT_N05_GEOJSON_URL to a hosted GeoJSON copy of MLIT KSJ N05 (long-term rail history).',
      description: 'MLIT KSJ N05 — long-term rail network history (active + abolished lines with opening/closure years)',
    },
    metadata: {},
  };
}
