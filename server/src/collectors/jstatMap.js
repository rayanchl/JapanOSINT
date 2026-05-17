/**
 * jSTAT MAP grid-square statistics collector (jstat-map).
 *
 * jSTAT MAP (https://jstatmap.e-stat.go.jp/) is e-Stat's grid-square
 * statistical-mapping front end. Its programmatic data is the e-Stat
 * statistics API (getStatsData) keyed by `ESTAT_APP_ID` — there is no
 * documented key-free bulk grid endpoint. We fetch a 地域メッシュ
 * (regional mesh) population table via the e-Stat API when the app id is
 * present, otherwise return an honest empty intel envelope (never seed).
 *
 * statsDataId 0003445078 = e-Stat 1km mesh population (the same public
 * mesh table used by estatPopulation.js). Override via
 * JSTAT_MAP_STATS_DATA_ID.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchJson } from './_liveHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

const SOURCE_ID = 'jstat-map';
const API_BASE = 'https://api.e-stat.go.jp/rest/3.0/app/json/getStatsData';
const DEFAULT_STATS_DATA_ID = '0003445078';

function statsDataId() {
  return process.env.JSTAT_MAP_STATS_DATA_ID || DEFAULT_STATS_DATA_ID;
}

export default async function collectJstatMap() {
  const appId = envFor('ESTAT_APP_ID');
  if (!appId) {
    return intelEnvelope({
      sourceId: SOURCE_ID,
      items: [],
      live: false,
      extraMeta: { source: 'jstat_map_no_key', needs_key: 'ESTAT_APP_ID' },
      description: 'jSTAT MAP grid-square statistics via e-Stat API (requires ESTAT_APP_ID)',
    });
  }

  const url = `${API_BASE}?appId=${encodeURIComponent(appId)}`
    + `&statsDataId=${encodeURIComponent(statsDataId())}&limit=3000`;
  const data = await fetchJson(url, { timeoutMs: 20000 });
  const values = data?.GET_STATS_DATA?.STATISTICAL_DATA?.DATA_INF?.VALUE;
  if (!Array.isArray(values) || values.length === 0) {
    return intelEnvelope({
      sourceId: SOURCE_ID,
      items: [],
      live: false,
      extraMeta: { source: 'jstat_map_unavailable' },
      description: 'jSTAT MAP grid-square statistics via e-Stat API',
    });
  }

  const items = values.slice(0, 3000).map((v, i) => {
    const mesh = v['@area'] ?? null;
    const cat = v['@cat01'] ?? null;
    const time = v['@time'] ?? null;
    const val = v.$ ?? null;
    return {
      uid: intelUid(SOURCE_ID, `${mesh}-${cat}-${time}` || i),
      title: `jSTAT mesh ${mesh ?? '?'} (${time ?? '?'})`,
      summary: `value=${val} cat=${cat ?? '-'}`,
      body: `jSTAT MAP / e-Stat mesh table ${statsDataId()}: mesh=${mesh}, category=${cat}, time=${time}, value=${val}`,
      link: 'https://jstatmap.e-stat.go.jp/',
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['statistics', 'population', 'mesh'],
      properties: { mesh_code: mesh, cat01: cat, time, value: val, statsDataId: statsDataId() },
    };
  });

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: true,
    extraMeta: { source: 'jstat_map_live', statsDataId: statsDataId() },
    description: 'jSTAT MAP grid-square (mesh) statistics via e-Stat API',
  });
}
