/**
 * NPA Special Fraud (特殊詐欺) — monthly nationwide statistics.
 *
 * Source: `hurikomesagi_toukei.csv` published under
 * `/bureau/criminal/souni/tokusyusagi/`. The file is Shift_JIS, a
 * multi-section sheet covering the current and previous Reiwa years with
 * monthly columns Jan–Dec. We extract the four headline series:
 *   - 認知件数 (recognised cases)
 *   - 実質的な被害総額 (effective damage total in JPY)
 *   - 検挙件数 (arrests)
 *   - 検挙人員 (arrested persons)
 *
 * The CSV is national-aggregate (no per-prefecture rows), so each emitted
 * feature is pinned at NPA HQ in Tokyo and tagged `year_month` for the
 * temporal slider.
 */

import { fetchArrayBuffer, decodeShiftJis, parseCsv } from './_liveHelpers.js';
import { intelUid } from '../utils/intelHelpers.js';

const SOURCE_ID = 'npa-special-fraud';
const CSV_URL = 'https://www.npa.go.jp/bureau/criminal/souni/tokusyusagi/hurikomesagi_toukei.csv';
const INDEX_URL = 'https://www.npa.go.jp/publications/statistics/sousa/sagi.html';
const NPA_HQ = { lat: 35.6749, lon: 139.7531 };

function reiwaToYear(label) {
  const m = (label || '').match(/令和\s*([０-９0-9一二三四五六七八九十]+)\s*年/);
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
  const n = parseInt(String(s).replace(/[",\s円]/g, ''), 10);
  return Number.isFinite(n) ? n : null;
}

const MONTH_LABELS = ['１月','２月','３月','４月','５月','６月','７月','８月','９月','１０月','１１月','１２月'];

function extract(rows) {
  // Each year section is preceded by a row containing 令和X年 and followed
  // by a header row that lists 合計 / １月 … １２月. Walk the file linearly,
  // tracking the current year and column→month map; capture the four
  // metric rows under each year.
  const out = []; // { year, month (1-12), metric, value }
  let currentYear = null;
  let monthCols = null; // index → month
  for (let i = 0; i < rows.length; i++) {
    const row = rows[i];
    const joined = row.join(' ');
    const yr = reiwaToYear(joined);
    if (yr) currentYear = yr;
    // Detect header row by scanning for month labels
    if (row.some((c) => c === '１月') && row.some((c) => c === '１２月')) {
      monthCols = {};
      for (let j = 0; j < row.length; j++) {
        const idx = MONTH_LABELS.indexOf(row[j].trim());
        if (idx >= 0) monthCols[j] = idx + 1;
      }
      continue;
    }
    if (!currentYear || !monthCols) continue;
    const label = (row.find((c) => c.trim() !== '') || '').trim();
    let metric = null;
    if (label === '認知件数') metric = 'recognised';
    else if (label === '実質的な被害総額　（単位：円）' || label.startsWith('実質的な被害総額')) metric = 'damage_jpy';
    else if (label === '検挙件数') metric = 'arrests';
    else if (label === '検挙人員') metric = 'arrested_persons';
    if (!metric) continue;
    for (const [colIdx, month] of Object.entries(monthCols)) {
      const v = toInt(row[Number(colIdx)]);
      if (v != null) out.push({ year: currentYear, month, metric, value: v });
    }
  }
  return out;
}

function pivot(records) {
  const map = new Map(); // 'YYYY-MM' → metrics
  for (const r of records) {
    const ym = `${r.year}-${String(r.month).padStart(2, '0')}`;
    if (!map.has(ym)) map.set(ym, { year: r.year, month: r.month, ym });
    map.get(ym)[r.metric] = r.value;
  }
  return Array.from(map.values()).sort((a, b) => a.ym.localeCompare(b.ym));
}

export default async function collectNpaSpecialFraud() {
  const fetchedAt = new Date().toISOString();
  const buf = await fetchArrayBuffer(CSV_URL, { timeoutMs: 15000 });
  if (!buf) {
    return {
      type: 'FeatureCollection',
      features: [],
      _meta: { source: SOURCE_ID, fetchedAt, recordCount: 0, live: false, description: 'NPA special-fraud CSV unreachable.' },
      intel: { items: [] },
    };
  }
  const text = decodeShiftJis(buf);
  const rows = parseCsv(text, { headers: false });
  const records = extract(rows);
  const months = pivot(records);

  const features = months
    .filter((m) => m.recognised != null)
    .map((m, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [NPA_HQ.lon, NPA_HQ.lat] },
      properties: {
        id: `FRAUD_${m.ym}_${i}`,
        year_month: m.ym,
        year: m.year,
        month: m.month,
        recognised: m.recognised ?? null,
        damage_jpy: m.damage_jpy ?? null,
        arrests: m.arrests ?? null,
        arrested_persons: m.arrested_persons ?? null,
        source: SOURCE_ID,
      },
    }));

  const intelItems = [{
    uid: intelUid(SOURCE_ID, 'index'),
    title: 'NPA Special Fraud Monthly Statistics (特殊詐欺認知・検挙状況)',
    summary: `Monthly nationwide totals: ${months.length} months parsed; latest ${months[months.length - 1]?.ym ?? 'n/a'}.`,
    link: INDEX_URL,
    language: 'ja',
    published_at: fetchedAt,
    tags: ['crime', 'fraud', 'statistics', 'national'],
    properties: {
      csv_url: CSV_URL,
      months_covered: months.length,
      latest_month: months[months.length - 1]?.ym ?? null,
      latest_recognised: months[months.length - 1]?.recognised ?? null,
      latest_damage_jpy: months[months.length - 1]?.damage_jpy ?? null,
    },
  }];

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: SOURCE_ID,
      fetchedAt,
      recordCount: features.length,
      live: features.length > 0,
      live_source: 'npa_hurikomesagi_csv',
      upstream_url: INDEX_URL,
      description: 'NPA monthly special-fraud (特殊詐欺) totals — recognised cases, damage in JPY, arrests, arrested persons. One feature per month tagged year_month.',
    },
    intel: { items: intelItems },
  };
}
