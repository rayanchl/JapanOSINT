/**
 * Mercari — Japanese consumer-to-consumer marketplace.
 *
 * No official public API. The site uses internal XHR endpoints. Rather
 * than scrape the full marketplace, this collector tracks the handful
 * of "trending category" slots exposed on the landing page.
 *
 * LEGAL NOTE: Mercari's ToS references external prohibited-acts
 * guidelines that likely disallow automated extraction at scale. Use
 * at low cadence (>= daily) and keep it to trending-only, not bulk
 * listing scrape.
 *
 * Non-geospatial. Returns zero-geometry features.
 */

const BASE = 'https://jp.mercari.com';
const TIMEOUT_MS = 12000;

// Stable-ish internal endpoint returning trending keywords. If Mercari
// refactors, this collector degrades silently to an empty feed.
const TRENDING_PATH = '/v1/web/suggest/trends';

export default async function collectMercariTrending() {
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(`${BASE}${TRENDING_PATH}`, {
      signal: controller.signal,
      headers: {
        'user-agent': 'JapanOSINT/1.0',
        'accept': 'application/json',
      },
    });
    clearTimeout(timer);
    if (!res.ok) return empty(`mercari_http_${res.status}`);
    const data = await res.json();
    const items = Array.isArray(data?.trends) ? data.trends : Array.isArray(data) ? data : [];
    const features = items.slice(0, 50).map((it, i) => ({
      type: 'Feature',
      geometry: null,
      properties: {
        id: `MERCARI_${i + 1}`,
        keyword: typeof it === 'string' ? it : (it.keyword || it.name || null),
        rank: i + 1,
        source: 'mercari_trending',
      },
    })).filter((f) => f.properties.keyword);

    return {
      type: 'FeatureCollection',
      features,
      _meta: {
        source: features.length ? 'mercari_live' : 'mercari_empty',
        fetchedAt: new Date().toISOString(),
        recordCount: features.length,
        description: 'Mercari trending search keywords (Japan consumer demand signal)',
      },
      metadata: {},
    };
  } catch (err) {
    console.warn('[mercariTrending] fetch failed:', err?.message);
    return empty('mercari_error');
  }
}

function empty(source) {
  return {
    type: 'FeatureCollection',
    features: [],
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: 0,
      description: 'Mercari trending (unavailable)',
    },
    metadata: {},
  };
}
