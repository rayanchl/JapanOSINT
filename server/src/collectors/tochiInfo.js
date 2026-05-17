/**
 * Tochi.info land-use / land-price collector (tochi-info).
 *
 * MLIT's 土地総合情報システム exposes a long-standing KEY-FREE JSON API at
 * https://www.land.mlit.go.jp/webland/api/ — endpoint `TradeListSearch`
 * returns real real-estate transaction records by year/quarter and area
 * (2-digit prefecture code). We pull the latest completed quarter for all
 * 47 prefectures. No key required. On any upstream failure the collector
 * returns an honest empty intel envelope — never a fabricated record.
 *
 * NOTE: MLIT is migrating this system into reinfolib.mlit.go.jp; the legacy
 * webland API still resolves as of 2026 but is treated as best-effort and
 * degrades to empty if retired.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchJson } from './_liveHelpers.js';

const SOURCE_ID = 'tochi-info';
const API_URL = 'https://www.land.mlit.go.jp/webland/api/TradeListSearch';

function latestYearQuarter() {
  const d = new Date();
  d.setMonth(d.getMonth() - 6); // ~1-2 quarters in arrears
  return { year: d.getFullYear(), quarter: Math.floor(d.getMonth() / 3) + 1 };
}

export default async function collectTochiInfo() {
  const { year, quarter } = latestYearQuarter();
  const items = [];

  for (let pref = 1; pref <= 47; pref++) {
    const area = String(pref).padStart(2, '0');
    const url = `${API_URL}?from=${year}${quarter}&to=${year}${quarter}&area=${area}`;
    const data = await fetchJson(url, { timeoutMs: 15000 });
    const rows = data?.status === 'OK' && Array.isArray(data.data) ? data.data : null;
    if (!rows) continue;
    for (let i = 0; i < rows.length; i++) {
      const r = rows[i];
      items.push({
        uid: intelUid(SOURCE_ID, `${area}-${year}Q${quarter}-${i}`),
        title: `${r.Municipality ?? r.Prefecture ?? area} — ${r.Type ?? 'land transaction'}`,
        summary: `${r.TradePrice ?? '?'} JPY · ${r.DistrictName ?? ''} · ${r.LandShape ?? ''}`,
        body: `MLIT webland TradeListSearch ${year}Q${quarter} area=${area}: ${JSON.stringify(r)}`,
        link: 'https://www.land.mlit.go.jp/webland/',
        language: 'ja',
        published_at: new Date().toISOString(),
        tags: ['economy', 'land-use', 'landprice'],
        properties: {
          prefCode: pref,
          year,
          quarter,
          prefecture: r.Prefecture ?? null,
          municipality: r.Municipality ?? null,
          district: r.DistrictName ?? null,
          type: r.Type ?? null,
          land_use: r.Use ?? null,
          land_shape: r.LandShape ?? null,
          trade_price: r.TradePrice ?? null,
          area_sqm: r.Area ?? null,
          raw: r,
        },
      });
    }
    await new Promise((rsv) => setTimeout(rsv, 120));
  }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: items.length > 0,
    extraMeta: {
      source: items.length > 0 ? 'tochi_info_live' : 'tochi_info_unavailable',
      year,
      quarter,
    },
    description: 'MLIT 土地総合情報システム — land transaction / land-use records (webland API, key-free)',
  });
}
