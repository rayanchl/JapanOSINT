/**
 * e-Stat Household Data collector (estat-household).
 *
 * Live: e-Stat REST 3.0 getStatsData with a household-composition table.
 * Auth: free app id via `ESTAT_APP_ID` (same credential the existing
 * eStatCrime.js / estat-census collectors use). Without the app id, or on
 * any upstream failure, returns an honest empty intel envelope — never a
 * fabricated/seed row.
 *
 * Non-spatial → emits `kind:'intel'` rows, one per statistical VALUE.
 * statsDataId: 0003448237 is a public household-composition table on
 * e-Stat (国勢調査 世帯の家族類型別世帯数). Override via
 * ESTAT_HOUSEHOLD_STATS_DATA_ID if a more specific table is preferred.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

const SOURCE_ID = 'estat-household';
const API_BASE = 'https://api.e-stat.go.jp/rest/3.0/app/json/getStatsData';
// Public household-composition table; override-able if a better id is found.
const DEFAULT_STATS_DATA_ID = '0003448237';

function statsDataId() {
  return process.env.ESTAT_HOUSEHOLD_STATS_DATA_ID || DEFAULT_STATS_DATA_ID;
}

export default async function collectEstatHousehold() {
  const appId = envFor('ESTAT_APP_ID');
  if (!appId) {
    return intelEnvelope({
      sourceId: SOURCE_ID,
      items: [],
      live: false,
      extraMeta: { source: 'estat_household_no_key', needs_key: 'ESTAT_APP_ID' },
      description: 'e-Stat household composition statistics (requires ESTAT_APP_ID)',
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
      extraMeta: { source: 'estat_household_unavailable' },
      description: 'e-Stat household composition statistics',
    });
  }

  const items = values.slice(0, 2000).map((v, i) => {
    const area = v['@area'] ?? null;
    const cat = v['@cat01'] ?? null;
    const time = v['@time'] ?? null;
    const val = v.$ ?? null;
    return {
      uid: intelUid(SOURCE_ID, `${area}-${cat}-${time}` || i),
      title: `Household stat — area ${area ?? '?'} (${time ?? '?'})`,
      summary: `value=${val} cat=${cat ?? '-'}`,
      body: `e-Stat household composition table ${statsDataId()}: area=${area}, category=${cat}, time=${time}, value=${val}`,
      link: 'https://www.e-stat.go.jp/',
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['statistics', 'population', 'household'],
      properties: { area, cat01: cat, time, value: val, statsDataId: statsDataId() },
    };
  });

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: true,
    extraMeta: { source: 'estat_household_live', statsDataId: statsDataId() },
    description: 'e-Stat household composition statistics (国勢調査 世帯)',
  });
}
