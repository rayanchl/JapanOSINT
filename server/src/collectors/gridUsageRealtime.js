/**
 * Per-grid 5-minute supply/demand CSV feeds.
 *
 * Each of the 10 regional grids publishes a juyo-d-j.csv (today's by-5-min
 * load) — 99 % of the dynamic-economy signal in JP, lagged by minutes.
 * Pair with crowd density for an economic-activity heatmap.
 *
 * Endpoints:
 *   TEPCO    https://www.tepco.co.jp/forecast/html/images/juyo-d-j.csv
 *   KEPCO    https://www.kansai-td.co.jp/yamasou/juyo1_kansai.csv
 *   CHUDEN   https://denki-yoho.chuden.jp/denki_yoho_content_data/juyo_cepco003.csv
 *   TOHOKU   https://setsuden.nw.tohoku-epco.co.jp/common/demand/juyo_05_TOHOKU.csv
 *   KYUDEN   https://www.kyuden.co.jp/td_power_usages/csv/juyo-hourly-Kyushu.csv
 *   HEPCO    https://denkiyoho.hepco.co.jp/area/data/juyo_01_hokkaido.csv
 *   YONDEN   https://www.yonden.co.jp/nw/denkiyoho/csv/juyo_shikoku.csv
 *   RIKUDEN  https://www.rikuden.co.jp/nw/denki-yoho/csv/juyo_05_rikuden.csv
 *   CHUGOKU  https://www.energia.co.jp/nw/jukyuu/sys/juyo-d1-j.csv
 *   OKINAWA  https://www.okiden.co.jp/denki2/juyo_10_okinawa.csv
 */

import { intelEnvelope, intelUid } from '../utils/intelHelpers.js';
import { fetchText } from './_liveHelpers.js';

const SOURCE_ID = 'grid-usage-realtime';

const GRIDS = [
  { id: 'tepco',    url: 'https://www.tepco.co.jp/forecast/html/images/juyo-d-j.csv',                     lat: 35.6762, lon: 139.6503 },
  { id: 'kepco',    url: 'https://www.kansai-td.co.jp/yamasou/juyo1_kansai.csv',                          lat: 34.6937, lon: 135.5023 },
  { id: 'chuden',   url: 'https://denki-yoho.chuden.jp/denki_yoho_content_data/juyo_cepco003.csv',        lat: 35.1815, lon: 136.9066 },
  { id: 'tohoku',   url: 'https://setsuden.nw.tohoku-epco.co.jp/common/demand/juyo_05_TOHOKU.csv',        lat: 38.2682, lon: 140.8694 },
  { id: 'kyuden',   url: 'https://www.kyuden.co.jp/td_power_usages/csv/juyo-hourly-Kyushu.csv',           lat: 33.5902, lon: 130.4017 },
  { id: 'hepco',    url: 'https://denkiyoho.hepco.co.jp/area/data/juyo_01_hokkaido.csv',                  lat: 43.0642, lon: 141.3469 },
  { id: 'yonden',   url: 'https://www.yonden.co.jp/nw/denkiyoho/csv/juyo_shikoku.csv',                    lat: 34.3401, lon: 134.0434 },
  { id: 'rikuden',  url: 'https://www.rikuden.co.jp/nw/denki-yoho/csv/juyo_05_rikuden.csv',               lat: 36.6953, lon: 137.2113 },
  { id: 'chugoku',  url: 'https://www.energia.co.jp/nw/jukyuu/sys/juyo-d1-j.csv',                         lat: 34.3966, lon: 132.4596 },
  { id: 'okiden',   url: 'https://www.okiden.co.jp/denki2/juyo_10_okinawa.csv',                           lat: 26.2124, lon: 127.6792 },
];

function parseLastLoad(csv) {
  if (!csv) return null;
  const lines = csv.split(/\r?\n/).map((l) => l.trim()).filter(Boolean);
  // Find last data line containing two numbers — date/time + load
  for (let i = lines.length - 1; i >= 0; i--) {
    const cols = lines[i].split(',');
    if (cols.length < 2) continue;
    const last = parseFloat(cols[cols.length - 1]);
    if (Number.isFinite(last) && last > 0) {
      return { load_mw: last, raw: lines[i] };
    }
  }
  return null;
}

export default async function collectGridUsageRealtime() {
  const items = [];
  let anyLive = false;

  for (const g of GRIDS) {
    let csv = '';
    try { csv = await fetchText(g.url, { timeoutMs: 10000 }); } catch { /* keep empty */ }
    const parsed = parseLastLoad(csv);
    if (parsed) anyLive = true;
    items.push({
      uid: intelUid(SOURCE_ID, g.id),
      title: `${g.id.toUpperCase()} 5-min supply/demand`,
      summary: parsed ? `latest load ≈ ${parsed.load_mw} (raw: ${parsed.raw})` : 'unreachable',
      link: g.url,
      language: 'ja',
      published_at: new Date().toISOString(),
      tags: ['power', 'grid', 'demand', g.id],
      properties: {
        grid: g.id,
        lat: g.lat,
        lon: g.lon,
        load_mw: parsed?.load_mw ?? null,
        reachable: !!parsed,
      },
    });
  }

  return intelEnvelope({
    sourceId: SOURCE_ID,
    items,
    live: anyLive,
    description: 'Regional grid 5-minute supply/demand CSVs (10 grids)',
  });
}
