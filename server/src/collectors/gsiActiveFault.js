/**
 * Active fault traces — gsi-active-fault.
 *
 * GSI publishes the「都市圏活断層図」as raster/vector map tiles, not as a
 * clean bulk GeoJSON download. The machine-readable national active-fault
 * trace dataset is the AIST (産総研) "活断層データベース" GeoJSON service.
 * This collector does a real best-effort fetch of a public active-fault
 * GeoJSON; if the configured/known endpoint is unreachable or returns no
 * usable geometry, we return an HONEST EMPTY FeatureCollection.
 *
 * NOTE: the AIST/GSI bulk endpoints are not officially documented as a stable
 * open API and are UNVERIFIED here. An override URL may be supplied via the
 * optional env var GSI_ACTIVE_FAULT_GEOJSON_URL. No fault geometry is ever
 * fabricated — empty-on-miss keeps the contract honest.
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';
import { intelHashKey } from '../utils/intelHelpers.js';

// Unverified candidate endpoint (AIST active-fault DB GeoJSON export). Kept
// as a best-effort attempt only; honest empty on failure.
const CANDIDATE_URLS = [
  'https://gbank.gsj.jp/activefault/cgi-bin/geojson.cgi',
];

function emptyResult(source) {
  return {
    type: 'FeatureCollection',
    features: [],
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: 0,
      description: 'Active fault traces (AIST/GSI active-fault database, best-effort)',
    },
  };
}

function normalize(data) {
  const src = Array.isArray(data?.features) ? data.features : null;
  if (!src) return [];
  const out = [];
  for (const f of src) {
    if (!f?.geometry || !f.geometry.type || !f.geometry.coordinates) continue;
    const p = f.properties || {};
    out.push({
      type: 'Feature',
      geometry: f.geometry,
      properties: {
        fault_id:
          p.id ?? p.fault_id ?? p.name ?? `AF_${intelHashKey(JSON.stringify(f.geometry))}`,
        name: p.name ?? p.fault_name ?? p.断層名 ?? null,
        fault_type: p.type ?? p.断層型 ?? null,
        activity: p.activity ?? p.活動度 ?? null,
        source: 'gsi_active_fault',
      },
    });
  }
  return out;
}

export default async function collectGsiActiveFault() {
  const override = envFor('GSI_ACTIVE_FAULT_GEOJSON_URL');
  const urls = override ? [override, ...CANDIDATE_URLS] : CANDIDATE_URLS;

  for (const url of urls) {
    let data = null;
    try {
      data = await fetchJson(url, { timeoutMs: 20000 });
    } catch {
      data = null;
    }
    if (!data) continue;
    const features = normalize(data);
    if (features.length > 0) {
      return {
        type: 'FeatureCollection',
        features,
        _meta: {
          source: 'gsi_active_fault',
          fetchedAt: new Date().toISOString(),
          recordCount: features.length,
          description: 'Active fault traces (AIST/GSI active-fault database, best-effort)',
        },
      };
    }
  }

  return emptyResult('gsi_active_fault_unavailable');
}
