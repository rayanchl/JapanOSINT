/**
 * Hazard Map Portal Collector
 * Curated set of high-risk hazard zones across Japan: tsunami inundation,
 * volcanic eruption, landslide, flood, and liquefaction zones.
 * Data points are centroids of well-known hazard areas published by MLIT.
 */

const HAZARD_ZONES = [
  // ── Tsunami inundation (Pacific coast) ─────────────────────────
  { name: '気仙沼湾 津波浸水想定', lat: 38.9067, lon: 141.5700, hazard: 'tsunami', max_depth_m: 15, prefecture: '宮城県' },
  { name: '石巻湾 津波浸水想定', lat: 38.4344, lon: 141.3028, hazard: 'tsunami', max_depth_m: 12, prefecture: '宮城県' },
  { name: '陸前高田 津波浸水想定', lat: 39.0286, lon: 141.6256, hazard: 'tsunami', max_depth_m: 18, prefecture: '岩手県' },
  { name: '釜石 津波浸水想定', lat: 39.2756, lon: 141.8856, hazard: 'tsunami', max_depth_m: 14, prefecture: '岩手県' },
  { name: '宮古 津波浸水想定', lat: 39.6411, lon: 141.9525, hazard: 'tsunami', max_depth_m: 13, prefecture: '岩手県' },
  { name: '南海トラフ 串本町', lat: 33.4694, lon: 135.7775, hazard: 'tsunami', max_depth_m: 22, prefecture: '和歌山県' },
  { name: '南海トラフ 黒潮町', lat: 33.0064, lon: 132.9844, hazard: 'tsunami', max_depth_m: 34, prefecture: '高知県' },
  { name: '南海トラフ 須崎市', lat: 33.3978, lon: 133.2842, hazard: 'tsunami', max_depth_m: 25, prefecture: '高知県' },
  { name: '南海トラフ 高知市', lat: 33.5594, lon: 133.5311, hazard: 'tsunami', max_depth_m: 16, prefecture: '高知県' },
  { name: '南海トラフ 静岡市', lat: 34.9756, lon: 138.3828, hazard: 'tsunami', max_depth_m: 11, prefecture: '静岡県' },
  { name: '南海トラフ 浜松市', lat: 34.7108, lon: 137.7261, hazard: 'tsunami', max_depth_m: 12, prefecture: '静岡県' },
  { name: '南海トラフ 紀伊半島南端', lat: 33.4514, lon: 135.7889, hazard: 'tsunami', max_depth_m: 20, prefecture: '和歌山県' },

  // ── Active volcanoes (eruption zones) ──────────────────────────
  { name: '富士山 噴火警戒区域', lat: 35.3606, lon: 138.7274, hazard: 'volcano', alert_level: 1, prefecture: '山梨県' },
  { name: '桜島 噴火警戒区域', lat: 31.5853, lon: 130.6572, hazard: 'volcano', alert_level: 3, prefecture: '鹿児島県' },
  { name: '阿蘇山 噴火警戒区域', lat: 32.8847, lon: 131.1042, hazard: 'volcano', alert_level: 2, prefecture: '熊本県' },
  { name: '雲仙岳 噴火警戒区域', lat: 32.7558, lon: 130.2964, hazard: 'volcano', alert_level: 1, prefecture: '長崎県' },
  { name: '霧島山 (新燃岳) 噴火警戒区域', lat: 31.9097, lon: 130.8861, hazard: 'volcano', alert_level: 2, prefecture: '宮崎県' },
  { name: '草津白根山 噴火警戒区域', lat: 36.6450, lon: 138.5283, hazard: 'volcano', alert_level: 2, prefecture: '群馬県' },
  { name: '浅間山 噴火警戒区域', lat: 36.4039, lon: 138.5269, hazard: 'volcano', alert_level: 2, prefecture: '長野県' },
  { name: '御嶽山 噴火警戒区域', lat: 35.8933, lon: 137.4806, hazard: 'volcano', alert_level: 1, prefecture: '長野県' },
  { name: '十勝岳 噴火警戒区域', lat: 43.4181, lon: 142.6856, hazard: 'volcano', alert_level: 2, prefecture: '北海道' },
  { name: '有珠山 噴火警戒区域', lat: 42.5333, lon: 140.8389, hazard: 'volcano', alert_level: 1, prefecture: '北海道' },
  { name: '蔵王山 噴火警戒区域', lat: 38.1444, lon: 140.4500, hazard: 'volcano', alert_level: 1, prefecture: '宮城県' },
  { name: '吾妻山 噴火警戒区域', lat: 37.7350, lon: 140.2444, hazard: 'volcano', alert_level: 1, prefecture: '福島県' },
  { name: '口永良部島 噴火警戒区域', lat: 30.4433, lon: 130.2169, hazard: 'volcano', alert_level: 3, prefecture: '鹿児島県' },
  { name: '硫黄島 噴火警戒区域', lat: 24.7610, lon: 141.2880, hazard: 'volcano', alert_level: 2, prefecture: '東京都' },
  { name: '諏訪之瀬島 噴火警戒区域', lat: 29.6383, lon: 129.7158, hazard: 'volcano', alert_level: 2, prefecture: '鹿児島県' },

  // ── Major landslide / rockfall zones ───────────────────────────
  { name: '広島市安佐南区 土砂災害警戒区域', lat: 34.4717, lon: 132.4661, hazard: 'landslide', risk: 'high', prefecture: '広島県' },
  { name: '熱海伊豆山 土砂災害', lat: 35.1158, lon: 139.0822, hazard: 'landslide', risk: 'extreme', prefecture: '静岡県' },
  { name: '岩泉町 土砂崩れ警戒', lat: 39.8439, lon: 141.7900, hazard: 'landslide', risk: 'high', prefecture: '岩手県' },
  { name: '柳田川流域 (能登)', lat: 37.4283, lon: 137.0189, hazard: 'landslide', risk: 'high', prefecture: '石川県' },

  // ── Flood prone river areas ────────────────────────────────────
  { name: '荒川 河川氾濫想定区域', lat: 35.7944, lon: 139.6611, hazard: 'flood', max_depth_m: 5, prefecture: '東京都' },
  { name: '利根川 河川氾濫想定区域', lat: 36.0500, lon: 139.4889, hazard: 'flood', max_depth_m: 5, prefecture: '埼玉県' },
  { name: '淀川 河川氾濫想定区域', lat: 34.7267, lon: 135.5169, hazard: 'flood', max_depth_m: 4, prefecture: '大阪府' },
  { name: '木曽三川 河川氾濫想定区域', lat: 35.2089, lon: 136.6981, hazard: 'flood', max_depth_m: 5, prefecture: '愛知県' },
  { name: '球磨川 河川氾濫想定区域 (人吉)', lat: 32.2106, lon: 130.7592, hazard: 'flood', max_depth_m: 6, prefecture: '熊本県' },
  { name: '千曲川 河川氾濫想定区域 (長野)', lat: 36.6603, lon: 138.2042, hazard: 'flood', max_depth_m: 5, prefecture: '長野県' },

  // ── Liquefaction zones (Tokyo bay area, Niigata, Kansai) ───────
  { name: '東京湾岸 液状化想定 (浦安)', lat: 35.6500, lon: 139.9000, hazard: 'liquefaction', risk: 'high', prefecture: '千葉県' },
  { name: '東京湾岸 液状化想定 (江東区)', lat: 35.6700, lon: 139.8100, hazard: 'liquefaction', risk: 'high', prefecture: '東京都' },
  { name: '横浜港湾 液状化想定', lat: 35.4486, lon: 139.6431, hazard: 'liquefaction', risk: 'medium', prefecture: '神奈川県' },
  { name: '新潟市 液状化想定', lat: 37.9161, lon: 139.0364, hazard: 'liquefaction', risk: 'high', prefecture: '新潟県' },
  { name: '大阪湾岸 液状化想定', lat: 34.6464, lon: 135.4153, hazard: 'liquefaction', risk: 'medium', prefecture: '大阪府' },
];

function generateSeedData() {
  const now = new Date();
  return HAZARD_ZONES.map((h, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [h.lon, h.lat] },
    properties: {
      hazard_id: `HAZARD_${String(i + 1).padStart(5, '0')}`,
      name: h.name,
      hazard_type: h.hazard,
      max_depth_m: h.max_depth_m || null,
      alert_level: h.alert_level || null,
      risk_level: h.risk || null,
      prefecture: h.prefecture,
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'mlit_hazard_portal',
    },
  }));
}

export default async function collectHazardMapPortal() {
  const features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'hazard_map_portal',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: false,
      description: 'Japan hazard zones - tsunami, volcano, landslide, flood, liquefaction',
    },
    metadata: {},
  };
}
