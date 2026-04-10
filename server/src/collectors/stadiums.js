/**
 * Stadiums Collector
 * J-League, NPB, Sumo, and major stadiums across Japan.
 */

const OVERPASS_URL = 'https://overpass-api.de/api/interpreter';

const SEED_STADIUMS = [
  // NPB baseball
  { name: '東京ドーム', lat: 35.7056, lon: 139.7519, kind: 'baseball', capacity: 46000, team: '読売ジャイアンツ' },
  { name: '明治神宮野球場', lat: 35.6742, lon: 139.7172, kind: 'baseball', capacity: 33556, team: 'ヤクルトスワローズ' },
  { name: '横浜スタジアム', lat: 35.4433, lon: 139.6400, kind: 'baseball', capacity: 34046, team: '横浜DeNAベイスターズ' },
  { name: 'バンテリンドーム ナゴヤ', lat: 35.1858, lon: 136.9475, kind: 'baseball', capacity: 36370, team: '中日ドラゴンズ' },
  { name: '阪神甲子園球場', lat: 34.7213, lon: 135.3616, kind: 'baseball', capacity: 47808, team: '阪神タイガース' },
  { name: '京セラドーム大阪', lat: 34.6694, lon: 135.4761, kind: 'baseball', capacity: 36220, team: 'オリックスバファローズ' },
  { name: 'MAZDA Zoom-Zoom スタジアム広島', lat: 34.3917, lon: 132.4842, kind: 'baseball', capacity: 33000, team: '広島東洋カープ' },
  { name: 'みずほPayPayドーム福岡', lat: 33.5953, lon: 130.3622, kind: 'baseball', capacity: 40062, team: 'ソフトバンクホークス' },
  { name: '楽天モバイルパーク宮城', lat: 38.2564, lon: 140.9028, kind: 'baseball', capacity: 30508, team: '楽天イーグルス' },
  { name: 'ベルーナドーム', lat: 35.7731, lon: 139.4172, kind: 'baseball', capacity: 31552, team: '西武ライオンズ' },
  { name: 'ZOZOマリンスタジアム', lat: 35.6456, lon: 140.0306, kind: 'baseball', capacity: 30348, team: '千葉ロッテマリーンズ' },
  { name: 'エスコンフィールドHOKKAIDO', lat: 42.9928, lon: 141.4092, kind: 'baseball', capacity: 35000, team: '北海道日本ハムファイターズ' },
  // J-League football
  { name: '国立競技場', lat: 35.6778, lon: 139.7147, kind: 'football', capacity: 68000, team: '代表' },
  { name: '日産スタジアム', lat: 35.5097, lon: 139.6064, kind: 'football', capacity: 72327, team: '横浜F・マリノス' },
  { name: '埼玉スタジアム2002', lat: 35.9036, lon: 139.7172, kind: 'football', capacity: 63700, team: '浦和レッズ' },
  { name: '味の素スタジアム', lat: 35.6642, lon: 139.5272, kind: 'football', capacity: 49970, team: 'FC東京' },
  { name: 'パナソニックスタジアム吹田', lat: 34.8039, lon: 135.5389, kind: 'football', capacity: 40000, team: 'ガンバ大阪' },
  { name: 'ヨドコウ桜スタジアム', lat: 34.6094, lon: 135.5181, kind: 'football', capacity: 24481, team: 'セレッソ大阪' },
  { name: 'IAIスタジアム日本平', lat: 34.9706, lon: 138.4236, kind: 'football', capacity: 20248, team: '清水エスパルス' },
  { name: 'ノエビアスタジアム神戸', lat: 34.6572, lon: 135.1700, kind: 'football', capacity: 30132, team: 'ヴィッセル神戸' },
  { name: 'デンカビッグスワンスタジアム', lat: 37.8886, lon: 139.0803, kind: 'football', capacity: 42300, team: 'アルビレックス新潟' },
  { name: 'エディオンピースウイング広島', lat: 34.3981, lon: 132.4583, kind: 'football', capacity: 28520, team: 'サンフレッチェ広島' },
  { name: 'ベスト電器スタジアム', lat: 33.5669, lon: 130.4528, kind: 'football', capacity: 22563, team: 'アビスパ福岡' },
  { name: 'パロマ瑞穂スタジアム', lat: 35.1319, lon: 136.9472, kind: 'football', capacity: 27000, team: '名古屋グランパス' },
  { name: 'サンガスタジアム by KYOCERA', lat: 35.0147, lon: 135.5589, kind: 'football', capacity: 21600, team: '京都サンガ' },
  // Sumo
  { name: '両国国技館', lat: 35.6967, lon: 139.7933, kind: 'sumo', capacity: 11098, team: '日本相撲協会' },
  { name: '大阪府立体育会館', lat: 34.6658, lon: 135.5011, kind: 'sumo', capacity: 8000, team: '大阪場所' },
  { name: '愛知県体育館', lat: 35.1850, lon: 136.9014, kind: 'sumo', capacity: 7000, team: '名古屋場所' },
  { name: '福岡国際センター', lat: 33.6022, lon: 130.4056, kind: 'sumo', capacity: 8000, team: '九州場所' },
  // Rugby / Multi
  { name: '秩父宮ラグビー場', lat: 35.6744, lon: 139.7172, kind: 'rugby', capacity: 24871, team: '日本代表' },
  { name: '花園ラグビー場', lat: 34.6694, lon: 135.6408, kind: 'rugby', capacity: 27000, team: '東大阪市' },
  { name: '長居スタジアム', lat: 34.6117, lon: 135.5175, kind: 'multi', capacity: 47000, team: '大阪市' },
  { name: '札幌ドーム', lat: 43.0156, lon: 141.4097, kind: 'multi', capacity: 41484, team: '札幌市' },
  { name: '国立代々木競技場', lat: 35.6683, lon: 139.6983, kind: 'indoor', capacity: 13291, team: '代々木' },
  { name: 'さいたまスーパーアリーナ', lat: 35.8947, lon: 139.6306, kind: 'indoor', capacity: 37000, team: 'さいたま市' },
  { name: '大阪城ホール', lat: 34.6878, lon: 135.5333, kind: 'indoor', capacity: 16000, team: '大阪市' },
];

async function tryOSMOverpass() {
  try {
    const ctrl = new AbortController();
    const timeout = setTimeout(() => ctrl.abort(), 10000);
    const query = `[out:json][timeout:25];area["ISO3166-1"="JP"][admin_level=2];(node["leisure"="stadium"](area););out center 200;`;
    const res = await fetch(OVERPASS_URL, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: 'data=' + encodeURIComponent(query),
      signal: ctrl.signal,
    });
    clearTimeout(timeout);
    if (!res.ok) return null;
    const data = await res.json();
    return (data.elements || []).slice(0, 200).map((el, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [el.lon, el.lat] },
      properties: {
        stadium_id: `OSM_${el.id}`,
        name: el.tags?.name || `Stadium ${i + 1}`,
        kind: el.tags?.sport || 'multi',
        capacity: parseInt(el.tags?.capacity) || null,
        country: 'JP',
        source: 'osm_overpass',
      },
    }));
  } catch { return null; }
}

function generateSeedData() {
  return SEED_STADIUMS.map((s, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [s.lon, s.lat] },
    properties: {
      stadium_id: `STAD_${String(i + 1).padStart(5, '0')}`,
      name: s.name,
      kind: s.kind,
      capacity: s.capacity,
      team: s.team,
      country: 'JP',
      source: 'stadium_seed',
    },
  }));
}

export default async function collectStadiums() {
  let features = await tryOSMOverpass();
  if (!features || features.length === 0) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'stadiums',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: features?.[0]?.properties?.source === 'osm_overpass',
      description: 'J-League football, NPB baseball, sumo, rugby, indoor arenas',
    },
    metadata: {},
  };
}
