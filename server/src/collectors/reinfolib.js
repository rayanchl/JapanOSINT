/**
 * MLIT Real Estate Information Library collector (reinfolib).
 *
 * Live: MLIT 不動産情報ライブラリ external API
 * (https://www.reinfolib.mlit.go.jp/ex-api/external/). Auth: subscription
 * key via `REINFOLIB_API_KEY` (envFor), sent as the
 * `Ocp-Apim-Subscription-Key` header per MLIT's API docs. Without the key,
 * or on any upstream failure, returns an honest empty intel envelope —
 * never a fabricated transaction.
 *
 * Endpoint XIT001 = 不動産取引価格情報 (real-estate transaction prices) by
 * year/quarter/area (area = 2-digit prefecture code). We pull the latest
 * completed quarter for all 47 prefectures.
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { envFor } from '../utils/collectorEnv.js';

const SOURCE_ID = 'reinfolib';
const API_URL = 'https://www.reinfolib.mlit.go.jp/ex-api/external/XIT001';
const TIMEOUT_MS = 15000;

// Latest fully-published quarter (conservative: two quarters back from now
// so the data is guaranteed released). MLIT publishes ~1 quarter in arrears.
function latestYearQuarter() {
  const d = new Date();
  d.setMonth(d.getMonth() - 6);
  const year = d.getFullYear();
  const quarter = Math.floor(d.getMonth() / 3) + 1;
  return { year, quarter };
}

async function fetchPref(area, year, quarter, key) {
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const url = `${API_URL}?year=${year}&quarter=${quarter}&area=${area}`;
    const res = await fetch(url, {
      signal: ctrl.signal,
      headers: { 'Ocp-Apim-Subscription-Key': key, Accept: 'application/json' },
    });
    clearTimeout(timer);
    if (!res.ok) return null;
    const data = await res.json();
    return Array.isArray(data?.data) ? data.data : null;
  } catch {
    return null;
  }
}

export default async function collectReinfolib() {
  const key = envFor('REINFOLIB_API_KEY');
  if (!key) {
    return intelEnvelope({
      sourceId: SOURCE_ID,
      items: [],
      live: false,
      extraMeta: { source: 'reinfolib_no_key', needs_key: 'REINFOLIB_API_KEY' },
      description: 'MLIT Real Estate Information Library transactions (requires REINFOLIB_API_KEY)',
    });
  }

  const { year, quarter } = latestYearQuarter();
  const items = [];
  for (let pref = 1; pref <= 47; pref++) {
    const area = String(pref).padStart(2, '0');
    const rows = await fetchPref(area, year, quarter, key);
    if (!Array.isArray(rows)) continue;
    for (let i = 0; i < rows.length; i++) {
      const r = rows[i];
      items.push({
        uid: intelUid(SOURCE_ID, `${area}-${year}Q${quarter}-${i}`),
        title: `${r.Municipality ?? r.Prefecture ?? area} — ${r.Type ?? 'transaction'}`,
        summary: `${r.TradePrice ?? '?'} JPY · ${r.DistrictName ?? ''}`,
        body: `MLIT reinfolib XIT001 ${year}Q${quarter} area=${area}: ${JSON.stringify(r)}`,
        link: 'https://www.reinfolib.mlit.go.jp/',
        language: 'ja',
        published_at: new Date().toISOString(),
        tags: ['economy', 'real-estate', 'landprice'],
        properties: {
          prefCode: pref,
          year,
          quarter,
          prefecture: r.Prefecture ?? null,
          municipality: r.Municipality ?? null,
          district: r.DistrictName ?? null,
          type: r.Type ?? null,
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
      source: items.length > 0 ? 'reinfolib_live' : 'reinfolib_unavailable',
      year,
      quarter,
    },
    description: 'MLIT Real Estate Information Library — transaction prices (XIT001)',
  });
}
