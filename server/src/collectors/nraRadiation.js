/**
 * NRA Radiation Monitoring Collector
 * Nuclear Regulation Authority dose rate data
 * Fallback with known monitoring stations across Japan
 */

const API_URL = 'https://radioactivity.nra.go.jp/api/api_one.html';
const TIMEOUT_MS = 5000;

const STATIONS = [
  // Fukushima area (higher readings)
  { id: 'NRA001', name: '福島市杉妻町', pref: '福島県', lat: 37.750, lon: 140.468, baseline: 120 },
  { id: 'NRA002', name: '郡山合同庁舎', pref: '福島県', lat: 37.400, lon: 140.360, baseline: 95 },
  { id: 'NRA003', name: 'いわき合同庁舎', pref: '福島県', lat: 36.945, lon: 140.888, baseline: 85 },
  { id: 'NRA004', name: '南相馬市原町区', pref: '福島県', lat: 37.642, lon: 140.957, baseline: 150 },
  { id: 'NRA005', name: '双葉町中央', pref: '福島県', lat: 37.451, lon: 141.009, baseline: 350 },
  { id: 'NRA006', name: '大熊町大川原', pref: '福島県', lat: 37.394, lon: 140.979, baseline: 420 },
  { id: 'NRA007', name: '浪江町幾世橋', pref: '福島県', lat: 37.495, lon: 141.001, baseline: 280 },
  { id: 'NRA008', name: '富岡町小浜', pref: '福島県', lat: 37.342, lon: 141.010, baseline: 220 },
  { id: 'NRA009', name: '飯舘村役場', pref: '福島県', lat: 37.680, lon: 140.737, baseline: 180 },
  { id: 'NRA010', name: '田村市常葉', pref: '福島県', lat: 37.433, lon: 140.574, baseline: 75 },
  // Prefectural capitals
  { id: 'NRA011', name: '札幌', pref: '北海道', lat: 43.064, lon: 141.347, baseline: 25 },
  { id: 'NRA012', name: '青森', pref: '青森県', lat: 40.824, lon: 140.740, baseline: 30 },
  { id: 'NRA013', name: '盛岡', pref: '岩手県', lat: 39.704, lon: 141.153, baseline: 28 },
  { id: 'NRA014', name: '仙台', pref: '宮城県', lat: 38.269, lon: 140.872, baseline: 45 },
  { id: 'NRA015', name: '秋田', pref: '秋田県', lat: 39.720, lon: 140.103, baseline: 30 },
  { id: 'NRA016', name: '山形', pref: '山形県', lat: 38.241, lon: 140.364, baseline: 40 },
  { id: 'NRA017', name: '水戸', pref: '茨城県', lat: 36.342, lon: 140.447, baseline: 55 },
  { id: 'NRA018', name: '宇都宮', pref: '栃木県', lat: 36.566, lon: 139.884, baseline: 50 },
  { id: 'NRA019', name: '前橋', pref: '群馬県', lat: 36.391, lon: 139.061, baseline: 30 },
  { id: 'NRA020', name: 'さいたま', pref: '埼玉県', lat: 35.857, lon: 139.649, baseline: 35 },
  { id: 'NRA021', name: '千葉', pref: '千葉県', lat: 35.605, lon: 140.123, baseline: 38 },
  { id: 'NRA022', name: '新宿', pref: '東京都', lat: 35.694, lon: 139.703, baseline: 35 },
  { id: 'NRA023', name: '横浜', pref: '神奈川県', lat: 35.448, lon: 139.642, baseline: 32 },
  { id: 'NRA024', name: '新潟', pref: '新潟県', lat: 37.902, lon: 139.023, baseline: 35 },
  { id: 'NRA025', name: '金沢', pref: '石川県', lat: 36.594, lon: 136.626, baseline: 40 },
  { id: 'NRA026', name: '名古屋', pref: '愛知県', lat: 35.180, lon: 136.907, baseline: 35 },
  { id: 'NRA027', name: '京都', pref: '京都府', lat: 35.021, lon: 135.756, baseline: 40 },
  { id: 'NRA028', name: '大阪', pref: '大阪府', lat: 34.686, lon: 135.520, baseline: 42 },
  { id: 'NRA029', name: '神戸', pref: '兵庫県', lat: 34.691, lon: 135.183, baseline: 40 },
  { id: 'NRA030', name: '岡山', pref: '岡山県', lat: 34.662, lon: 133.935, baseline: 45 },
  { id: 'NRA031', name: '広島', pref: '広島県', lat: 34.396, lon: 132.460, baseline: 48 },
  { id: 'NRA032', name: '松山', pref: '愛媛県', lat: 33.842, lon: 132.766, baseline: 45 },
  { id: 'NRA033', name: '福岡', pref: '福岡県', lat: 33.607, lon: 130.418, baseline: 40 },
  { id: 'NRA034', name: '鹿児島', pref: '鹿児島県', lat: 31.560, lon: 130.558, baseline: 35 },
  { id: 'NRA035', name: '那覇', pref: '沖縄県', lat: 26.335, lon: 127.681, baseline: 22 },
  // Nuclear facility adjacent stations
  { id: 'NRA036', name: '東海村', pref: '茨城県', lat: 36.467, lon: 140.564, baseline: 60 },
  { id: 'NRA037', name: '柏崎市', pref: '新潟県', lat: 37.372, lon: 138.560, baseline: 35 },
  { id: 'NRA038', name: '敦賀市', pref: '福井県', lat: 35.645, lon: 136.055, baseline: 45 },
  { id: 'NRA039', name: '六ヶ所村', pref: '青森県', lat: 40.955, lon: 141.368, baseline: 32 },
  { id: 'NRA040', name: '玄海町', pref: '佐賀県', lat: 33.472, lon: 129.870, baseline: 38 },
];

function generateSeedData() {
  const now = new Date();
  return STATIONS.map(st => {
    const variation = (Math.random() - 0.5) * 0.2 * st.baseline;
    const doseRate = Math.round((st.baseline + variation) * 10) / 10;

    let status = 'normal';
    if (doseRate > 500) status = 'elevated';
    else if (doseRate > 200) status = 'attention';
    else if (doseRate > 100) status = 'monitoring';

    return {
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [st.lon, st.lat] },
      properties: {
        station_id: st.id,
        station_name: st.name,
        prefecture: st.pref,
        dose_rate_nGyh: doseRate,
        status,
        measured_at: now.toISOString(),
        source: 'nra_seed',
      },
    };
  });
}

export default async function collectNraRadiation() {
  let features = [];
  let source = 'nra_live';

  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(API_URL, {
      signal: controller.signal,
      headers: { Accept: 'application/json' },
    });
    clearTimeout(timer);

    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.json();

    if (Array.isArray(data) && data.length > 0) {
      features = data
        .filter(d => d.lat && d.lon)
        .map(d => ({
          type: 'Feature',
          geometry: { type: 'Point', coordinates: [+d.lon, +d.lat] },
          properties: {
            station_id: d.id ?? d.station_id,
            station_name: d.name ?? d.station_name,
            dose_rate_nGyh: d.value ?? d.dose_rate ?? null,
            measured_at: d.datetime ?? d.measured_at ?? null,
            source: 'nra_live',
          },
        }));
    }
    if (features.length === 0) throw new Error('No features parsed');
  } catch {
    features = generateSeedData();
    source = 'nra_seed';
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Radiation dose rate monitoring from NRA',
    },
    metadata: {},
  };
}
