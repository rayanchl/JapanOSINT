/**
 * e-Stat Crime Statistics — government statistics portal API.
 *
 * Source: https://www.e-stat.go.jp/ — the Japanese government's
 * machine-readable statistics portal. Crime statistics live under the
 * 犯罪統計 (00130001) statsCode published by the National Police Agency,
 * with per-prefecture annual breakdowns.
 *
 * This is the canonical source for cross-prefecture crime numbers — it
 * unifies what each prefectural police force submits to the NPA into one
 * machine-readable feed. Unlike scraping per-prefecture HTML portals (the
 * `prefPoliceCrime` directory layer), e-Stat publishes the actual numbers
 * with consistent schema.
 *
 * Authentication: the API requires a free app ID. Set `ESTAT_APP_ID` in
 * the environment to enable live data; without it, the collector emits
 * one intel item with the registration link and no map features.
 *
 * Default `statsDataId`: configurable via `ESTAT_CRIME_STATS_DATA_ID`. The
 * NPA's per-prefecture crime totals table id rotates roughly annually as
 * new years land — the value defaults to a known recent table but can be
 * overridden in the env without redeploying.
 */

import { fetchJson } from './_liveHelpers.js';
import { JP_PREFECTURES, resolvePrefecture } from './_jpPrefectures.js';
import { intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'estat-crime';
const PORTAL_URL = 'https://www.e-stat.go.jp/stat-search/database?toukei=00130001';
const REGISTER_URL = 'https://www.e-stat.go.jp/api/';
const API_BASE = 'https://api.e-stat.go.jp/rest/3.0/app/json/getStatsData';

// Default table — `0003433388` is the NPA's prefecture × year crime totals
// table (刑法犯 認知件数・検挙件数 都道府県別). Override via env var as
// e-Stat rotates IDs when new years are added.
const DEFAULT_STATS_DATA_ID = '0003433388';

function appId() {
  return process.env.ESTAT_APP_ID || null;
}

function statsDataId() {
  return process.env.ESTAT_CRIME_STATS_DATA_ID || DEFAULT_STATS_DATA_ID;
}

/**
 * Walk the e-Stat response shape:
 *   GET_STATS_DATA.STATISTICAL_DATA.DATA_INF.VALUE[] — array of rows
 *   GET_STATS_DATA.STATISTICAL_DATA.CLASS_INF.CLASS_OBJ[] — meta describing
 *     each class dimension (area, time, indicator). We use this to map
 *     `@area` codes to prefecture names and `@time` codes to years.
 */
function buildClassMaps(classObjs) {
  const maps = {};
  for (const obj of classObjs || []) {
    const id = obj['@id'];
    const items = Array.isArray(obj.CLASS) ? obj.CLASS : (obj.CLASS ? [obj.CLASS] : []);
    const m = {};
    for (const c of items) m[c['@code']] = c['@name'];
    maps[id] = m;
  }
  return maps;
}

function parseValues(json) {
  const stat = json?.GET_STATS_DATA?.STATISTICAL_DATA;
  if (!stat) return null;
  const classObjs = stat.CLASS_INF?.CLASS_OBJ;
  const valueArr = stat.DATA_INF?.VALUE;
  if (!Array.isArray(valueArr)) return null;
  const classMaps = buildClassMaps(Array.isArray(classObjs) ? classObjs : [classObjs]);

  const records = [];
  for (const v of valueArr) {
    const areaCode = v['@area'] || v['@cat01'] || null;
    const timeCode = v['@time'] || null;
    const indicatorCode = v['@cat01'] || v['@tab'] || null;
    const value = parseInt(String(v['$'] || '').replace(/,/g, ''), 10);
    if (!Number.isFinite(value)) continue;
    const areaName = areaCode ? classMaps.area?.[areaCode] || classMaps.cat01?.[areaCode] : null;
    const timeName = timeCode ? classMaps.time?.[timeCode] : null;
    const indicatorName = indicatorCode
      ? classMaps.cat01?.[indicatorCode] || classMaps.tab?.[indicatorCode]
      : null;
    records.push({ areaCode, areaName, timeCode, timeName, indicatorCode, indicatorName, value });
  }
  return records;
}

function timeToYearMonth(label) {
  if (!label) return null;
  // e-Stat year labels: '2023年' or '2023', month labels: '2023年12月'.
  const ym = label.match(/(\d{4})\s*年\s*(\d{1,2})\s*月/);
  if (ym) return `${ym[1]}-${ym[2].padStart(2, '0')}`;
  const y = label.match(/(\d{4})/);
  if (y) return `${y[1]}-12`;
  return null;
}

export default async function collectEStatCrime() {
  const fetchedAt = new Date().toISOString();
  const id = appId();

  // Always emit a directory intel item — even when not configured, so the
  // catalogue points at the registration page.
  const baseIntel = {
    uid: intelUid(SOURCE_ID, 'portal'),
    title: 'e-Stat 犯罪統計 (Crime Statistics)',
    summary: 'Government Statistics portal — per-prefecture annual crime totals from NPA. Requires free ESTAT_APP_ID.',
    link: PORTAL_URL,
    language: 'ja',
    published_at: fetchedAt,
    tags: ['crime', 'statistics', 'estat', 'national'],
    properties: {
      portal_url: PORTAL_URL,
      register_url: REGISTER_URL,
      stats_code: '00130001',
      configured: !!id,
      env_hint: 'Set ESTAT_APP_ID and (optionally) ESTAT_CRIME_STATS_DATA_ID to populate map features.',
    },
  };

  if (!id) {
    return {
      type: 'FeatureCollection',
      features: [],
      _meta: {
        source: SOURCE_ID,
        fetchedAt,
        recordCount: 0,
        live: false,
        live_source: null,
        upstream_url: PORTAL_URL,
        env_hint: 'Set ESTAT_APP_ID to a registered e-Stat application ID to enable live data.',
        description: 'e-Stat crime statistics portal directory (no app ID configured).',
      },
      intel: { items: [baseIntel] },
    };
  }

  const url = `${API_BASE}?appId=${encodeURIComponent(id)}&statsDataId=${encodeURIComponent(statsDataId())}&metaGetFlg=Y&limit=100000`;
  let json = null;
  try {
    json = await fetchJson(url, { timeoutMs: 20000, retries: 1 });
  } catch { /* fall through */ }

  const records = json ? parseValues(json) : null;
  if (!records || records.length === 0) {
    return {
      type: 'FeatureCollection',
      features: [],
      _meta: {
        source: SOURCE_ID,
        fetchedAt,
        recordCount: 0,
        live: false,
        live_source: 'estat_api_unreachable',
        upstream_url: PORTAL_URL,
        description: 'e-Stat API returned no parseable rows. Check ESTAT_APP_ID and ESTAT_CRIME_STATS_DATA_ID.',
      },
      intel: { items: [baseIntel] },
    };
  }

  // Pivot per (prefecture, year): sum across indicator codes — many tables
  // include both 認知件数 and 検挙件数 as separate cat01 codes; we keep
  // both as named fields on the same feature.
  const byKey = new Map();
  for (const r of records) {
    const pref = resolvePrefecture(r.areaName || '');
    if (!pref) continue;
    const ym = timeToYearMonth(r.timeName);
    if (!ym) continue;
    const key = `${pref.code}|${ym}`;
    let bucket = byKey.get(key);
    if (!bucket) {
      bucket = { pref, ym, indicators: {} };
      byKey.set(key, bucket);
    }
    if (r.indicatorName) bucket.indicators[r.indicatorName] = r.value;
  }

  const features = [];
  for (const bucket of byKey.values()) {
    const { pref, ym, indicators } = bucket;
    const recognised = indicators['認知件数'] ?? indicators['総数'] ?? null;
    const cleared = indicators['検挙件数'] ?? null;
    const arrests = indicators['検挙人員'] ?? null;
    features.push({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [pref.lon, pref.lat] },
      properties: {
        id: `ESTAT_${pref.code}_${ym}`,
        prefecture_code: pref.code,
        prefecture_ja: pref.ja,
        prefecture_en: pref.en,
        year_month: ym,
        recognised,
        cleared,
        arrests,
        indicators,
        source: SOURCE_ID,
      },
    });
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: SOURCE_ID,
      fetchedAt,
      recordCount: features.length,
      live: features.length > 0,
      live_source: 'estat_api',
      upstream_url: PORTAL_URL,
      stats_data_id: statsDataId(),
      description: 'e-Stat per-prefecture crime statistics from NPA-reported data. One feature per (prefecture, year) tagged year_month.',
    },
    intel: { items: [baseIntel] },
  };
}
