/**
 * SoftBank crowd-analytics collector (softbank-crowd).
 *
 * SoftBank's crowd / population analytics ("全国うごき統計" via SoftBank
 * Synergy / Agoop) is a paid, contract-only product with no public API. No
 * key-free fallback exists, so without a contract key we return an honest
 * empty FeatureCollection (never fabricated points). The missing-key state
 * surfaces via the apiCredentials map ("needs API key" badge in the Sources
 * tab).
 *
 * When SOFTBANK_CROWD_API_KEY is present the documented contract endpoint is
 * attempted; its exact path is account-specific and unverified, so any
 * failure yields honest empty ('softbank-crowd_unavailable').
 */

import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

function result(features, source) {
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'SoftBank crowd / movement analytics over Japan (requires SOFTBANK_CROWD_API_KEY — paid contract API)',
    },
  };
}

export default async function collectSoftbankCrowd() {
  const key = envFor('SOFTBANK_CROWD_API_KEY');
  if (!key) return result([], 'softbank-crowd_no_key');

  // Contract-only endpoint — path is account-specific and unverified. Real
  // fetch attempted with the key; honest empty otherwise.
  const data = await fetchJson(
    'https://api.softbank.jp/ugoki-toukei/v1/crowd?area=japan',
    { timeoutMs: 20000, headers: { Authorization: `Bearer ${key}` } },
  ).catch(() => null);

  const rows = Array.isArray(data?.features) ? data.features
    : Array.isArray(data?.data) ? data.data : null;
  if (!rows || rows.length === 0) return result([], 'softbank-crowd_unavailable');

  const features = rows
    .filter((r) => {
      const lon = r.lon ?? r.longitude ?? r.geometry?.coordinates?.[0];
      const lat = r.lat ?? r.latitude ?? r.geometry?.coordinates?.[1];
      return lon != null && lat != null;
    })
    .map((r) => {
      const lon = r.lon ?? r.longitude ?? r.geometry?.coordinates?.[0];
      const lat = r.lat ?? r.latitude ?? r.geometry?.coordinates?.[1];
      return {
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [+lon, +lat] },
        properties: {
          mesh_code: r.mesh_code ?? r.meshCode ?? null,
          crowd: r.crowd ?? r.population ?? r.value ?? null,
          datetime: r.datetime ?? r.timestamp ?? null,
          source: 'softbank_crowd_api',
        },
      };
    });
  return result(features, 'softbank-crowd');
}
