/**
 * J-SHIS Seismic Hazard Collector
 * Maps seismic hazard probability across Japan from NIED J-SHIS API.
 * Falls back to a curated mesh of high-probability zones.
 */

const JSHIS_URL = 'https://www.j-shis.bosai.go.jp/map/api/pshm/Y2020/PT01/T30_I50_PS.geojson';

// Major seismic hazard mesh points - probability of seismic intensity 6-lower or higher in 30 years
const SEED_SEISMIC_MESH = [
  // ── Nankai trough subduction zone (very high) ─────────────────
  { name: '南海トラフ 高知市', lat: 33.5594, lon: 133.5311, prob_6lower_30yr: 0.78, prefecture: '高知県' },
  { name: '南海トラフ 静岡市', lat: 34.9756, lon: 138.3828, prob_6lower_30yr: 0.81, prefecture: '静岡県' },
  { name: '南海トラフ 浜松市', lat: 34.7108, lon: 137.7261, prob_6lower_30yr: 0.71, prefecture: '静岡県' },
  { name: '南海トラフ 名古屋市', lat: 35.1814, lon: 136.9069, prob_6lower_30yr: 0.46, prefecture: '愛知県' },
  { name: '南海トラフ 大阪市', lat: 34.6864, lon: 135.5197, prob_6lower_30yr: 0.55, prefecture: '大阪府' },
  { name: '南海トラフ 和歌山市', lat: 34.2261, lon: 135.1675, prob_6lower_30yr: 0.68, prefecture: '和歌山県' },
  { name: '南海トラフ 徳島市', lat: 34.0658, lon: 134.5594, prob_6lower_30yr: 0.73, prefecture: '徳島県' },
  { name: '南海トラフ 高松市', lat: 34.3401, lon: 134.0434, prob_6lower_30yr: 0.62, prefecture: '香川県' },
  { name: '南海トラフ 松山市', lat: 33.8392, lon: 132.7656, prob_6lower_30yr: 0.46, prefecture: '愛媛県' },
  { name: '南海トラフ 宮崎市', lat: 31.9111, lon: 131.4239, prob_6lower_30yr: 0.43, prefecture: '宮崎県' },

  // ── Sagami trough / Tokyo (high) ──────────────────────────────
  { name: '相模トラフ 東京駅', lat: 35.6812, lon: 139.7671, prob_6lower_30yr: 0.47, prefecture: '東京都' },
  { name: '相模トラフ 横浜市', lat: 35.4437, lon: 139.6380, prob_6lower_30yr: 0.82, prefecture: '神奈川県' },
  { name: '相模トラフ 川崎市', lat: 35.5311, lon: 139.7036, prob_6lower_30yr: 0.71, prefecture: '神奈川県' },
  { name: '相模トラフ 千葉市', lat: 35.6083, lon: 140.1233, prob_6lower_30yr: 0.85, prefecture: '千葉県' },
  { name: '相模トラフ さいたま市', lat: 35.8617, lon: 139.6455, prob_6lower_30yr: 0.55, prefecture: '埼玉県' },
  { name: '相模トラフ 水戸市', lat: 36.3658, lon: 140.4711, prob_6lower_30yr: 0.81, prefecture: '茨城県' },
  { name: '相模トラフ 宇都宮市', lat: 36.5658, lon: 139.8836, prob_6lower_30yr: 0.45, prefecture: '栃木県' },
  { name: '相模トラフ 前橋市', lat: 36.3911, lon: 139.0608, prob_6lower_30yr: 0.27, prefecture: '群馬県' },

  // ── Japan trench (Tohoku) ────────────────────────────────────
  { name: '日本海溝 仙台市', lat: 38.2683, lon: 140.8719, prob_6lower_30yr: 0.45, prefecture: '宮城県' },
  { name: '日本海溝 福島市', lat: 37.7503, lon: 140.4675, prob_6lower_30yr: 0.32, prefecture: '福島県' },
  { name: '日本海溝 盛岡市', lat: 39.7036, lon: 141.1525, prob_6lower_30yr: 0.21, prefecture: '岩手県' },
  { name: '日本海溝 青森市', lat: 40.8244, lon: 140.7400, prob_6lower_30yr: 0.18, prefecture: '青森県' },
  { name: '日本海溝 秋田市', lat: 39.7186, lon: 140.1024, prob_6lower_30yr: 0.10, prefecture: '秋田県' },
  { name: '日本海溝 山形市', lat: 38.2403, lon: 140.3633, prob_6lower_30yr: 0.07, prefecture: '山形県' },

  // ── Active inland faults ────────────────────────────────────
  { name: '中央構造線 京都市', lat: 35.0116, lon: 135.7681, prob_6lower_30yr: 0.32, prefecture: '京都府' },
  { name: '中央構造線 神戸市', lat: 34.6913, lon: 135.1830, prob_6lower_30yr: 0.45, prefecture: '兵庫県' },
  { name: '中央構造線 奈良市', lat: 34.6850, lon: 135.8048, prob_6lower_30yr: 0.61, prefecture: '奈良県' },
  { name: '糸魚川静岡構造線 長野市', lat: 36.6489, lon: 138.1944, prob_6lower_30yr: 0.27, prefecture: '長野県' },
  { name: '糸魚川静岡構造線 甲府市', lat: 35.6642, lon: 138.5683, prob_6lower_30yr: 0.42, prefecture: '山梨県' },
  { name: '糸魚川静岡構造線 富山市', lat: 36.6953, lon: 137.2113, prob_6lower_30yr: 0.13, prefecture: '富山県' },
  { name: '布田川断層 熊本市', lat: 32.8019, lon: 130.7256, prob_6lower_30yr: 0.39, prefecture: '熊本県' },

  // ── Hokkaido (Chishima) ─────────────────────────────────────
  { name: '千島海溝 根室市', lat: 43.3306, lon: 145.5828, prob_6lower_30yr: 0.21, prefecture: '北海道' },
  { name: '千島海溝 釧路市', lat: 42.9849, lon: 144.3819, prob_6lower_30yr: 0.69, prefecture: '北海道' },
  { name: '千島海溝 帯広市', lat: 42.9239, lon: 143.1953, prob_6lower_30yr: 0.27, prefecture: '北海道' },
  { name: '千島海溝 札幌市', lat: 43.0628, lon: 141.3478, prob_6lower_30yr: 0.07, prefecture: '北海道' },

  // ── Kyushu / Okinawa ────────────────────────────────────────
  { name: '日向灘 大分市', lat: 33.2381, lon: 131.6126, prob_6lower_30yr: 0.55, prefecture: '大分県' },
  { name: '日向灘 宮崎市', lat: 31.9111, lon: 131.4239, prob_6lower_30yr: 0.43, prefecture: '宮崎県' },
  { name: '南西諸島海溝 那覇市', lat: 26.2125, lon: 127.6809, prob_6lower_30yr: 0.21, prefecture: '沖縄県' },
  { name: '熊本地震 益城町', lat: 32.7867, lon: 130.8133, prob_6lower_30yr: 0.39, prefecture: '熊本県' },

  // ── Other major cities ──────────────────────────────────────
  { name: '北陸 金沢市', lat: 36.5613, lon: 136.6562, prob_6lower_30yr: 0.10, prefecture: '石川県' },
  { name: '北陸 福井市', lat: 36.0613, lon: 136.2229, prob_6lower_30yr: 0.21, prefecture: '福井県' },
  { name: '中国 広島市', lat: 34.3853, lon: 132.4553, prob_6lower_30yr: 0.18, prefecture: '広島県' },
  { name: '中国 岡山市', lat: 34.6628, lon: 133.9197, prob_6lower_30yr: 0.34, prefecture: '岡山県' },
  { name: '九州 福岡市', lat: 33.5904, lon: 130.4017, prob_6lower_30yr: 0.07, prefecture: '福岡県' },
];

async function tryJshis() {
  try {
    const ctrl = new AbortController();
    const timeout = setTimeout(() => ctrl.abort(), 15000);
    const res = await fetch(JSHIS_URL, { signal: ctrl.signal });
    clearTimeout(timeout);
    if (!res.ok) return null;
    const data = await res.json();
    if (!data.features || data.features.length === 0) return null;
    return data.features.slice(0, 1000).map((f, i) => ({
      type: 'Feature',
      geometry: f.geometry,
      properties: {
        mesh_id: `JSHIS_${String(i + 1).padStart(6, '0')}`,
        prob_6lower_30yr: f.properties?.T30_I50_PS || null,
        country: 'JP',
        source: 'jshis_api',
      },
    }));
  } catch {
    return null;
  }
}

function generateSeedData() {
  const now = new Date();
  return SEED_SEISMIC_MESH.map((m, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [m.lon, m.lat] },
    properties: {
      mesh_id: `JSHIS_${String(i + 1).padStart(6, '0')}`,
      name: m.name,
      prob_6lower_30yr: m.prob_6lower_30yr,
      prefecture: m.prefecture,
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'jshis_seed',
    },
  }));
}

export default async function collectJshisSeismic() {
  let features = await tryJshis();
  const live = !!(features && features.length > 0);
  if (!live) features = [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'jshis_seismic',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'NIED J-SHIS seismic hazard probability mesh (intensity 6-lower or higher in 30 years)',
    },
  };
}
