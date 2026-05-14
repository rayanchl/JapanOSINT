/**
 * SORAMAME Air Quality Collector
 * Ministry of Environment 大気汚染物質広域監視システム ("そらまめくん").
 *
 * Tries several public SORAMAME / EnvJP endpoints (no auth required), then
 * falls back to OSM `man_made=monitoring_station` + `monitoring:air_quality=yes`
 * for any active sensor, then finally to a static seed of major stations
 * with synthetic readings (clearly marked as `soramame_seed`).
 */

import { fetchJson, fetchOverpass, fetchText } from './_liveHelpers.js';

const ATMOS_URLS = [
  // Public atmospheric stations index (lat/lon list)
  'https://soramame.env.go.jp/data/map/kyokuLatest/Pref01.json',
  'https://soramame.env.go.jp/data/jsons/japan/atmos.json',
  // Cross-government NIES backup (the AEROS feed)
  'https://tenbou.nies.go.jp/download/Air_pollution/json/StationLatest.json',
];

const STATIONS = [
  { id: 'AQ001', name: '札幌北', pref: '北海道', lat: 43.091, lon: 141.341 },
  { id: 'AQ002', name: '旭川', pref: '北海道', lat: 43.771, lon: 142.365 },
  { id: 'AQ003', name: '釧路', pref: '北海道', lat: 42.985, lon: 144.382 },
  { id: 'AQ004', name: '青森中央', pref: '青森県', lat: 40.822, lon: 140.747 },
  { id: 'AQ005', name: '盛岡', pref: '岩手県', lat: 39.702, lon: 141.153 },
  { id: 'AQ006', name: '仙台宮城野', pref: '宮城県', lat: 38.271, lon: 140.882 },
  { id: 'AQ007', name: '秋田中央', pref: '秋田県', lat: 39.720, lon: 140.103 },
  { id: 'AQ008', name: '山形', pref: '山形県', lat: 38.241, lon: 140.334 },
  { id: 'AQ009', name: '福島', pref: '福島県', lat: 37.750, lon: 140.468 },
  { id: 'AQ010', name: '水戸', pref: '茨城県', lat: 36.366, lon: 140.471 },
  { id: 'AQ011', name: '宇都宮', pref: '栃木県', lat: 36.566, lon: 139.884 },
  { id: 'AQ012', name: '前橋', pref: '群馬県', lat: 36.391, lon: 139.061 },
  { id: 'AQ013', name: 'さいたま大宮', pref: '埼玉県', lat: 35.906, lon: 139.631 },
  { id: 'AQ014', name: '千葉中央', pref: '千葉県', lat: 35.607, lon: 140.106 },
  { id: 'AQ015', name: '東京千代田', pref: '東京都', lat: 35.694, lon: 139.754 },
  { id: 'AQ016', name: '東京新宿', pref: '東京都', lat: 35.694, lon: 139.703 },
  { id: 'AQ017', name: '東京世田谷', pref: '東京都', lat: 35.646, lon: 139.653 },
  { id: 'AQ018', name: '東京大田', pref: '東京都', lat: 35.561, lon: 139.716 },
  { id: 'AQ019', name: '東京江東', pref: '東京都', lat: 35.673, lon: 139.817 },
  { id: 'AQ020', name: '横浜鶴見', pref: '神奈川県', lat: 35.510, lon: 139.682 },
  { id: 'AQ021', name: '川崎', pref: '神奈川県', lat: 35.531, lon: 139.703 },
  { id: 'AQ022', name: '新潟中央', pref: '新潟県', lat: 37.916, lon: 139.036 },
  { id: 'AQ023', name: '富山', pref: '富山県', lat: 36.695, lon: 137.211 },
  { id: 'AQ024', name: '金沢', pref: '石川県', lat: 36.594, lon: 136.626 },
  { id: 'AQ025', name: '福井', pref: '福井県', lat: 36.065, lon: 136.222 },
  { id: 'AQ026', name: '甲府', pref: '山梨県', lat: 35.664, lon: 138.568 },
  { id: 'AQ027', name: '長野', pref: '長野県', lat: 36.232, lon: 138.181 },
  { id: 'AQ028', name: '岐阜', pref: '岐阜県', lat: 35.391, lon: 136.722 },
  { id: 'AQ029', name: '静岡', pref: '静岡県', lat: 34.977, lon: 138.383 },
  { id: 'AQ030', name: '名古屋中', pref: '愛知県', lat: 35.181, lon: 136.906 },
  { id: 'AQ031', name: '名古屋南', pref: '愛知県', lat: 35.115, lon: 136.933 },
  { id: 'AQ032', name: '津', pref: '三重県', lat: 34.730, lon: 136.509 },
  { id: 'AQ033', name: '大津', pref: '滋賀県', lat: 35.005, lon: 135.869 },
  { id: 'AQ034', name: '京都中京', pref: '京都府', lat: 35.012, lon: 135.768 },
  { id: 'AQ035', name: '大阪市此花', pref: '大阪府', lat: 34.681, lon: 135.437 },
  { id: 'AQ036', name: '大阪市天王寺', pref: '大阪府', lat: 34.653, lon: 135.519 },
  { id: 'AQ037', name: '堺', pref: '大阪府', lat: 34.573, lon: 135.483 },
  { id: 'AQ038', name: '神戸中央', pref: '兵庫県', lat: 34.690, lon: 135.196 },
  { id: 'AQ039', name: '尼崎', pref: '兵庫県', lat: 34.733, lon: 135.407 },
  { id: 'AQ040', name: '奈良', pref: '奈良県', lat: 34.685, lon: 135.833 },
  { id: 'AQ041', name: '和歌山', pref: '和歌山県', lat: 34.226, lon: 135.168 },
  { id: 'AQ042', name: '岡山', pref: '岡山県', lat: 34.662, lon: 133.935 },
  { id: 'AQ043', name: '広島中区', pref: '広島県', lat: 34.396, lon: 132.460 },
  { id: 'AQ044', name: '北九州小倉', pref: '福岡県', lat: 33.883, lon: 130.875 },
  { id: 'AQ045', name: '福岡中央', pref: '福岡県', lat: 33.590, lon: 130.402 },
  { id: 'AQ046', name: '久留米', pref: '福岡県', lat: 33.319, lon: 130.508 },
  { id: 'AQ047', name: '熊本', pref: '熊本県', lat: 32.790, lon: 130.742 },
  { id: 'AQ048', name: '鹿児島', pref: '鹿児島県', lat: 31.560, lon: 130.558 },
  { id: 'AQ049', name: '那覇', pref: '沖縄県', lat: 26.335, lon: 127.681 },
  { id: 'AQ050', name: '松山', pref: '愛媛県', lat: 33.842, lon: 132.766 },
];

/**
 * Fetch each prefecture-keyed JSON ("Pref01.json" .. "Pref47.json") if the
 * primary endpoint pattern works. Returns features or null.
 */
async function tryPrefScan() {
  const features = [];
  // Iterate all 47 prefectures concurrently (low load — small JSON each)
  const tasks = [];
  for (let p = 1; p <= 47; p++) {
    const code = String(p).padStart(2, '0');
    const url = `https://soramame.env.go.jp/data/map/kyokuLatest/Pref${code}.json`;
    tasks.push(fetchJson(url, { timeoutMs: 10_000 }).then((data) => {
      if (!data || !Array.isArray(data)) return;
      for (const r of data) {
        const lat = Number(r?.緯度 ?? r?.lat ?? r?.LAT);
        const lon = Number(r?.経度 ?? r?.lon ?? r?.LON);
        const geocoded = Number.isFinite(lat) && Number.isFinite(lon);
        features.push({
          type: 'Feature',
          geometry: geocoded ? { type: 'Point', coordinates: [lon, lat] } : null,
          properties: {
            station_id: String(r?.測定局コード ?? r?.code ?? ''),
            station_name: r?.測定局名称 ?? r?.name ?? null,
            prefecture: r?.都道府県名 ?? null,
            pm25_ugm3: r?.PM25 ?? null,
            pm10_ugm3: r?.SPM ?? null,
            so2_ppb: r?.SO2 ?? null,
            no2_ppb: r?.NO2 ?? null,
            ox_ppb: r?.OX ?? null,
            co_ppm: r?.CO ?? null,
            measured_at: r?.測定時刻 ?? null,
            source: 'soramame_live',
          },
        });
      }
    }).catch(() => {}));
  }
  await Promise.all(tasks);
  return features.length > 0 ? features : null;
}

async function tryAtmosJson() {
  for (const url of ATMOS_URLS) {
    const data = await fetchJson(url, { timeoutMs: 10_000 });
    if (!data) continue;
    const arr = Array.isArray(data) ? data : (data.features || data.stations || data.list || data.data);
    if (!Array.isArray(arr) || arr.length === 0) continue;
    const features = [];
    for (const r of arr) {
      // GeoJSON feature?
      if (r?.geometry?.coordinates) {
        features.push({
          type: 'Feature',
          geometry: r.geometry,
          properties: { ...(r.properties || {}), source: 'soramame_live' },
        });
        continue;
      }
      const lat = Number(r?.lat ?? r?.LAT ?? r?.緯度);
      const lon = Number(r?.lon ?? r?.LON ?? r?.経度);
      const geocoded = Number.isFinite(lat) && Number.isFinite(lon);
      features.push({
        type: 'Feature',
        geometry: geocoded ? { type: 'Point', coordinates: [lon, lat] } : null,
        properties: {
          station_id: r?.station_id ?? r?.code ?? null,
          station_name: r?.name ?? r?.station_name ?? null,
          prefecture: r?.pref ?? null,
          pm25_ugm3: r?.pm25 ?? null,
          source: 'soramame_live',
        },
      });
    }
    if (features.length > 0) return features;
  }
  return null;
}

async function tryOsmStations() {
  return fetchOverpass(
    [
      'node["man_made"="monitoring_station"]["monitoring:air_quality"="yes"](area.jp);',
      'node["man_made"="monitoring_station"]["monitoring:weather"="yes"](area.jp);',
    ].join(''),
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        station_id: `OSM_${el.id}`,
        station_name: el.tags?.['name:en'] || el.tags?.name || 'Air monitoring station',
        operator: el.tags?.operator || null,
        ref: el.tags?.ref || null,
        source: 'osm_overpass',
      },
    }),
    60_000,
    { limit: 0, queryTimeout: 120 },
  );
}

function randomInRange(min, max) {
  return Math.round((min + Math.random() * (max - min)) * 10) / 10;
}

function generateSeedData() {
  const now = new Date();
  return STATIONS.map((st) => {
    const isUrban = ['東京', '大阪', '名古屋', '横浜', '川崎', '北九州'].some(
      (c) => st.name.includes(c) || st.pref.includes(c),
    );
    const f = isUrban ? 1.4 : 1.0;
    const pm25 = randomInRange(5, 35) * f;
    return {
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [st.lon, st.lat] },
      properties: {
        station_id: st.id,
        station_name: st.name,
        prefecture: st.pref,
        pm25_ugm3: Math.round(pm25 * 10) / 10,
        measured_at: now.toISOString(),
        source: 'soramame_seed',
      },
    };
  });
}

export default async function collectSoramame() {
  // Live cascade: prefecture scan → atmos JSON → OSM monitoring stations → seed
  let features = await tryPrefScan();
  let live = !!(features && features.length);
  let liveSrc = live ? 'soramame_pref_scan' : null;

  if (!live) {
    features = await tryAtmosJson();
    live = !!(features && features.length);
    if (live) liveSrc = 'soramame_atmos';
  }
  if (!live) {
    features = await tryOsmStations();
    live = !!(features && features.length);
    if (live) liveSrc = 'osm_monitoring_station';
  }
  if (!live) features = [];

  return {
    type: 'FeatureCollection',
    features: features || [],
    _meta: {
      source: live ? liveSrc : 'soramame_seed',
      fetchedAt: new Date().toISOString(),
      recordCount: features?.length || 0,
      live,
      description: 'SORAMAME / NIES air quality monitoring stations across Japan',
    },
  };
}
