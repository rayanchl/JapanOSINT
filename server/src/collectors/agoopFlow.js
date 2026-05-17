/**
 * Agoop flow-population collector (agoop-flow).
 *
 * Agoop (ソフトバンク系) sells "流動人口データ" (flow population) derived
 * from GPS point data. It is a paid, contract-only product with no public
 * API. No key-free fallback exists, so without a contract key we return an
 * honest empty FeatureCollection (never fabricated GPS points). The
 * missing-key state surfaces via the apiCredentials map ("needs API key"
 * badge in the Sources tab).
 *
 * When AGOOP_API_KEY is present the documented contract endpoint is
 * attempted; its exact path is account-specific and unverified, so any
 * failure yields honest empty ('agoop-flow_unavailable').
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
      description: 'Agoop flow-population (GPS-derived) over Japan (requires AGOOP_API_KEY — paid contract API)',
    },
  };
}

export default async function collectAgoopFlow() {
  const key = envFor('AGOOP_API_KEY');
  if (!key) return result([], 'agoop-flow_no_key');

  // Contract-only endpoint — path is account-specific and unverified. Real
  // fetch attempted with the key; honest empty otherwise.
  const data = await fetchJson(
    'https://api.agoop.net/v1/flow-population?area=japan',
    { timeoutMs: 20000, headers: { Authorization: `Bearer ${key}` } },
  ).catch(() => null);

  const rows = Array.isArray(data?.features) ? data.features
    : Array.isArray(data?.data) ? data.data : null;
  if (!rows || rows.length === 0) return result([], 'agoop-flow_unavailable');

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
          flow_population: r.flow_population ?? r.value ?? null,
          datetime: r.datetime ?? r.timestamp ?? null,
          source: 'agoop_flow_api',
        },
      };
    });
  return result(features, 'agoop-flow');
}
