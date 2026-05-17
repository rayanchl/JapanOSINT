/**
 * NTT Docomo Insight Data collector (docomo-insight).
 *
 * Docomo Insight Data (人口統計 / 流動人口インサイト) is a paid,
 * contract-only analytics product with no public API. No key-free fallback
 * exists, so without a contract key we return an honest empty
 * FeatureCollection (never fabricated points). The missing-key state surfaces
 * via the apiCredentials map ("needs API key" badge in the Sources tab).
 *
 * When DOCOMO_INSIGHT_API_KEY is present the documented contract endpoint is
 * attempted; its exact path is account-specific and unverified, so any
 * failure yields honest empty ('docomo-insight_unavailable').
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
      description: 'NTT Docomo Insight Data flow/visitor analytics over Japan (requires DOCOMO_INSIGHT_API_KEY — paid contract API)',
    },
  };
}

export default async function collectDocomoInsight() {
  const key = envFor('DOCOMO_INSIGHT_API_KEY');
  if (!key) return result([], 'docomo-insight_no_key');

  // Contract-only endpoint — path is account-specific and unverified. Real
  // fetch attempted with the key; honest empty otherwise.
  const data = await fetchJson(
    'https://api.docomo-datasquare.co.jp/v1/insight/flow?area=japan',
    { timeoutMs: 20000, headers: { Authorization: `Bearer ${key}` } },
  ).catch(() => null);

  const rows = Array.isArray(data?.features) ? data.features
    : Array.isArray(data?.data) ? data.data : null;
  if (!rows || rows.length === 0) return result([], 'docomo-insight_unavailable');

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
          area_code: r.area_code ?? r.areaCode ?? null,
          visitors: r.visitors ?? r.value ?? null,
          datetime: r.datetime ?? r.timestamp ?? null,
          source: 'docomo_insight_api',
        },
      };
    });
  return result(features, 'docomo-insight');
}
