/**
 * e-Stat Education Stats collector (estat-education).
 *
 * Live: e-Stat REST 3.0 getStatsData with a school-basic-survey table.
 * Auth: free app id via `ESTAT_APP_ID` (shared with eStatCrime.js). Honest
 * empty intel envelope without the app id or on failure — no seed.
 *
 * statsDataId 0003147040 = 学校基本調査 (School Basic Survey) public table.
 * Override via ESTAT_EDUCATION_STATS_DATA_ID.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

const SOURCE_ID = 'estat-education';
const API_BASE = 'https://api.e-stat.go.jp/rest/3.0/app/json/getStatsData';
const DEFAULT_STATS_DATA_ID = '0003147040';

function statsDataId() {
  return process.env.ESTAT_EDUCATION_STATS_DATA_ID || DEFAULT_STATS_DATA_ID;
}

export default async function collectEstatEducation() {
  const appId = envFor('ESTAT_APP_ID');
  if (!appId) {
    return intelEnvelope({
      sourceId: SOURCE_ID,
      items: [],
      live: false,
      extraMeta: { source: 'estat_education_no_key', needs_key: 'ESTAT_APP_ID' },
      description: 'e-Stat education / school-basic-survey statistics (requires ESTAT_APP_ID)',
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
      extraMeta: { source: 'estat_education_unavailable' },
      description: 'e-Stat education / school-basic-survey statistics',
    });
  }

  const items = values.slice(0, 2000).map((v, i) => {
    const area = v['@area'] ?? null;
    const cat = v['@cat01'] ?? null;
    const time = v['@time'] ?? null;
    const val = v.$ ?? null;
    return {
      uid: intelUid(SOURCE_ID, `${area}-${cat}-${time}` || i),
      title: `Education stat — area ${area ?? '?'} (${time ?? '?'})`,
      summary: `value=${val} cat=${cat ?? '-'}`,
      body: `e-Stat school-basic-survey table ${statsDataId()}: area=${area}, category=${cat}, time=${time}, value=${val}`,
      link: 'https://www.e-stat.go.jp/',
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['statistics', 'population', 'education'],
      properties: { area, cat01: cat, time, value: val, statsDataId: statsDataId() },
    };
  });

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: true,
    extraMeta: { source: 'estat_education_live', statsDataId: statsDataId() },
    description: 'e-Stat education / school-basic-survey statistics (学校基本調査)',
  });
}
