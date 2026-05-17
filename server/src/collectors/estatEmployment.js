/**
 * e-Stat Employment collector (estat-employment).
 *
 * Live: e-Stat REST 3.0 getStatsData with a labour-force / employment table.
 * Auth: free app id via `ESTAT_APP_ID` (shared with eStatCrime.js). Honest
 * empty intel envelope without the app id or on upstream failure — no seed.
 *
 * statsDataId 0003109741 = 労働力調査 (Labour Force Survey) public table.
 * Override via ESTAT_EMPLOYMENT_STATS_DATA_ID.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

const SOURCE_ID = 'estat-employment';
const API_BASE = 'https://api.e-stat.go.jp/rest/3.0/app/json/getStatsData';
const DEFAULT_STATS_DATA_ID = '0003109741';

function statsDataId() {
  return process.env.ESTAT_EMPLOYMENT_STATS_DATA_ID || DEFAULT_STATS_DATA_ID;
}

export default async function collectEstatEmployment() {
  const appId = envFor('ESTAT_APP_ID');
  if (!appId) {
    return intelEnvelope({
      sourceId: SOURCE_ID,
      items: [],
      live: false,
      extraMeta: { source: 'estat_employment_no_key', needs_key: 'ESTAT_APP_ID' },
      description: 'e-Stat employment / labour-force statistics (requires ESTAT_APP_ID)',
    });
  }

  const url = `${API_BASE}?appId=${encodeURIComponent(appId)}`
    + `&statsDataId=${encodeURIComponent(statsDataId())}&limit=2000`;
  const data = await fetchJson(url, { timeoutMs: 20000 });
  const values = data?.GET_STATS_DATA?.STATISTICAL_DATA?.DATA_INF?.VALUE;
  if (!Array.isArray(values) || values.length === 0) {
    return intelEnvelope({
      sourceId: SOURCE_ID,
      items: [],
      live: false,
      extraMeta: { source: 'estat_employment_unavailable' },
      description: 'e-Stat employment / labour-force statistics',
    });
  }

  const items = values.slice(0, 2000).map((v, i) => {
    const area = v['@area'] ?? null;
    const cat = v['@cat01'] ?? null;
    const time = v['@time'] ?? null;
    const val = v.$ ?? null;
    return {
      uid: intelUid(SOURCE_ID, `${area}-${cat}-${time}` || i),
      title: `Employment stat — area ${area ?? '?'} (${time ?? '?'})`,
      summary: `value=${val} cat=${cat ?? '-'}`,
      body: `e-Stat labour-force table ${statsDataId()}: area=${area}, category=${cat}, time=${time}, value=${val}`,
      link: 'https://www.e-stat.go.jp/',
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['statistics', 'economy', 'employment'],
      properties: { area, cat01: cat, time, value: val, statsDataId: statsDataId() },
    };
  });

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: true,
    extraMeta: { source: 'estat_employment_live', statsDataId: statsDataId() },
    description: 'e-Stat employment / labour-force statistics (労働力調査)',
  });
}
