/**
 * NTT Docomo "Mobile-kukan Toukei" population collector (docomo-population).
 *
 * NTT Docomo's mobile spatial-statistics population product is a paid,
 * contract-only service with no public API. There is no key-free fallback,
 * so without a contract API key we return an honest empty FeatureCollection
 * (never fabricated mesh/population points). The missing-key state surfaces
 * via the apiCredentials map ("needs API key" badge in the Sources tab).
 *
 * When DOCOMO_POPULATION_API_KEY is present we attempt the documented
 * contract endpoint; its exact path is account-specific and unverified, so
 * any failure yields honest empty ('docomo-population_unavailable').
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
      description: 'NTT Docomo mobile spatial-statistics population mesh over Japan (requires DOCOMO_POPULATION_API_KEY — paid contract API)',
    },
  };
}

export default async function collectDocomoPopulation() {
  const key = envFor('DOCOMO_POPULATION_API_KEY');
  if (!key) return result([], 'docomo-population_no_key');

  // Contract-only endpoint — path is account/provisioning specific and
  // unverified. Real fetch attempted with the key; honest empty otherwise.
  const data = await fetchJson(
    'https://api.docomo-datasquare.co.jp/v1/population/mesh?area=japan',
    { timeoutMs: 20000, headers: { Authorization: `Bearer ${key}` } },
  ).catch(() => null);

  const rows = Array.isArray(data?.features) ? data.features
    : Array.isArray(data?.data) ? data.data : null;
  if (!rows || rows.length === 0) return result([], 'docomo-population_unavailable');

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
          population: r.population ?? r.value ?? null,
          datetime: r.datetime ?? r.timestamp ?? null,
          source: 'docomo_population_api',
        },
      };
    });
  return result(features, 'docomo-population');
}
