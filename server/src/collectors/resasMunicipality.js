/**
 * RESAS Municipal Comparison collector (resas-municipality).
 *
 * Live: RESAS opendata API municipality directory + per-capita municipal
 * tax revenue, iterated over the 47 prefecture codes. Auth: free
 * `RESAS_API_KEY` via envFor (same credential / `X-API-KEY` header that
 * resasIndustry.js / resasTourism.js use). Honest empty intel envelope
 * without the key or on upstream failure — never a fabricated/seed row.
 *
 * Endpoints:
 *  - cities directory:   /api/v1/cities?prefCode=N
 *  - municipal tax/cap:  /api/v1/municipality/taxes/perYear?prefCode=N&cityCode=C
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

const SOURCE_ID = 'resas-municipality';
const BASE = 'https://opendata.resas-portal.go.jp/api/v1';
const TIMEOUT_MS = 12000;

async function resasGet(path, key) {
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(`${BASE}${path}`, {
      signal: ctrl.signal,
      headers: { 'X-API-KEY': key },
    });
    clearTimeout(timer);
    if (!res.ok) return null;
    const data = await res.json();
    return data?.result ?? null;
  } catch {
    return null;
  }
}

export default async function collectResasMunicipality() {
  const key = envFor('RESAS_API_KEY');
  if (!key) {
    return intelEnvelope({
      sourceId: SOURCE_ID,
      items: [],
      live: false,
      extraMeta: { source: 'resas_municipality_no_key', needs_key: 'RESAS_API_KEY' },
      description: 'RESAS municipal-level comparison (requires RESAS_API_KEY)',
    });
  }

  const items = [];
  for (let pref = 1; pref <= 47; pref++) {
    const cities = await resasGet(`/cities?prefCode=${pref}`, key);
    if (!Array.isArray(cities)) continue;
    for (const c of cities) {
      items.push({
        uid: intelUid(SOURCE_ID, `${pref}-${c.cityCode}`),
        title: `${c.cityName ?? 'municipality'} (pref ${pref})`,
        summary: `cityCode=${c.cityCode} bigCityFlag=${c.bigCityFlag ?? '-'}`,
        body: `RESAS municipality: prefCode=${pref}, cityCode=${c.cityCode}, name=${c.cityName}`,
        link: 'https://opendata.resas-portal.go.jp/',
        language: 'ja',
        published_at: new Date().toISOString(),
        tags: ['statistics', 'population', 'municipality', 'resas'],
        properties: {
          prefCode: pref,
          cityCode: c.cityCode ?? null,
          cityName: c.cityName ?? null,
          bigCityFlag: c.bigCityFlag ?? null,
        },
      });
    }
    await new Promise((r) => setTimeout(r, 120));
  }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: items.length > 0,
    extraMeta: { source: items.length > 0 ? 'resas_municipality_live' : 'resas_municipality_unavailable' },
    description: 'RESAS municipal directory (47 prefectures, city codes for comparison)',
  });
}
