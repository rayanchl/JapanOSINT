/**
 * RESAS Tourism Collector
 * Fetches tourist visitor stats from RESAS API.
 * Falls back to a curated seed of major tourist destinations with annual visitor counts.
 */

const RESAS_KEY = process.env.RESAS_API_KEY || '';
const RESAS_URL = 'https://opendata.resas-portal.go.jp/api/v1/tourism/foreigners/forFrom';

const SEED_TOURIST_SITES = [
  // ── Tokyo top sites ─────────────────────────────────────
  { name: '東京ディズニーリゾート', lat: 35.6329, lon: 139.8804, visitors_yr: 30100000, foreign_pct: 7, prefecture: '千葉県' },
  { name: '東京ディズニーシー', lat: 35.6267, lon: 139.8854, visitors_yr: 14650000, foreign_pct: 6, prefecture: '千葉県' },
  { name: '浅草寺', lat: 35.7148, lon: 139.7967, visitors_yr: 30000000, foreign_pct: 25, prefecture: '東京都' },
  { name: '東京スカイツリー', lat: 35.7100, lon: 139.8107, visitors_yr: 4600000, foreign_pct: 35, prefecture: '東京都' },
  { name: '東京タワー', lat: 35.6586, lon: 139.7454, visitors_yr: 3000000, foreign_pct: 40, prefecture: '東京都' },
  { name: '明治神宮', lat: 35.6764, lon: 139.6993, visitors_yr: 13700000, foreign_pct: 30, prefecture: '東京都' },
  { name: '上野動物園', lat: 35.7156, lon: 139.7714, visitors_yr: 4500000, foreign_pct: 15, prefecture: '東京都' },
  { name: '渋谷スクランブル交差点', lat: 35.6595, lon: 139.7008, visitors_yr: 100000000, foreign_pct: 25, prefecture: '東京都' },
  { name: '築地場外市場', lat: 35.6650, lon: 139.7715, visitors_yr: 14000000, foreign_pct: 35, prefecture: '東京都' },
  { name: '新宿御苑', lat: 35.6852, lon: 139.7100, visitors_yr: 2700000, foreign_pct: 30, prefecture: '東京都' },

  // ── Kyoto top sites ─────────────────────────────────────
  { name: '伏見稲荷大社', lat: 34.9671, lon: 135.7727, visitors_yr: 10000000, foreign_pct: 70, prefecture: '京都府' },
  { name: '清水寺', lat: 34.9949, lon: 135.7851, visitors_yr: 5500000, foreign_pct: 65, prefecture: '京都府' },
  { name: '金閣寺 (鹿苑寺)', lat: 35.0394, lon: 135.7292, visitors_yr: 4500000, foreign_pct: 60, prefecture: '京都府' },
  { name: '銀閣寺 (慈照寺)', lat: 35.0270, lon: 135.7983, visitors_yr: 1800000, foreign_pct: 50, prefecture: '京都府' },
  { name: '嵐山 渡月橋', lat: 35.0125, lon: 135.6781, visitors_yr: 6000000, foreign_pct: 55, prefecture: '京都府' },
  { name: '二条城', lat: 35.0142, lon: 135.7475, visitors_yr: 2300000, foreign_pct: 60, prefecture: '京都府' },
  { name: '京都鉄道博物館', lat: 34.9858, lon: 135.7558, visitors_yr: 1200000, foreign_pct: 20, prefecture: '京都府' },

  // ── Osaka / Kansai ──────────────────────────────────────
  { name: 'ユニバーサル・スタジオ・ジャパン', lat: 34.6656, lon: 135.4322, visitors_yr: 14500000, foreign_pct: 20, prefecture: '大阪府' },
  { name: '大阪城', lat: 34.6873, lon: 135.5259, visitors_yr: 2750000, foreign_pct: 50, prefecture: '大阪府' },
  { name: '道頓堀', lat: 34.6687, lon: 135.5028, visitors_yr: 30000000, foreign_pct: 40, prefecture: '大阪府' },
  { name: '海遊館', lat: 34.6549, lon: 135.4290, visitors_yr: 2500000, foreign_pct: 25, prefecture: '大阪府' },
  { name: '通天閣', lat: 34.6526, lon: 135.5063, visitors_yr: 1300000, foreign_pct: 30, prefecture: '大阪府' },
  { name: '奈良公園 (鹿)', lat: 34.6850, lon: 135.8431, visitors_yr: 13000000, foreign_pct: 60, prefecture: '奈良県' },
  { name: '東大寺', lat: 34.6890, lon: 135.8398, visitors_yr: 5000000, foreign_pct: 55, prefecture: '奈良県' },
  { name: '姫路城', lat: 34.8394, lon: 134.6939, visitors_yr: 1800000, foreign_pct: 40, prefecture: '兵庫県' },

  // ── Other regions ───────────────────────────────────────
  { name: '富士山 五合目', lat: 35.3950, lon: 138.7300, visitors_yr: 5000000, foreign_pct: 30, prefecture: '山梨県' },
  { name: '河口湖', lat: 35.5167, lon: 138.7522, visitors_yr: 4000000, foreign_pct: 25, prefecture: '山梨県' },
  { name: '熱海温泉', lat: 35.0950, lon: 139.0719, visitors_yr: 3000000, foreign_pct: 5, prefecture: '静岡県' },
  { name: '箱根湯本', lat: 35.2333, lon: 139.0250, visitors_yr: 7000000, foreign_pct: 20, prefecture: '神奈川県' },
  { name: '横浜中華街', lat: 35.4444, lon: 139.6489, visitors_yr: 18000000, foreign_pct: 15, prefecture: '神奈川県' },
  { name: '日光東照宮', lat: 36.7581, lon: 139.5986, visitors_yr: 1500000, foreign_pct: 30, prefecture: '栃木県' },
  { name: '広島平和記念公園', lat: 34.3925, lon: 132.4525, visitors_yr: 1700000, foreign_pct: 50, prefecture: '広島県' },
  { name: '厳島神社 (宮島)', lat: 34.2960, lon: 132.3199, visitors_yr: 4500000, foreign_pct: 45, prefecture: '広島県' },
  { name: '札幌雪まつり', lat: 43.0606, lon: 141.3547, visitors_yr: 2500000, foreign_pct: 30, prefecture: '北海道' },
  { name: '小樽運河', lat: 43.1989, lon: 140.9928, visitors_yr: 7500000, foreign_pct: 40, prefecture: '北海道' },
  { name: '函館山夜景', lat: 41.7600, lon: 140.7039, visitors_yr: 1500000, foreign_pct: 35, prefecture: '北海道' },
  { name: '太宰府天満宮', lat: 33.5219, lon: 130.5347, visitors_yr: 8000000, foreign_pct: 30, prefecture: '福岡県' },
  { name: '熊本城', lat: 32.8064, lon: 130.7058, visitors_yr: 2200000, foreign_pct: 25, prefecture: '熊本県' },
  { name: '阿蘇山', lat: 32.8847, lon: 131.1042, visitors_yr: 1500000, foreign_pct: 25, prefecture: '熊本県' },
  { name: '美ら海水族館', lat: 26.6944, lon: 127.8779, visitors_yr: 3500000, foreign_pct: 25, prefecture: '沖縄県' },
  { name: '首里城', lat: 26.2169, lon: 127.7194, visitors_yr: 2700000, foreign_pct: 30, prefecture: '沖縄県' },
  { name: '出雲大社', lat: 35.4019, lon: 132.6855, visitors_yr: 6000000, foreign_pct: 5, prefecture: '島根県' },
];

async function tryResas() {
  if (!RESAS_KEY) return null;
  try {
    const ctrl = new AbortController();
    const timeout = setTimeout(() => ctrl.abort(), 10000);
    const url = `${RESAS_URL}?year=2019&prefCode=13&matter=1`;
    const res = await fetch(url, {
      signal: ctrl.signal,
      headers: { 'X-API-KEY': RESAS_KEY },
    });
    clearTimeout(timeout);
    if (!res.ok) return null;
    const data = await res.json();
    const items = data.result?.changes || [];
    if (items.length === 0) return null;
    return items.slice(0, 50).map((it, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [139.6917, 35.6896] },
      properties: {
        from_country: it.prefName || null,
        value: it.value || null,
        country: 'JP',
        source: 'resas_api',
      },
    }));
  } catch {
    return null;
  }
}

function generateSeedData() {
  const now = new Date();
  return SEED_TOURIST_SITES.map((s, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [s.lon, s.lat] },
    properties: {
      site_id: `TOUR_${String(i + 1).padStart(5, '0')}`,
      name: s.name,
      visitors_yr: s.visitors_yr,
      foreign_pct: s.foreign_pct,
      prefecture: s.prefecture,
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'resas_tourism_seed',
    },
  }));
}

export default async function collectResasTourism() {
  let features = await tryResas();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'resas_tourism',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'RESAS tourism data + major tourist destinations with annual visitor counts',
    },
    metadata: {},
  };
}
