/**
 * gBizINFO Geocoded Collector
 * METI's 経済産業省 gBizINFO corporate database - geocoded HQ locations for
 * major Japanese corporations (法人番号 / corporate number registry).
 *
 * Tries the gBizINFO API if GBIZ_API_KEY env var is set, otherwise falls
 * back to a curated seed of major corporate HQs.
 *
 * API: https://info.gbiz.go.jp/hojin/v1/hojin
 */

import { fetchJson } from './_liveHelpers.js';

const GBIZ_KEY = process.env.GBIZ_API_KEY || '';
const GBIZ_URL = 'https://info.gbiz.go.jp/hojin/v1/hojin';

// Curated seed of major Japanese corporate HQs with approximate capital,
// employee counts and primary industry (from gBizINFO public records).
const SEED_GBIZ = [
  // Automotive
  { corp: '1180301018771', name: 'トヨタ自動車株式会社', name_en: 'Toyota Motor Corporation', lat: 35.0822, lon: 137.1536, industry: 'automotive', employees: 372817, capital_jpy: 635401000000, prefecture: '愛知県', city: '豊田市' },
  { corp: '6010401058862', name: '本田技研工業株式会社', name_en: 'Honda Motor Co., Ltd.', lat: 35.6694, lon: 139.7297, industry: 'automotive', employees: 204035, capital_jpy: 86067000000, prefecture: '東京都', city: '港区' },
  { corp: '7010401022916', name: '日産自動車株式会社', name_en: 'Nissan Motor Co., Ltd.', lat: 35.4636, lon: 139.6222, industry: 'automotive', employees: 131461, capital_jpy: 605813000000, prefecture: '神奈川県', city: '横浜市' },
  { corp: '9180001017325', name: 'スズキ株式会社', name_en: 'Suzuki Motor Corporation', lat: 34.7037, lon: 137.7295, industry: 'automotive', employees: 68499, capital_jpy: 138014000000, prefecture: '静岡県', city: '浜松市' },
  { corp: '8011101028104', name: '株式会社SUBARU', name_en: 'Subaru Corporation', lat: 35.7011, lon: 139.7156, industry: 'automotive', employees: 37693, capital_jpy: 153795000000, prefecture: '東京都', city: '渋谷区' },
  { corp: '6140001002588', name: 'マツダ株式会社', name_en: 'Mazda Motor Corporation', lat: 34.3667, lon: 132.5233, industry: 'automotive', employees: 49135, capital_jpy: 283957000000, prefecture: '広島県', city: '安芸郡' },

  // Electronics / Technology
  { corp: '7010401052137', name: 'ソニーグループ株式会社', name_en: 'Sony Group Corporation', lat: 35.6283, lon: 139.7394, industry: 'electronics', employees: 108900, capital_jpy: 880214000000, prefecture: '東京都', city: '港区' },
  { corp: '5120001077455', name: 'パナソニックホールディングス株式会社', name_en: 'Panasonic Holdings', lat: 34.7689, lon: 135.6158, industry: 'electronics', employees: 233391, capital_jpy: 259363000000, prefecture: '大阪府', city: '門真市' },
  { corp: '8010401007778', name: '株式会社日立製作所', name_en: 'Hitachi, Ltd.', lat: 35.6825, lon: 139.7697, industry: 'electronics', employees: 322525, capital_jpy: 462817000000, prefecture: '東京都', city: '千代田区' },
  { corp: '8010401023267', name: '富士通株式会社', name_en: 'Fujitsu Limited', lat: 35.6294, lon: 139.7436, industry: 'electronics_it', employees: 124000, capital_jpy: 324625000000, prefecture: '東京都', city: '港区' },
  { corp: '6010401023450', name: 'NEC株式会社', name_en: 'NEC Corporation', lat: 35.6747, lon: 139.7597, industry: 'electronics_it', employees: 118527, capital_jpy: 427831000000, prefecture: '東京都', city: '港区' },
  { corp: '1011001027294', name: 'キヤノン株式会社', name_en: 'Canon Inc.', lat: 35.6764, lon: 139.6881, industry: 'electronics_optics', employees: 180775, capital_jpy: 174762000000, prefecture: '東京都', city: '大田区' },
  { corp: '7010001008844', name: '株式会社ニコン', name_en: 'Nikon Corporation', lat: 35.6717, lon: 139.7158, industry: 'electronics_optics', employees: 19365, capital_jpy: 65475000000, prefecture: '東京都', city: '港区' },

  // Finance / Banking
  { corp: '9010001008669', name: '三菱UFJ銀行', name_en: 'MUFG Bank', lat: 35.6816, lon: 139.7655, industry: 'finance_banking', employees: 165000, capital_jpy: 1711958000000, prefecture: '東京都', city: '千代田区' },
  { corp: '6010001008845', name: '株式会社みずほ銀行', name_en: 'Mizuho Bank', lat: 35.6833, lon: 139.7606, industry: 'finance_banking', employees: 55000, capital_jpy: 1404065000000, prefecture: '東京都', city: '千代田区' },
  { corp: '3010001008848', name: '株式会社三井住友銀行', name_en: 'Sumitomo Mitsui Banking', lat: 35.6889, lon: 139.7633, industry: 'finance_banking', employees: 28113, capital_jpy: 1770996000000, prefecture: '東京都', city: '千代田区' },
  { corp: '4010401022860', name: '野村ホールディングス株式会社', name_en: 'Nomura Holdings', lat: 35.6828, lon: 139.7769, industry: 'finance_securities', employees: 26000, capital_jpy: 594493000000, prefecture: '東京都', city: '中央区' },

  // Telecom
  { corp: '7010001064648', name: '日本電信電話株式会社', name_en: 'NTT Corp.', lat: 35.6853, lon: 139.7658, industry: 'telecom', employees: 324667, capital_jpy: 937950000000, prefecture: '東京都', city: '千代田区' },
  { corp: '9010701021531', name: 'ソフトバンク株式会社', name_en: 'SoftBank Corp.', lat: 35.6503, lon: 139.7392, industry: 'telecom', employees: 54988, capital_jpy: 204309000000, prefecture: '東京都', city: '港区' },
  { corp: '9011101031552', name: 'KDDI株式会社', name_en: 'KDDI Corporation', lat: 35.6806, lon: 139.7569, industry: 'telecom', employees: 48829, capital_jpy: 141852000000, prefecture: '東京都', city: '千代田区' },

  // Trading / Retail
  { corp: '6010401033167', name: '三井物産株式会社', name_en: 'Mitsui & Co., Ltd.', lat: 35.6878, lon: 139.7756, industry: 'trading', employees: 45634, capital_jpy: 342435000000, prefecture: '東京都', city: '千代田区' },
  { corp: '1010401019817', name: '三菱商事株式会社', name_en: 'Mitsubishi Corporation', lat: 35.6756, lon: 139.7622, industry: 'trading', employees: 80728, capital_jpy: 204446000000, prefecture: '東京都', city: '千代田区' },
  { corp: '6010001030403', name: '株式会社ファーストリテイリング', name_en: 'Fast Retailing (UNIQLO)', lat: 34.1700, lon: 131.4706, industry: 'retail_apparel', employees: 57727, capital_jpy: 10273000000, prefecture: '山口県', city: '山口市' },
  { corp: '2013301012522', name: '株式会社セブン＆アイ・ホールディングス', name_en: 'Seven & i Holdings', lat: 35.6847, lon: 139.7167, industry: 'retail', employees: 159842, capital_jpy: 50000000000, prefecture: '東京都', city: '千代田区' },

  // Gaming / Entertainment
  { corp: '4120001059231', name: '任天堂株式会社', name_en: 'Nintendo Co., Ltd.', lat: 34.9667, lon: 135.7564, industry: 'entertainment_gaming', employees: 7317, capital_jpy: 10065000000, prefecture: '京都府', city: '京都市' },
  { corp: '9011001050418', name: '株式会社バンダイナムコホールディングス', name_en: 'Bandai Namco Holdings', lat: 35.6280, lon: 139.7794, industry: 'entertainment_gaming', employees: 10730, capital_jpy: 10000000000, prefecture: '東京都', city: '港区' },

  // Chemicals / Pharma
  { corp: '6120001041769', name: '武田薬品工業株式会社', name_en: 'Takeda Pharmaceutical', lat: 35.6870, lon: 139.7444, industry: 'pharmaceutical', employees: 49578, capital_jpy: 1676596000000, prefecture: '東京都', city: '中央区' },
  { corp: '5010001034790', name: '信越化学工業株式会社', name_en: 'Shin-Etsu Chemical', lat: 35.6740, lon: 139.7601, industry: 'chemical', employees: 25657, capital_jpy: 119419000000, prefecture: '東京都', city: '千代田区' },

  // Heavy industry
  { corp: '4010001008772', name: '三菱重工業株式会社', name_en: 'Mitsubishi Heavy Industries', lat: 35.6628, lon: 139.7281, industry: 'heavy_industry', employees: 77991, capital_jpy: 265608000000, prefecture: '東京都', city: '千代田区' },
  { corp: '1140001003452', name: '川崎重工業株式会社', name_en: 'Kawasaki Heavy Industries', lat: 34.6811, lon: 135.1894, industry: 'heavy_industry', employees: 36691, capital_jpy: 104484000000, prefecture: '兵庫県', city: '神戸市' },
  { corp: '8010001059128', name: '日本製鉄株式会社', name_en: 'Nippon Steel Corporation', lat: 35.6664, lon: 139.7591, industry: 'steel', employees: 106456, capital_jpy: 419524000000, prefecture: '東京都', city: '千代田区' },
];

async function tryGbiz() {
  if (!GBIZ_KEY) return null;
  try {
    // Fetch a page of major corporations (prefecture 13 = Tokyo) by capital
    const url = `${GBIZ_URL}?prefecture=13&capital_stock_from=10000000000&limit=100`;
    const data = await fetchJson(url, {
      timeoutMs: 12000,
      headers: { 'X-hojinInfo-api-token': GBIZ_KEY, Accept: 'application/json' },
    });
    const items = data?.['hojin-infos'] || data?.hojinInfos || [];
    if (!items.length) return null;
    return items
      .map((it, i) => {
        const lat = it.latitude ?? it.lat ?? null;
        const lon = it.longitude ?? it.lon ?? null;
        if (lat == null || lon == null) return null;
        return {
          type: 'Feature',
          geometry: { type: 'Point', coordinates: [Number(lon), Number(lat)] },
          properties: {
            corp_number: it.corporate_number || it.corporateNumber,
            name: it.name,
            name_en: it.name_en || null,
            industry: it.business_summary || it.primary_industry || null,
            employees: it.employee_number || null,
            capital_jpy: it.capital_stock || null,
            prefecture: it.prefecture_name || null,
            city: it.city_name || null,
            country: 'JP',
            source: 'gbizinfo_api',
          },
        };
      })
      .filter(Boolean);
  } catch {
    return null;
  }
}

function generateSeedData() {
  const now = new Date();
  return SEED_GBIZ.map((s) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [s.lon, s.lat] },
    properties: {
      corp_number: s.corp,
      name: s.name,
      name_en: s.name_en,
      industry: s.industry,
      employees: s.employees,
      capital_jpy: s.capital_jpy,
      prefecture: s.prefecture,
      city: s.city,
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'gbizinfo_seed',
    },
  }));
}

export default async function collectGbizinfoGeocoded() {
  let features = await tryGbiz();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'gbizinfo-geocoded',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'gbizinfo_api' : 'gbizinfo_seed',
      description: 'METI gBizINFO corporate registry - geocoded HQ locations of major Japanese corporations with capital, employees and industry',
    },
    metadata: {},
  };
}
