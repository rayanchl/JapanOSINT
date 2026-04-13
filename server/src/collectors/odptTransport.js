/**
 * ODPT Transport Collector
 * Open Data for Public Transportation Japan - nationwide station coverage.
 *
 * Authentication: set ODPT_TOKEN (or ODPT_CHALLENGE_TOKEN) in env to unlock
 * the full `odpt:Station` endpoint (~10,000 stations across all operators).
 * Tokens are free from https://developer.odpt.org/ (Challenge) or
 * https://api-challenge.odpt.org/.
 *
 * Without a token we fall back to curated seed data (major Tokyo/Kansai
 * stations) so the layer still renders in dev environments.
 */

import { fetchOverpass } from './_liveHelpers.js';

const API_BASE = 'https://api.odpt.org/api/v4/';
const CHALLENGE_API_BASE = 'https://api-challenge.odpt.org/api/v4/';
const TIMEOUT_MS = 30000;

function getOdptToken() {
  return process.env.ODPT_TOKEN
    || process.env.ODPT_CONSUMER_KEY
    || process.env.ODPT_CHALLENGE_TOKEN
    || null;
}

const STATIONS = [
  // JR Yamanote Line
  { name: '東京', line: 'JR山手線', lat: 35.6812, lon: 139.7671, passengers: 462000 },
  { name: '有楽町', line: 'JR山手線', lat: 35.6748, lon: 139.7630, passengers: 170000 },
  { name: '新橋', line: 'JR山手線', lat: 35.6660, lon: 139.7583, passengers: 275000 },
  { name: '浜松町', line: 'JR山手線', lat: 35.6554, lon: 139.7571, passengers: 157000 },
  { name: '田町', line: 'JR山手線', lat: 35.6459, lon: 139.7475, passengers: 152000 },
  { name: '品川', line: 'JR山手線', lat: 35.6285, lon: 139.7388, passengers: 378000 },
  { name: '大崎', line: 'JR山手線', lat: 35.6197, lon: 139.7284, passengers: 130000 },
  { name: '五反田', line: 'JR山手線', lat: 35.6262, lon: 139.7235, passengers: 130000 },
  { name: '目黒', line: 'JR山手線', lat: 35.6338, lon: 139.7158, passengers: 112000 },
  { name: '恵比寿', line: 'JR山手線', lat: 35.6467, lon: 139.7101, passengers: 136000 },
  { name: '渋谷', line: 'JR山手線', lat: 35.6580, lon: 139.7016, passengers: 366000 },
  { name: '原宿', line: 'JR山手線', lat: 35.6702, lon: 139.7027, passengers: 75000 },
  { name: '代々木', line: 'JR山手線', lat: 35.6834, lon: 139.7020, passengers: 67000 },
  { name: '新宿', line: 'JR山手線', lat: 35.6896, lon: 139.7006, passengers: 775000 },
  { name: '新大久保', line: 'JR山手線', lat: 35.7012, lon: 139.7001, passengers: 53000 },
  { name: '高田馬場', line: 'JR山手線', lat: 35.7127, lon: 139.7038, passengers: 208000 },
  { name: '目白', line: 'JR山手線', lat: 35.7211, lon: 139.7068, passengers: 37000 },
  { name: '池袋', line: 'JR山手線', lat: 35.7295, lon: 139.7109, passengers: 558000 },
  { name: '大塚', line: 'JR山手線', lat: 35.7319, lon: 139.7286, passengers: 52000 },
  { name: '巣鴨', line: 'JR山手線', lat: 35.7334, lon: 139.7393, passengers: 56000 },
  { name: '駒込', line: 'JR山手線', lat: 35.7368, lon: 139.7470, passengers: 47000 },
  { name: '田端', line: 'JR山手線', lat: 35.7381, lon: 139.7609, passengers: 46000 },
  { name: '西日暮里', line: 'JR山手線', lat: 35.7321, lon: 139.7668, passengers: 51000 },
  { name: '日暮里', line: 'JR山手線', lat: 35.7280, lon: 139.7710, passengers: 106000 },
  { name: '鶯谷', line: 'JR山手線', lat: 35.7210, lon: 139.7780, passengers: 25000 },
  { name: '上野', line: 'JR山手線', lat: 35.7141, lon: 139.7774, passengers: 187000 },
  { name: '御徒町', line: 'JR山手線', lat: 35.7075, lon: 139.7748, passengers: 68000 },
  { name: '秋葉原', line: 'JR山手線', lat: 35.6984, lon: 139.7731, passengers: 246000 },
  { name: '神田', line: 'JR山手線', lat: 35.6917, lon: 139.7709, passengers: 108000 },
  // Tokyo Metro key stations
  { name: '銀座', line: '東京メトロ銀座線', lat: 35.6717, lon: 139.7637, passengers: 235000 },
  { name: '表参道', line: '東京メトロ銀座線', lat: 35.6654, lon: 139.7122, passengers: 182000 },
  { name: '赤坂見附', line: '東京メトロ銀座線', lat: 35.6770, lon: 139.7371, passengers: 95000 },
  { name: '溜池山王', line: '東京メトロ銀座線', lat: 35.6739, lon: 139.7415, passengers: 120000 },
  { name: '大手町', line: '東京メトロ丸ノ内線', lat: 35.6860, lon: 139.7636, passengers: 330000 },
  { name: '霞ケ関', line: '東京メトロ丸ノ内線', lat: 35.6733, lon: 139.7502, passengers: 145000 },
  { name: '六本木', line: '東京メトロ日比谷線', lat: 35.6626, lon: 139.7315, passengers: 112000 },
  { name: '中目黒', line: '東京メトロ日比谷線', lat: 35.6443, lon: 139.6989, passengers: 105000 },
  { name: '北千住', line: '東京メトロ日比谷線', lat: 35.7497, lon: 139.8049, passengers: 210000 },
  { name: '飯田橋', line: '東京メトロ東西線', lat: 35.7020, lon: 139.7452, passengers: 152000 },
  { name: '九段下', line: '東京メトロ東西線', lat: 35.6952, lon: 139.7511, passengers: 120000 },
  { name: '日本橋', line: '東京メトロ東西線', lat: 35.6818, lon: 139.7748, passengers: 135000 },
  { name: '門前仲町', line: '東京メトロ東西線', lat: 35.6726, lon: 139.7963, passengers: 98000 },
  { name: '豊洲', line: '東京メトロ有楽町線', lat: 35.6535, lon: 139.7965, passengers: 125000 },
  { name: '月島', line: '東京メトロ有楽町線', lat: 35.6636, lon: 139.7866, passengers: 55000 },
  { name: '永田町', line: '東京メトロ有楽町線', lat: 35.6784, lon: 139.7389, passengers: 75000 },
  { name: '護国寺', line: '東京メトロ有楽町線', lat: 35.7169, lon: 139.7270, passengers: 42000 },
  // Toei Subway
  { name: '新宿三丁目', line: '都営新宿線', lat: 35.6910, lon: 139.7045, passengers: 175000 },
  { name: '馬喰横山', line: '都営新宿線', lat: 35.6929, lon: 139.7834, passengers: 55000 },
  { name: '神保町', line: '都営三田線', lat: 35.6958, lon: 139.7577, passengers: 88000 },
  { name: '三田', line: '都営三田線', lat: 35.6487, lon: 139.7467, passengers: 62000 },
  { name: '大門', line: '都営大江戸線', lat: 35.6557, lon: 139.7555, passengers: 85000 },
  { name: '青山一丁目', line: '都営大江戸線', lat: 35.6726, lon: 139.7244, passengers: 75000 },
  { name: '汐留', line: '都営大江戸線', lat: 35.6609, lon: 139.7621, passengers: 65000 },
  { name: '築地市場', line: '都営大江戸線', lat: 35.6622, lon: 139.7698, passengers: 45000 },
  // Major JR stations outside Yamanote
  { name: '吉祥寺', line: 'JR中央線', lat: 35.7030, lon: 139.5803, passengers: 142000 },
  { name: '立川', line: 'JR中央線', lat: 35.6980, lon: 139.4137, passengers: 160000 },
  { name: '八王子', line: 'JR中央線', lat: 35.6558, lon: 139.3388, passengers: 85000 },
  { name: '町田', line: 'JR横浜線', lat: 35.5423, lon: 139.4466, passengers: 110000 },
  { name: '武蔵小杉', line: 'JR横須賀線', lat: 35.5763, lon: 139.6597, passengers: 120000 },
  { name: '川崎', line: 'JR東海道線', lat: 35.5308, lon: 139.6992, passengers: 210000 },
  { name: '横浜', line: 'JR東海道線', lat: 35.4658, lon: 139.6225, passengers: 420000 },
  // Other major stations
  { name: '大宮', line: 'JR京浜東北線', lat: 35.9062, lon: 139.6237, passengers: 260000 },
  { name: '柏', line: 'JR常磐線', lat: 35.8618, lon: 139.9751, passengers: 125000 },
  { name: '船橋', line: 'JR総武線', lat: 35.7017, lon: 139.9852, passengers: 140000 },
  { name: '千葉', line: 'JR総武線', lat: 35.6131, lon: 140.1134, passengers: 105000 },
  // Private railways
  { name: '二子玉川', line: '東急田園都市線', lat: 35.6116, lon: 139.6264, passengers: 92000 },
  { name: '自由が丘', line: '東急東横線', lat: 35.6077, lon: 139.6688, passengers: 95000 },
  { name: '下北沢', line: '小田急小田原線', lat: 35.6612, lon: 139.6677, passengers: 125000 },
  { name: '登戸', line: '小田急小田原線', lat: 35.6160, lon: 139.5666, passengers: 85000 },
  { name: '所沢', line: '西武池袋線', lat: 35.7878, lon: 139.4691, passengers: 100000 },
  { name: '練馬', line: '西武池袋線', lat: 35.7375, lon: 139.6541, passengers: 65000 },
  { name: '押上', line: '東京メトロ半蔵門線', lat: 35.7108, lon: 139.8133, passengers: 85000 },
  // Kansai area
  { name: '大阪/梅田', line: 'JR大阪環状線', lat: 34.7024, lon: 135.4959, passengers: 430000 },
  { name: '天王寺', line: 'JR大阪環状線', lat: 34.6466, lon: 135.5170, passengers: 155000 },
  { name: '難波', line: '南海本線', lat: 34.6625, lon: 135.5008, passengers: 250000 },
  { name: '京都', line: 'JR東海道線', lat: 34.9858, lon: 135.7588, passengers: 200000 },
  { name: '三ノ宮', line: 'JR東海道線', lat: 34.6937, lon: 135.1953, passengers: 125000 },
  { name: '新大阪', line: 'JR東海道新幹線', lat: 34.7334, lon: 135.5001, passengers: 230000 },
  // Other major cities
  { name: '名古屋', line: 'JR東海道新幹線', lat: 35.1709, lon: 136.8815, passengers: 405000 },
  { name: '博多', line: 'JR山陽新幹線', lat: 33.5897, lon: 130.4207, passengers: 140000 },
  { name: '仙台', line: 'JR東北新幹線', lat: 38.2601, lon: 140.8822, passengers: 90000 },
  { name: '札幌', line: 'JR函館本線', lat: 43.0687, lon: 141.3508, passengers: 95000 },
  { name: '広島', line: 'JR山陽新幹線', lat: 34.3981, lon: 132.4753, passengers: 75000 },
  { name: '岡山', line: 'JR山陽新幹線', lat: 34.6655, lon: 133.9184, passengers: 65000 },
  { name: '新横浜', line: 'JR東海道新幹線', lat: 35.5067, lon: 139.6179, passengers: 125000 },
  // Airports
  { name: '成田空港', line: 'JR成田エクスプレス', lat: 35.7720, lon: 140.3929, passengers: 45000 },
  { name: '羽田空港第1ターミナル', line: '東京モノレール', lat: 35.5494, lon: 139.7836, passengers: 95000 },
  { name: '関西空港', line: 'JR関空快速', lat: 34.4320, lon: 135.2304, passengers: 35000 },
  // More Tokyo Metro
  { name: '後楽園', line: '東京メトロ丸ノ内線', lat: 35.7081, lon: 139.7523, passengers: 92000 },
  { name: '茗荷谷', line: '東京メトロ丸ノ内線', lat: 35.7177, lon: 139.7342, passengers: 45000 },
  { name: '四ツ谷', line: '東京メトロ丸ノ内線', lat: 35.6862, lon: 139.7309, passengers: 78000 },
  { name: '市ケ谷', line: '東京メトロ有楽町線', lat: 35.6928, lon: 139.7355, passengers: 72000 },
  { name: '麻布十番', line: '東京メトロ南北線', lat: 35.6547, lon: 139.7374, passengers: 55000 },
  { name: '白金高輪', line: '東京メトロ南北線', lat: 35.6433, lon: 139.7337, passengers: 48000 },
  { name: '清澄白河', line: '東京メトロ半蔵門線', lat: 35.6811, lon: 139.8014, passengers: 42000 },
  { name: '錦糸町', line: 'JR総武線', lat: 35.6960, lon: 139.8150, passengers: 105000 },
  { name: '亀戸', line: 'JR総武線', lat: 35.6974, lon: 139.8264, passengers: 45000 },
  { name: '西船橋', line: 'JR総武線', lat: 35.7184, lon: 139.9554, passengers: 135000 },
];

function generateSeedData() {
  const now = new Date();
  return STATIONS.map((st, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [st.lon, st.lat] },
    properties: {
      station_id: `ODPT_${String(i + 1).padStart(3, '0')}`,
      station_name: st.name,
      line_name: st.line,
      daily_passengers: st.passengers,
      operator: st.line.startsWith('JR') ? 'JR' :
        st.line.includes('東京メトロ') ? '東京メトロ' :
        st.line.includes('都営') ? '都営地下鉄' :
        st.line.includes('東急') ? '東急電鉄' :
        st.line.includes('小田急') ? '小田急電鉄' :
        st.line.includes('西武') ? '西武鉄道' :
        st.line.includes('南海') ? '南海電鉄' :
        st.line.includes('モノレール') ? '東京モノレール' : 'その他',
      wheelchair_accessible: true,
      measured_at: now.toISOString(),
      source: 'odpt_seed',
    },
  }));
}

function mapOdptStation(d) {
  const lat = d['geo:lat'] ?? d.geo_lat;
  const lon = d['geo:long'] ?? d.geo_long;
  if (lat == null || lon == null) return null;
  const titleJa = d['odpt:stationTitle']?.ja ?? d['dc:title'] ?? null;
  const titleEn = d['odpt:stationTitle']?.en ?? null;
  const railway = d['odpt:railway'] ?? null;
  const operator = d['odpt:operator'] ?? null;
  return {
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [+lon, +lat] },
    properties: {
      station_id: d['owl:sameAs'] ?? d['@id'] ?? null,
      station_name: titleEn || titleJa,
      station_name_ja: titleJa,
      line_name: railway,
      operator: operator ? String(operator).replace(/^odpt\.Operator:/, '') : null,
      station_code: d['odpt:stationCode'] ?? null,
      connecting_railways: d['odpt:connectingRailway'] ?? null,
      source: 'odpt_live',
    },
  };
}

async function fetchAllOdptStations(token) {
  // Try production endpoint first, then challenge endpoint.
  const bases = [API_BASE, CHALLENGE_API_BASE];
  for (const base of bases) {
    try {
      const controller = new AbortController();
      const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
      const url = `${base}odpt:Station?acl:consumerKey=${encodeURIComponent(token)}`;
      const res = await fetch(url, {
        signal: controller.signal,
        headers: { Accept: 'application/json', 'User-Agent': 'JapanOSINT/1.0' },
      });
      clearTimeout(timer);
      if (!res.ok) continue;
      const data = await res.json();
      if (!Array.isArray(data) || data.length === 0) continue;
      const features = data.map(mapOdptStation).filter(Boolean);
      if (features.length > 0) return { features, endpoint: base };
    } catch { /* try next */ }
  }
  return null;
}

async function fetchOsmStations() {
  // Public-domain fallback when no ODPT token: pull every OSM rail station
  // tagged `railway=station|halt|tram_stop` across Japan via Overpass.
  return fetchOverpass(
    [
      'node["railway"="station"](area.jp);',
      'node["railway"="halt"](area.jp);',
      'node["railway"="tram_stop"](area.jp);',
      'node["public_transport"="station"]["train"="yes"](area.jp);',
      'node["public_transport"="station"]["subway"="yes"](area.jp);',
    ].join(''),
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        station_id: `OSM_${el.id}`,
        station_name: el.tags?.['name:en'] || el.tags?.name || 'Station',
        station_name_ja: el.tags?.name || null,
        line_name: el.tags?.line || el.tags?.network || null,
        operator: el.tags?.operator || null,
        railway: el.tags?.railway || el.tags?.public_transport || null,
        wheelchair: el.tags?.wheelchair || null,
        source: 'osm_overpass',
      },
    }),
    60_000,
    { limit: 0, queryTimeout: 180 },
  );
}

export default async function collectOdptTransport() {
  const token = getOdptToken();
  let features = [];
  let source = 'odpt_seed';
  let endpoint = null;

  if (token) {
    const result = await fetchAllOdptStations(token);
    if (result) {
      features = result.features;
      endpoint = result.endpoint;
      source = 'odpt_live';
    }
  }

  if (features.length === 0) {
    // Fall back to OSM nationwide rail-station inventory (no token, no rate cap).
    const osm = await fetchOsmStations();
    if (osm && osm.length > 0) {
      features = osm;
      source = 'osm_overpass_railway_station';
    }
  }

  if (features.length === 0) {
    features = generateSeedData();
    source = token ? 'odpt_seed_fallback' : 'odpt_seed_no_token';
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      endpoint,
      has_token: !!token,
      description: 'Public transportation station data from ODPT (Open Data for Public Transportation Japan)',
    },
    metadata: {},
  };
}
