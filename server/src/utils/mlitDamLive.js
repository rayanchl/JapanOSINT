/**
 * Click-triggered live water level fetch for MLIT-administered dams.
 *
 * Upstream: MLIT 水文・水質DB at www1.river.go.jp. No JSON — the CGI
 * returns an EUC-JP HTML scaffold that references an iframe carrying the
 * real hourly rows. We resolve the iframe URL from the scaffold, then
 * parse the row table.
 *
 * This is NOT polled on a schedule. The upstream explicitly states it
 * does not want tool-based access, so we only fetch when a user clicks
 * a dam pin (see client/src/components/map/MapPopup.jsx → DamLiveLevel)
 * and cache results for 5 minutes in-process to avoid hammering on
 * repeat clicks.
 *
 * Coverage: MLIT's DB is limited to dams it administers (河川局 / 水資源
 * 機構). Power-company dams (J-Power, TEPCO, KEPCO) are not listed, so
 * seed entries like 黒部ダム / 奥只見ダム / 佐久間ダム intentionally
 * have no mapping and fetchDamLive() returns { ok: false, reason: 'no
 * MLIT ID mapped' } for them.
 */

import IconvLite from 'iconv-lite';

const MLIT_CGI = 'http://www1.river.go.jp/cgi-bin/DspDamData.exe';
const MLIT_ORIGIN = 'http://www1.river.go.jp';
const BROWSER_HEADERS = {
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/127.0.0.0 Safari/537.36',
  'Referer': 'http://www1.river.go.jp/',
  'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
  'Accept-Language': 'ja,en;q=0.8',
};

// Resolved once from a one-shot scrape of SrchSite.exe?KOMOKU=05 across
// all 13 pages (126 dams total), then matched against the SEED_DAMS in
// damWaterLevel.js by normalised name. 29 of 46 seed dams map into MLIT's
// DB; the rest are power-company dams MLIT does not administer.
const DAM_ID_MAP = {
  DAM_00001: '605091285502400',  // 徳山ダム
  DAM_00004: '1368030375210',    // 下久保ダム
  DAM_00005: '1368030375010',    // 矢木沢ダム
  DAM_00006: '1368030375020',    // 奈良俣ダム
  DAM_00007: '1368030375030',    // 藤原ダム
  DAM_00008: '1368030375090',    // 相俣ダム
  DAM_00009: '1368030375180',    // 草木ダム
  DAM_00010: '1368030376090',    // 川治ダム
  DAM_00011: '1368030376050',    // 五十里ダム
  DAM_00012: '1368030799020',    // 宮ヶ瀬ダム
  DAM_00018: '1368040365050',    // 大町ダム
  DAM_00019: '1368050570120',    // 美和ダム
  DAM_00021: '608061288803010',  // 長安口ダム
  DAM_00022: '1368050931320',    // 阿木川ダム
  DAM_00023: '1368050931255',    // 丸山ダム
  DAM_00024: '1368050931035',    // 味噌川ダム
  DAM_00025: '1368060475080',    // 布目ダム (機構)
  DAM_00027: '1368060444010',    // 一庫ダム (機構)
  DAM_00029: '1368080700040',    // 池田ダム (機構, 徳島)
  DAM_00030: '1368080700010',    // 早明浦ダム (機構)
  DAM_00035: '1368091050080',    // 緑川ダム
  DAM_00037: '609061289920020',  // 松原ダム
  DAM_00038: '1368090600030',    // 寺内ダム
  DAM_00039: '1368090600040',    // 江川ダム
  DAM_00040: '1368010829060',    // 十勝ダム
  DAM_00044: '601031281101001',  // 夕張シューパロダム
  DAM_00045: '1368060778130',    // 九頭竜ダム
  DAM_00046: '1368060778090',    // 真名川ダム
  // unresolved (power-company, not MLIT):
  //   DAM_00002 奥只見ダム, DAM_00003 田子倉ダム, DAM_00013 相模ダム,
  //   DAM_00014 黒部ダム, DAM_00015 佐久間ダム, DAM_00016 井川ダム,
  //   DAM_00017 高瀬ダム, DAM_00020 長島ダム, DAM_00026 布引ダム,
  //   DAM_00028 津風呂ダム, DAM_00031 津賀ダム, DAM_00032 一ツ瀬ダム,
  //   DAM_00033 上椎葉ダム, DAM_00034 川辺川ダム (計画),
  //   DAM_00036 耶馬渓ダム, DAM_00041 糠平ダム, DAM_00042 雨竜第一ダム,
  //   DAM_00043 池田ダム (北海道) — prefecturally administered
};

const CACHE_TTL_MS = 5 * 60 * 1000;
const FETCH_TIMEOUT_MS = 10_000;
const cache = new Map();  // damId → { at: number, data: object }

function yyyymmdd(d) {
  const y = d.getFullYear();
  const m = String(d.getMonth() + 1).padStart(2, '0');
  const dd = String(d.getDate()).padStart(2, '0');
  return `${y}${m}${dd}`;
}

// Upstream values use "-" for "未受信" (not received) and "$" for "欠測"
// (missing observation). Normalise to null. "閉局" means station closed.
function parseCell(raw) {
  if (raw == null) return null;
  // strip <FONT …> wrappers and surrounding whitespace
  const s = String(raw).replace(/<[^>]+>/g, '').trim();
  if (!s || s === '-' || s === '$' || s === '閉局') return null;
  const n = Number(s);
  return Number.isFinite(n) ? n : null;
}

async function fetchEucJp(url) {
  const ctrl = new AbortController();
  const t = setTimeout(() => ctrl.abort(), FETCH_TIMEOUT_MS);
  try {
    const res = await fetch(url, { headers: BROWSER_HEADERS, signal: ctrl.signal, redirect: 'follow' });
    if (!res.ok) return { ok: false, reason: `HTTP ${res.status}` };
    const buf = Buffer.from(await res.arrayBuffer());
    return { ok: true, html: IconvLite.decode(buf, 'EUC-JP') };
  } catch (err) {
    return { ok: false, reason: err?.message || 'fetch failed' };
  } finally {
    clearTimeout(t);
  }
}

// The scaffold page references an iframe at /html/frm/DamFreeData<session>.html
// which carries the actual hourly rows. Extract its URL.
function extractIframeUrl(scaffoldHtml) {
  const m = scaffoldHtml.match(/\/html\/frm\/DamFreeData[^"'\s]+\.html/);
  return m ? m[0] : null;
}

// Parse the iframe HTML. Each <TR> holds 7 <TD>s: date, time, rainfall
// (mm/h), storage (×10³m³), inflow (m³/s), outflow (m³/s), fill (%).
// Return the LAST row that has numeric storage OR fill — older rows can
// be "閉局" while recent ones are live.
function parseIframeRows(iframeHtml) {
  const rows = [];
  const rowRe = /<TR>([\s\S]*?)<\/TR>/gi;
  const cellRe = /<TD[^>]*>([\s\S]*?)<\/TD>/gi;
  let rm;
  while ((rm = rowRe.exec(iframeHtml)) !== null) {
    const cells = [];
    let cm;
    cellRe.lastIndex = 0;
    while ((cm = cellRe.exec(rm[1])) !== null) cells.push(cm[1]);
    if (cells.length < 7) continue;
    const [dateRaw, timeRaw, rainRaw, storageRaw, inflowRaw, outflowRaw, fillRaw] = cells;
    const date = String(dateRaw).replace(/<[^>]+>/g, '').trim();
    const time = String(timeRaw).replace(/<[^>]+>/g, '').trim();
    if (!/^\d{4}\/\d{2}\/\d{2}$/.test(date)) continue;
    rows.push({
      date,
      time,
      rainfall_mm_h: parseCell(rainRaw),
      storage_1000m3: parseCell(storageRaw),
      inflow_m3s: parseCell(inflowRaw),
      outflow_m3s: parseCell(outflowRaw),
      fill_pct: parseCell(fillRaw),
    });
  }
  // Latest row with actual data in storage or fill.
  for (let i = rows.length - 1; i >= 0; i--) {
    const r = rows[i];
    if (r.storage_1000m3 != null || r.fill_pct != null) return r;
  }
  return null;
}

function cacheAndReturn(damId, data) {
  cache.set(damId, { at: Date.now(), data });
  return data;
}

export async function fetchDamLive(damId) {
  const stationId = DAM_ID_MAP[damId];
  if (!stationId) return { ok: false, reason: 'no MLIT ID mapped' };

  const hit = cache.get(damId);
  if (hit && Date.now() - hit.at < CACHE_TTL_MS) return hit.data;

  // 3-day window gives us enough rows to find the latest live reading
  // even for dams that report at daily rather than hourly cadence.
  const now = new Date();
  const bgn = new Date(now.getTime() - 3 * 24 * 3600 * 1000);
  const scaffoldUrl = `${MLIT_CGI}?KIND=1&ID=${stationId}&BGNDATE=${yyyymmdd(bgn)}&ENDDATE=${yyyymmdd(now)}`;

  const scaffold = await fetchEucJp(scaffoldUrl);
  if (!scaffold.ok) return cacheAndReturn(damId, { ok: false, reason: scaffold.reason });

  const iframePath = extractIframeUrl(scaffold.html);
  if (!iframePath) return cacheAndReturn(damId, { ok: false, reason: 'no iframe in scaffold' });

  const iframe = await fetchEucJp(`${MLIT_ORIGIN}${iframePath}`);
  if (!iframe.ok) return cacheAndReturn(damId, { ok: false, reason: iframe.reason });

  const row = parseIframeRows(iframe.html);
  if (!row) return cacheAndReturn(damId, { ok: false, reason: 'station offline / no data rows' });

  return cacheAndReturn(damId, {
    ok: true,
    station_id: stationId,
    observed_at: `${row.date} ${row.time}`,
    rainfall_mm_h:  row.rainfall_mm_h,
    storage_1000m3: row.storage_1000m3,
    inflow_m3s:     row.inflow_m3s,
    outflow_m3s:    row.outflow_m3s,
    fill_pct:       row.fill_pct,
  });
}
