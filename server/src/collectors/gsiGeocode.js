/**
 * GSI address search (geocoder) — probe collector.
 * https://msearch.gsi.go.jp/address-search/AddressSearch?q=...
 *
 * Uses the shared gsiAddressSearch helper; falls back to a hardcoded seed
 * feature if the API is unreachable.
 */

import { gsiAddressSearch } from '../utils/gsiAddressSearch.js';

const PROBE_QUERY = '東京駅';

export default async function collectGsiGeocode() {
  const hit = await gsiAddressSearch(PROBE_QUERY);
  let features;
  let source;
  if (hit) {
    features = [{
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [hit.lon, hit.lat] },
      properties: { title: hit.title, source: 'gsi_geocode' },
    }];
    source = 'live';
  } else {
    features = [{
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [139.767, 35.681] },
      properties: { title: '東京駅 (seed)', source: 'gsi_seed' },
    }];
    source = 'seed';
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'GSI address-search geocoder probe',
    },
    metadata: {},
  };
}
