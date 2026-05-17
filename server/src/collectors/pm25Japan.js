/**
 * PM2.5 Japan collector (pm25-japan).
 *
 * The MOE Soramame / AEROS air-quality network is the authoritative source and
 * is already mirrored by the dedicated soramame/AEROS collectors. The public
 * soramame.env.go.jp site is a JavaScript SPA — the historical key-free CSV
 * paths (data/sokutei/NoudoFile/...) now return the SPA HTML shell, not data,
 * so there is no usable key-free PM2.5 JSON/CSV endpoint for this source ID.
 *
 * Per the no-fabrication rule this returns an honest EMPTY FeatureCollection
 * (PM2.5 belongs to the existing soramame source). We still do a best-effort
 * reachability probe so the source surfaces an honest live/unavailable state.
 *
 * NOTE: a key-free Soramame PM2.5 JSON endpoint is UNVERIFIED / believed
 * absent; AEROS API access requires registration (no public key here).
 */

import { fetchHead } from './_liveHelpers.js';

const PROBE = 'https://soramame.env.go.jp/';

export default async function collectPm25Japan() {
  const reachable = await fetchHead(PROBE, { timeoutMs: 8000 }).catch(() => false);
  return {
    type: 'FeatureCollection',
    features: [],
    _meta: {
      source: reachable ? 'pm25_japan_no_public_endpoint' : 'pm25_japan_unavailable',
      fetchedAt: new Date().toISOString(),
      recordCount: 0,
      live: false,
      description: 'PM2.5 Japan — no key-free machine-readable endpoint (covered by soramame/AEROS); honest empty',
    },
  };
}
