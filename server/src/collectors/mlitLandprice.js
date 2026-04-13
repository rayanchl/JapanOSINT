/**
 * MLIT Land Price Collector
 * Pulls real estate transaction prices from MLIT 不動産取引価格情報 Web API.
 *
 * The API supports a `prefecture` query (codes 01..47). We iterate every
 * prefecture for the most recent four quarters, geocoding each transaction
 * to its municipality centroid (the API returns prefecture + municipality
 * codes — we look up centroids from a small embedded table).
 *
 * No auth required.
 */

import { fetchJson } from './_liveHelpers.js';
import { MUNICIPALITY_CENTROIDS } from './_municipalityCentroids.js';

const API_URL = 'https://www.land.mlit.go.jp/webland/api/TradeListSearch';

// Generate the most recent ~6 quarters as YYYY[Q]
function recentQuarters(n = 6) {
  const out = [];
  const now = new Date();
  let y = now.getUTCFullYear();
  let q = Math.floor(now.getUTCMonth() / 3) + 1;
  // Walk back from current quarter
  for (let i = 0; i < n; i++) {
    out.push(`${y}${q}`);
    q -= 1;
    if (q < 1) { q = 4; y -= 1; }
  }
  return out.reverse();
}

function municipalityCenter(prefCode, cityCode) {
  return MUNICIPALITY_CENTROIDS[String(cityCode)]
    || MUNICIPALITY_CENTROIDS[String(prefCode).padStart(2, '0')]
    || null;
}

const SEED_PRICES = [
  { name: '銀座4丁目', area: '東京都中央区', lat: 35.6717, lon: 139.7653, price: 56080000, use: '商業地' },
  { name: '丸の内2丁目', area: '東京都千代田区', lat: 35.6802, lon: 139.7651, price: 43000000, use: '商業地' },
  { name: '新宿3丁目', area: '東京都新宿区', lat: 35.6912, lon: 139.7042, price: 32200000, use: '商業地' },
  { name: '梅田1丁目', area: '大阪府大阪市北区', lat: 34.7024, lon: 135.4983, price: 21000000, use: '商業地' },
  { name: '名駅1丁目', area: '愛知県名古屋市中村区', lat: 35.1709, lon: 136.8815, price: 15000000, use: '商業地' },
  { name: '札幌駅前通', area: '北海道札幌市中央区', lat: 43.0629, lon: 141.3544, price: 5500000, use: '商業地' },
  { name: '天神1丁目', area: '福岡県福岡市中央区', lat: 33.5917, lon: 130.3994, price: 9500000, use: '商業地' },
  { name: '四条河原町', area: '京都府京都市下京区', lat: 35.0040, lon: 135.7693, price: 7500000, use: '商業地' },
];

async function tryLive() {
  const periods = recentQuarters(6);
  const fromQ = periods[0];
  const toQ = periods[periods.length - 1];

  const all = [];
  // Iterate all 47 prefectures
  for (let p = 1; p <= 47; p++) {
    const pref = String(p).padStart(2, '0');
    const url = `${API_URL}?from=${fromQ}&to=${toQ}&area=${pref}`;
    const data = await fetchJson(url, { timeoutMs: 20_000, retries: 1 });
    const trades = data?.data;
    if (!Array.isArray(trades)) continue;
    for (let i = 0; i < trades.length; i++) {
      const t = trades[i];
      const price = parseFloat(t?.TradePrice);
      const area = parseFloat(t?.Area);
      if (!Number.isFinite(price) || !Number.isFinite(area) || area <= 0) continue;
      const center = municipalityCenter(pref, t?.MunicipalityCode);
      if (!center) continue;
      // Jitter within municipality so points don't all stack
      const jitterLat = (Math.random() - 0.5) * 0.02;
      const jitterLon = (Math.random() - 0.5) * 0.02;
      all.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [center.lon + jitterLon, center.lat + jitterLat] },
        properties: {
          point_id: `MLIT_${pref}_${i}_${t?.Period || ''}`,
          price_total_yen: Math.round(price),
          area_sqm: area,
          price_per_sqm: Math.round(price / area),
          land_use: t?.Type || null,
          purpose: t?.Purpose || null,
          structure: t?.Structure || null,
          prefecture: t?.Prefecture || null,
          municipality: t?.Municipality || null,
          district: t?.DistrictName || null,
          period: t?.Period || null,
          source: 'mlit_webland_live',
        },
      });
    }
  }
  return all.length > 0 ? all : null;
}

function generateSeedData() {
  return SEED_PRICES.map((pt, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [pt.lon, pt.lat] },
    properties: {
      point_id: `LP_${String(i + 1).padStart(3, '0')}`,
      name: pt.name,
      area: pt.area,
      price_per_sqm: pt.price,
      land_use: pt.use,
      source: 'mlit_seed',
    },
  }));
}

export default async function collectMlitLandprice() {
  let features = await tryLive();
  const live = !!(features && features.length);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: live ? 'mlit_webland_live' : 'mlit_seed',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'MLIT 不動産取引価格 nationwide transactions, geocoded to municipality centroid',
    },
    metadata: {},
  };
}
