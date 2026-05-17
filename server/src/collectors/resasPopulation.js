/**
 * RESAS Population by City collector (resas-population).
 *
 * Live: RESAS opendata API population composition per year, iterated over
 * the 47 prefecture codes. Auth: free `RESAS_API_KEY` via envFor (same
 * credential the existing resasIndustry.js / resasTourism.js use, sent as
 * the `X-API-KEY` header). Honest empty intel envelope without the key or
 * on upstream failure — never a fabricated/seed row.
 *
 * Endpoint: https://opendata.resas-portal.go.jp/api/v1/population/composition/perYear
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

const SOURCE_ID = 'resas-population';
const RESAS_URL = 'https://opendata.resas-portal.go.jp/api/v1/population/composition/perYear';
const TIMEOUT_MS = 12000;

async function fetchPref(prefCode, key) {
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(`${RESAS_URL}?prefCode=${prefCode}&cityCode=-`, {
      signal: ctrl.signal,
      headers: { 'X-API-KEY': key },
    });
    clearTimeout(timer);
    if (!res.ok) return null;
    const data = await res.json();
    return data?.result || null;
  } catch {
    return null;
  }
}

export default async function collectResasPopulation() {
  const key = envFor('RESAS_API_KEY');
  if (!key) {
    return intelEnvelope({
      sourceId: SOURCE_ID,
      items: [],
      live: false,
      extraMeta: { source: 'resas_population_no_key', needs_key: 'RESAS_API_KEY' },
      description: 'RESAS population composition by prefecture (requires RESAS_API_KEY)',
    });
  }

  const items = [];
  for (let pref = 1; pref <= 47; pref++) {
    const result = await fetchPref(pref, key);
    if (!result || !Array.isArray(result.data)) continue;
    for (const series of result.data) {
      const label = series.label || 'population';
      const latest = Array.isArray(series.data) && series.data.length
        ? series.data[series.data.length - 1]
        : null;
      items.push({
        uid: intelUid(SOURCE_ID, `${pref}-${label}`),
        title: `RESAS population — pref ${pref} (${label})`,
        summary: latest ? `${label} ${latest.year}: ${latest.value}` : label,
        body: `RESAS population composition perYear, prefCode=${pref}, series=${label}, boundaryYear=${result.boundaryYear ?? '?'}`,
        link: 'https://opendata.resas-portal.go.jp/',
        language: 'ja',
        published_at: new Date().toISOString(),
        tags: ['statistics', 'population', 'resas'],
        properties: {
          prefCode: pref,
          series: label,
          boundaryYear: result.boundaryYear ?? null,
          data: series.data ?? [],
        },
      });
    }
    await new Promise((r) => setTimeout(r, 120));
  }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: items.length > 0,
    extraMeta: { source: items.length > 0 ? 'resas_population_live' : 'resas_population_unavailable' },
    description: 'RESAS population composition by prefecture (0-14 / 15-64 / 65+)',
  });
}
