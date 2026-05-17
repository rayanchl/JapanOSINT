/**
 * RIPE Atlas probes in Japan (atlas-jp).
 *
 * Live, key-free: RIPE Atlas public API. We list connected probes whose
 * country_code is JP. Probes carry latitude/longitude, so this is emitted as
 * a FeatureCollection. On failure we return an honest empty result — never
 * fabricated points.
 *
 * Endpoint (verified, key-free, paginated):
 *   https://atlas.ripe.net/api/v2/probes/?country_code=JP&status=1
 */

import { fetchJson } from './_liveHelpers.js';

const BASE = 'https://atlas.ripe.net/api/v2/probes/';
const MAX_PAGES = 12; // 100/page → up to ~1200 probes; JP has a few hundred

function result(features, source) {
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'RIPE Atlas measurement probes located in Japan (key-free public API)',
    },
  };
}

export default async function collectAtlasJp() {
  const features = [];
  let url = `${BASE}?country_code=JP&status=1&page_size=100&fields=id,address_v4,asn_v4,asn_v6,prefix_v4,latitude,longitude,status_name,is_anchor,first_connected`;
  let pages = 0;
  let gotAny = false;

  while (url && pages < MAX_PAGES) {
    const data = await fetchJson(url, { timeoutMs: 15000 });
    if (!data || !Array.isArray(data.results)) break;
    gotAny = true;
    for (const p of data.results) {
      if (p.latitude == null || p.longitude == null) continue;
      if (+p.latitude === 0 && +p.longitude === 0) continue;
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [+p.longitude, +p.latitude] },
        properties: {
          probe_id: p.id,
          asn_v4: p.asn_v4 ?? null,
          asn_v6: p.asn_v6 ?? null,
          prefix_v4: p.prefix_v4 ?? null,
          address_v4: p.address_v4 ?? null,
          status: p.status_name ?? null,
          is_anchor: !!p.is_anchor,
          first_connected: p.first_connected
            ? new Date(p.first_connected * 1000).toISOString()
            : null,
          link: `https://atlas.ripe.net/probes/${p.id}/`,
          source: 'ripe_atlas_api',
        },
      });
    }
    url = data.next || null;
    pages++;
  }

  if (!gotAny) return result([], 'atlas_jp_unavailable');
  return result(features, 'atlas_jp_api');
}
