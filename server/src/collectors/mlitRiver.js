/**
 * MLIT River Water Level Collector
 * River monitoring station water level data
 * Fallback with known major river monitoring stations
 */

import { fetchOverpass, fetchJson } from './_liveHelpers.js';

// MLIT 川の防災情報 (www.river.go.jp/kawabou) does not publish a stable public
// JSON station list. The site SPA builds its data layer dynamically with
// session state, and the legacy www1.river.go.jp CGI endpoints return a 403
// "tools prohibited" block to non-browser clients. The old speculative paths
// were returning the SPA HTML fallback (5 KB of <!DOCTYPE html>) which fetchJson
// silently rejected — producing noise, no data.
//
// No verified public JSON source at time of writing. Kept as an empty array so
// the collector falls straight through to OSM (man_made=monitoring_station)
// and then the curated seed list below.
const API_URLS = [];
const TIMEOUT_MS = 15000;
const BROWSER_HEADERS = {
  'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/127.0.0.0 Safari/537.36',
  'Referer': 'https://www.river.go.jp/kawabou/',
  'Accept': 'application/json,text/javascript,*/*;q=0.8',
};

const RIVER_STATIONS = [
  // Tone River (利根川) - Japan's largest watershed
  { name: '栗橋', river: '利根川', pref: '埼玉県', lat: 36.1322, lon: 139.7081, warnLevel: 8.5, dangerLevel: 9.9 },
  { name: '取手', river: '利根川', pref: '茨城県', lat: 35.9116, lon: 140.0601, warnLevel: 6.0, dangerLevel: 7.7 },
  { name: '佐原', river: '利根川', pref: '千葉県', lat: 35.8914, lon: 140.4992, warnLevel: 4.5, dangerLevel: 5.8 },
  // Arakawa (荒川)
  { name: '治水橋', river: '荒川', pref: '埼玉県', lat: 35.8988, lon: 139.6054, warnLevel: 6.0, dangerLevel: 7.7 },
  { name: '岩淵水門', river: '荒川', pref: '東京都', lat: 35.7928, lon: 139.7214, warnLevel: 5.0, dangerLevel: 7.0 },
  { name: '笹目橋', river: '荒川', pref: '埼玉県', lat: 35.8087, lon: 139.6729, warnLevel: 5.5, dangerLevel: 7.2 },
  // Tama River (多摩川)
  { name: '石原', river: '多摩川', pref: '東京都', lat: 35.6713, lon: 139.4452, warnLevel: 4.5, dangerLevel: 6.0 },
  { name: '田園調布', river: '多摩川', pref: '東京都', lat: 35.5909, lon: 139.6652, warnLevel: 5.0, dangerLevel: 6.5 },
  // Sumida River (隅田川)
  { name: '千住大橋', river: '隅田川', pref: '東京都', lat: 35.7468, lon: 139.7987, warnLevel: 2.5, dangerLevel: 3.5 },
  // Shinano River (信濃川) - Japan's longest river
  { name: '小千谷', river: '信濃川', pref: '新潟県', lat: 37.3123, lon: 138.7917, warnLevel: 7.0, dangerLevel: 9.0 },
  { name: '長岡大橋', river: '信濃川', pref: '新潟県', lat: 37.4517, lon: 138.8434, warnLevel: 6.5, dangerLevel: 8.5 },
  { name: '帝石橋', river: '信濃川', pref: '新潟県', lat: 37.8836, lon: 139.0201, warnLevel: 5.5, dangerLevel: 7.0 },
  // Yodo River (淀川)
  { name: '枚方', river: '淀川', pref: '大阪府', lat: 34.8134, lon: 135.6455, warnLevel: 5.0, dangerLevel: 7.0 },
  { name: '毛馬', river: '淀川', pref: '大阪府', lat: 34.7128, lon: 135.5131, warnLevel: 3.5, dangerLevel: 5.0 },
  // Kiso River (木曽川)
  { name: '犬山', river: '木曽川', pref: '愛知県', lat: 35.3853, lon: 136.9419, warnLevel: 5.0, dangerLevel: 7.0 },
  { name: '木曽川大橋', river: '木曽川', pref: '三重県', lat: 35.0607, lon: 136.7245, warnLevel: 4.5, dangerLevel: 6.5 },
  // Kitakami River (北上川)
  { name: '登米', river: '北上川', pref: '宮城県', lat: 38.6791, lon: 141.2737, warnLevel: 5.0, dangerLevel: 7.0 },
  { name: '石巻', river: '北上川', pref: '宮城県', lat: 38.4347, lon: 141.3058, warnLevel: 3.0, dangerLevel: 4.5 },
  // Ishikari River (石狩川)
  { name: '旭橋', river: '石狩川', pref: '北海道', lat: 43.7706, lon: 142.3646, warnLevel: 6.0, dangerLevel: 8.0 },
  { name: '石狩大橋', river: '石狩川', pref: '北海道', lat: 43.2249, lon: 141.3713, warnLevel: 5.0, dangerLevel: 7.0 },
  // Chikugo River (筑後川)
  { name: '瀬の下', river: '筑後川', pref: '福岡県', lat: 33.3193, lon: 130.5170, warnLevel: 5.5, dangerLevel: 7.5 },
  { name: '荒瀬', river: '筑後川', pref: '大分県', lat: 33.2879, lon: 130.9731, warnLevel: 4.0, dangerLevel: 5.5 },
  // Yoshino River (吉野川)
  { name: '池田', river: '吉野川', pref: '徳島県', lat: 34.0246, lon: 133.8086, warnLevel: 5.0, dangerLevel: 7.0 },
  { name: '岩津', river: '吉野川', pref: '徳島県', lat: 34.0813, lon: 134.3527, warnLevel: 4.0, dangerLevel: 5.5 },
  // Niyodo River (仁淀川)
  { name: '伊野', river: '仁淀川', pref: '高知県', lat: 33.5447, lon: 133.4261, warnLevel: 4.5, dangerLevel: 6.0 },
  // Mogami River (最上川)
  { name: '大石田', river: '最上川', pref: '山形県', lat: 38.5929, lon: 140.3740, warnLevel: 5.0, dangerLevel: 7.0 },
  // Tenryu River (天竜川)
  { name: '鹿島', river: '天竜川', pref: '静岡県', lat: 34.7298, lon: 137.8123, warnLevel: 5.0, dangerLevel: 6.5 },
  // Naga River (那珂川)
  { name: '野口', river: '那珂川', pref: '茨城県', lat: 36.3846, lon: 140.4613, warnLevel: 5.0, dangerLevel: 7.0 },
  // Fuji River (富士川)
  { name: '清水端', river: '富士川', pref: '静岡県', lat: 35.1408, lon: 138.6161, warnLevel: 4.0, dangerLevel: 6.0 },
  // Agano River (阿賀野川)
  { name: '馬下', river: '阿賀野川', pref: '新潟県', lat: 37.7164, lon: 139.2308, warnLevel: 5.5, dangerLevel: 7.5 },
];

function generateSeedData() {
  const now = new Date();
  return RIVER_STATIONS.map((st, i) => {
    // Normal water levels are typically 30-60% of warning level
    const normalBase = st.warnLevel * (0.3 + Math.random() * 0.3);
    const waterLevel = Math.round(normalBase * 100) / 100;

    let status = 'normal';
    if (waterLevel >= st.dangerLevel) status = 'danger';
    else if (waterLevel >= st.warnLevel) status = 'warning';
    else if (waterLevel >= st.warnLevel * 0.8) status = 'attention';

    return {
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [st.lon, st.lat] },
      properties: {
        station_id: `RIV_${String(i + 1).padStart(3, '0')}`,
        station_name: st.name,
        river_name: st.river,
        prefecture: st.pref,
        water_level_m: waterLevel,
        warning_level_m: st.warnLevel,
        danger_level_m: st.dangerLevel,
        status,
        measured_at: now.toISOString(),
        source: 'mlit_seed',
      },
    };
  });
}

async function tryMlitJson() {
  for (const url of API_URLS) {
    const data = await fetchJson(url, {
      timeoutMs: TIMEOUT_MS,
      retries: 1,
      headers: BROWSER_HEADERS,
    });
    if (!data) continue;
    const arr = Array.isArray(data) ? data : (data.stations || data.data || data.items || []);
    if (!Array.isArray(arr) || arr.length === 0) continue;
    const features = [];
    for (let i = 0; i < arr.length; i++) {
      const d = arr[i];
      const lat = Number(d?.lat ?? d?.LAT ?? d?.緯度);
      const lon = Number(d?.lon ?? d?.LON ?? d?.経度);
      if (!Number.isFinite(lat) || !Number.isFinite(lon)) continue;
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [lon, lat] },
        properties: {
          station_id: d?.station_id ?? d?.code ?? `RIV_LIVE_${i}`,
          station_name: d?.station_name ?? d?.name ?? null,
          river_name: d?.river_name ?? d?.river ?? null,
          water_level_m: d?.water_level ?? d?.level ?? null,
          source: 'mlit_river_live',
        },
      });
    }
    if (features.length > 0) return features;
  }
  return null;
}

async function tryOsmGauges() {
  return fetchOverpass(
    [
      'node["man_made"="monitoring_station"]["monitoring:water_level"="yes"](area.jp);',
      'node["man_made"="water_well"](area.jp);',
      'node["waterway"="water_point"](area.jp);',
    ].join(''),
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        station_id: `OSM_${el.id}`,
        station_name: el.tags?.['name:en'] || el.tags?.name || 'River gauge',
        operator: el.tags?.operator || null,
        ref: el.tags?.ref || null,
        source: 'osm_overpass',
      },
    }),
    60_000,
    { limit: 0, queryTimeout: 120 },
  );
}

export default async function collectMlitRiver() {
  let features = await tryMlitJson();
  let live = !!(features && features.length);
  let liveSrc = live ? 'mlit_river_live' : null;

  if (!live) {
    features = await tryOsmGauges();
    live = !!(features && features.length);
    if (live) liveSrc = 'osm_water_monitoring';
  }
  if (!live) features = generateSeedData();

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: live ? liveSrc : 'mlit_seed',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'River water level monitoring stations across Japan',
    },
    metadata: {},
  };
}
