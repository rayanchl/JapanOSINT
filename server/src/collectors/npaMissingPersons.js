/**
 * NPA Missing Persons (行方不明者) — annual nationwide statistics.
 *
 * Source: National Police Agency annual missing-persons report,
 * `R0{N}yukuefumeisha_csv.csv` (Shift_JIS, multi-table layout). The file is
 * a national-aggregate report with breakdowns by sex / age / cause but no
 * per-prefecture rows, so this collector emits one feature per reporting
 * year pinned at NPA HQ (Tokyo) carrying the year's totals plus key
 * breakdowns. Each feature is tagged with `year_month: 'YYYY-12'` so the
 * frontend time-window slider can scrub across years.
 *
 * The directory entry (link to the latest report + Excel/CSV downloads) is
 * also emitted as an intel item.
 */

import { fetchArrayBuffer, decodeShiftJis, parseCsv } from './_liveHelpers.js';
import { intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'npa-missing-persons';
const NPA_HQ = { lat: 35.6749, lon: 139.7531 }; // 警察庁本庁舎 (霞が関)
const INDEX_URL = 'https://www.npa.go.jp/publications/statistics/safetylife/yukue.html';

// The CSV filename rotates each fiscal year. Try the current and previous
// two Reiwa years; the first one that fetches wins.
function csvUrlsToTry() {
  const reiwaYear = new Date().getFullYear() - 2018; // R6 = 2024 → year 6
  const out = [];
  for (let r = reiwaYear; r >= reiwaYear - 2; r--) {
    out.push(`https://www.npa.go.jp/safetylife/seianki/fumei/R0${r}yukuefumeisha_csv.csv`);
  }
  return out;
}

function reiwaToYear(label) {
  // '令和２年' → 2020, '令和６年' → 2024.
  const m = label.match(/令和\s*([０-９0-9]+|[一二三四五六七八九十])\s*年/);
  if (!m) return null;
  const kanji = '一二三四五六七八九十';
  const digits = '０１２３４５６７８９';
  let n = NaN;
  if (/^[0-9]+$/.test(m[1])) n = parseInt(m[1], 10);
  else if (kanji.includes(m[1])) n = kanji.indexOf(m[1]) + 1;
  else {
    let v = '';
    for (const ch of m[1]) v += digits.includes(ch) ? digits.indexOf(ch) : ch;
    n = parseInt(v, 10);
  }
  if (!Number.isFinite(n)) return null;
  return 2018 + n;
}

function toInt(s) {
  if (s == null) return null;
  const n = parseInt(String(s).replace(/[",\s]/g, ''), 10);
  return Number.isFinite(n) ? n : null;
}

/**
 * Parse the multi-table NPA CSV. Returns `{ years, byYear }` where
 * `byYear[year]` carries `{ total, male, female, dementia, juvenile_under10,
 * teens, twenties }`. Skips sections we don't need.
 */
function extractYearlyTotals(rows) {
  // Find the first row that contains 5+ Reiwa year labels — that's the
  // header row of the sex section. Use it to map column index → year.
  const headers = rows.find((r) => r.filter((c) => /令和.+年/.test(c)).length >= 3);
  if (!headers) return { years: [], byYear: {} };
  const yearCols = {};
  for (let i = 0; i < headers.length; i++) {
    const y = reiwaToYear(headers[i] || '');
    if (y) yearCols[y] = i;
  }
  const years = Object.keys(yearCols).map(Number).sort();
  const byYear = {};
  for (const y of years) byYear[y] = {};

  for (const row of rows) {
    // Some sub-category rows are indented by one empty cell (e.g. "認知症"
    // appears at row[1] under "疾病関係" at row[0]). Pick the first
    // non-empty cell as the label.
    const label = ((row[0] || '').trim() || (row[1] || '').trim() || '');
    if (!label) continue;
    let key = null;
    if (label === '男性') key = 'male';
    else if (label === '女性') key = 'female';
    else if (label === '総数' || label === '合計') key = 'total';
    else if (label === '９歳以下' || label === '9歳以下') key = 'juvenile_under10';
    else if (label === '10歳代') key = 'teens';
    else if (label === '20歳代') key = 'twenties';
    else if (label === '認知症') key = 'dementia';
    if (!key) continue;
    for (const y of years) {
      const v = toInt(row[yearCols[y]]);
      if (v != null && byYear[y][key] == null) byYear[y][key] = v;
    }
  }
  return { years, byYear };
}

async function fetchAndParse() {
  for (const url of csvUrlsToTry()) {
    const buf = await fetchArrayBuffer(url, { timeoutMs: 15000 });
    if (!buf) continue;
    const text = decodeShiftJis(buf);
    const rows = parseCsv(text, { headers: false });
    const parsed = extractYearlyTotals(rows);
    if (parsed.years.length > 0) return { url, ...parsed };
  }
  return null;
}

export default async function collectNpaMissingPersons() {
  const fetchedAt = new Date().toISOString();
  const parsed = await fetchAndParse();

  const features = [];
  const intelItems = [];

  if (parsed) {
    for (const y of parsed.years) {
      const stats = parsed.byYear[y] || {};
      if (stats.total == null) continue;
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [NPA_HQ.lon, NPA_HQ.lat] },
        properties: {
          id: `MISSING_${y}`,
          year_month: `${y}-12`,
          year: y,
          total: stats.total ?? null,
          male: stats.male ?? null,
          female: stats.female ?? null,
          dementia: stats.dementia ?? null,
          juvenile_under10: stats.juvenile_under10 ?? null,
          teens: stats.teens ?? null,
          twenties: stats.twenties ?? null,
          source: SOURCE_ID,
        },
      });
    }

    intelItems.push({
      uid: intelUid(SOURCE_ID, 'index'),
      title: 'NPA Annual Missing-Persons Statistics (行方不明者統計)',
      summary: 'Annual nationwide totals with breakdowns by sex, age group, and cause (incl. dementia).',
      link: INDEX_URL,
      language: 'ja',
      published_at: fetchedAt,
      tags: ['safety', 'statistics', 'missing-persons', 'national'],
      properties: {
        csv_url: parsed.url,
        years_covered: parsed.years,
        latest_total: parsed.byYear[parsed.years[parsed.years.length - 1]]?.total ?? null,
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
      live_source: parsed ? 'npa_yukuefumeisha_csv' : null,
      upstream_url: INDEX_URL,
      description: 'NPA annual missing-persons aggregates pinned at NPA HQ; one feature per year, tagged year_month for the temporal slider.',
    },
    intel: { items: intelItems },
  };
}
