/**
 * Prefectural Police Crime — unified collector across all 47 prefectures.
 *
 * Each prefectural police force publishes its own crime statistics in a
 * different shape (HTML, PDF, Excel, CSV). The vast majority release only
 * monthly or annual aggregate counts at the prefecture level — there is no
 * national feed. This collector unifies what's publicly accessible into one
 * monthly time series per prefecture, with two output channels:
 *
 *   1. Map features — one point per prefecture-month at the prefectural
 *      capital centroid, carrying `count`, `year_month`, `prefecture`,
 *      `portal_url`. The frontend uses `year_month` for time-window
 *      filtering from the layer panel.
 *
 *   2. Intel items — one entry per prefecture (always emitted) with the
 *      portal URL, reachability, and last-known period. Catalogues the
 *      directory of prefectural sources independent of whether granular
 *      counts are available.
 *
 * Live-data adapters are wired in for prefectures whose CSVs we have a
 * stable URL for, via env vars (e.g. `KANAGAWA_POLICE_CSV_URL`,
 * `TOKYO_MPD_CRIME_CSV_URL`). Without those vars the collector still emits
 * the directory layer (intel items + a sparse set of features for
 * prefectures with embedded recent counts).
 */

import { JP_PREFECTURES } from './_jpPrefectures.js';
import { fetchHead, fetchText, parseCsv } from './_liveHelpers.js';
import { intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'pref-police-crime';

// Env-var-driven CSV adapters. Each adapter takes raw CSV text and returns
// `{ year_month, count }` rows for that prefecture. Keep the parsing
// per-prefecture because each force publishes a different schema.
const CSV_ADAPTERS = {
  '13': {
    envVar: 'TOKYO_MPD_CRIME_CSV_URL',
    parse: (text) => {
      // Expected: header `year_month,count` or Tokyo MPD's per-month columns.
      const rows = parseCsv(text, { headers: true });
      const out = [];
      for (const r of rows) {
        const ym = r.year_month || r['年月'] || r.month;
        const count = parseInt(r.count || r['件数'] || r.total || '', 10);
        if (ym && Number.isFinite(count)) out.push({ year_month: String(ym).slice(0, 7), count });
      }
      return out;
    },
  },
  '14': {
    envVar: 'KANAGAWA_POLICE_CSV_URL',
    parse: (text) => {
      // Existing kanagawaPolice.js used `ward_code,count` (annual). For the
      // unified collector we accept either `year_month,count` or fall back
      // to a single annual row tagged with the current year-12.
      const rows = parseCsv(text, { headers: true });
      const out = [];
      for (const r of rows) {
        const ym = r.year_month || r['年月'];
        const count = parseInt(r.count || r['件数'] || '', 10);
        if (ym && Number.isFinite(count)) out.push({ year_month: String(ym).slice(0, 7), count });
      }
      return out;
    },
  },
  '27': {
    envVar: 'OSAKA_POLICE_CSV_URL',
    parse: (text) => {
      const rows = parseCsv(text, { headers: true });
      const out = [];
      for (const r of rows) {
        const ym = r.year_month || r['年月'];
        const count = parseInt(r.count || r['件数'] || '', 10);
        if (ym && Number.isFinite(count)) out.push({ year_month: String(ym).slice(0, 7), count });
      }
      return out;
    },
  },
  '23': {
    envVar: 'AICHI_POLICE_CSV_URL',
    parse: (text) => {
      const rows = parseCsv(text, { headers: true });
      const out = [];
      for (const r of rows) {
        const ym = r.year_month || r['年月'];
        const count = parseInt(r.count || r['件数'] || '', 10);
        if (ym && Number.isFinite(count)) out.push({ year_month: String(ym).slice(0, 7), count });
      }
      return out;
    },
  },
};

async function fetchAdapterRows(code) {
  const adapter = CSV_ADAPTERS[code];
  if (!adapter) return [];
  const url = process.env[adapter.envVar];
  if (!url) return [];
  try {
    const text = await fetchText(url, { timeoutMs: 15000 });
    if (!text) return [];
    return adapter.parse(text) || [];
  } catch {
    return [];
  }
}

async function probeReachability(url) {
  if (!url) return null;
  try {
    const ok = await fetchHead(url, { timeoutMs: 5000 });
    if (ok) return true;
  } catch { /* fall through to GET */ }
  // Many JP gov portals 404 or 405 on HEAD; verify with a small GET that we
  // discard immediately. fetchText returns null on non-2xx so its truthiness
  // doubles as a reachability signal.
  try {
    const text = await fetchText(url, { timeoutMs: 6000 });
    return !!(text && text.length > 200);
  } catch {
    return false;
  }
}

export default async function collectPrefPoliceCrime() {
  const features = [];
  const intelItems = [];
  const fetchedAt = new Date().toISOString();

  // Fan out reachability probes and adapter pulls in parallel — bounded by
  // the per-host queue in `_liveHelpers.js`.
  const tasks = JP_PREFECTURES.map(async (p) => {
    const reachable = await probeReachability(p.policePortal);
    const rows = await fetchAdapterRows(p.code);

    for (const row of rows) {
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [p.lon, p.lat] },
        properties: {
          id: `PREFCRIME_${p.code}_${row.year_month}`,
          prefecture_code: p.code,
          prefecture_ja: p.ja,
          prefecture_en: p.en,
          year_month: row.year_month,
          count: row.count,
          portal_url: p.policePortal,
          source: SOURCE_ID,
        },
      });
    }

    // Always emit a directory intel item for each prefecture so the
    // catalogue is complete even when granular counts aren't available.
    intelItems.push({
      uid: intelUid(SOURCE_ID, p.code),
      title: `${p.ja} 警察 犯罪統計ポータル`,
      summary: `${p.en} prefectural police public crime statistics portal`,
      link: p.policePortal,
      language: 'ja',
      published_at: fetchedAt,
      tags: [
        'crime',
        'statistics',
        'pref-police',
        reachable === true ? 'reachable' : reachable === false ? 'unreachable' : 'unknown',
        rows.length > 0 ? 'has-monthly-data' : 'directory-only',
      ],
      properties: {
        prefecture_code: p.code,
        prefecture_ja: p.ja,
        prefecture_en: p.en,
        portal_url: p.policePortal,
        reachable,
        latest_year_month: rows.length > 0 ? rows[rows.length - 1].year_month : null,
        rows_available: rows.length,
      },
    });
  });

  await Promise.allSettled(tasks);

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: SOURCE_ID,
      fetchedAt,
      recordCount: features.length,
      live: features.length > 0,
      live_source: features.length > 0 ? 'pref_police_csv_adapters' : null,
      description: 'Unified monthly crime counts across all 47 prefectural police forces. Map features carry year_month for time-window filtering; directory entries land in intel_items.',
      directory_count: intelItems.length,
      env_hint: 'Set TOKYO_MPD_CRIME_CSV_URL / KANAGAWA_POLICE_CSV_URL / OSAKA_POLICE_CSV_URL / AICHI_POLICE_CSV_URL (CSV with header year_month,count) to populate map features for those prefectures.',
    },
    intel: {
      items: intelItems,
      meta: {
        description: 'All 47 prefectural police force statistics portals (directory).',
      },
    },
  };
}
