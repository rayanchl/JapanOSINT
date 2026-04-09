/**
 * JSDF Bases Collector
 * Japan Self-Defense Forces installations across GSDF / MSDF / ASDF.
 * OSM Overpass landuse=military operator=自衛隊 with comprehensive seed fallback.
 */

const OVERPASS_URL = 'https://overpass-api.de/api/interpreter';

const SEED_JSDF = [
  // GSDF (Ground SDF) - Northern Army
  { name: '陸自 真駒内駐屯地', lat: 42.9856, lon: 141.3486, branch: 'GSDF', role: 'army_hq', region: 'Hokkaido' },
  { name: '陸自 千歳駐屯地', lat: 42.7806, lon: 141.6753, branch: 'GSDF', role: 'division', region: 'Hokkaido' },
  { name: '陸自 旭川駐屯地', lat: 43.7611, lon: 142.3644, branch: 'GSDF', role: 'division', region: 'Hokkaido' },
  { name: '陸自 帯広駐屯地', lat: 42.9219, lon: 143.1958, branch: 'GSDF', role: 'brigade', region: 'Hokkaido' },
  { name: '陸自 北恵庭駐屯地', lat: 42.8400, lon: 141.6014, branch: 'GSDF', role: 'tank', region: 'Hokkaido' },
  { name: '陸自 東千歳駐屯地', lat: 42.7869, lon: 141.6892, branch: 'GSDF', role: 'tank', region: 'Hokkaido' },
  { name: '陸自 名寄駐屯地', lat: 44.3567, lon: 142.4636, branch: 'GSDF', role: 'regiment', region: 'Hokkaido' },
  { name: '陸自 美幌駐屯地', lat: 43.8222, lon: 144.1239, branch: 'GSDF', role: 'regiment', region: 'Hokkaido' },
  // GSDF Northeastern Army
  { name: '陸自 仙台駐屯地', lat: 38.2611, lon: 140.8689, branch: 'GSDF', role: 'army_hq', region: 'Tohoku' },
  { name: '陸自 多賀城駐屯地', lat: 38.2956, lon: 140.9928, branch: 'GSDF', role: 'division', region: 'Tohoku' },
  { name: '陸自 福島駐屯地', lat: 37.7197, lon: 140.4639, branch: 'GSDF', role: 'regiment', region: 'Tohoku' },
  { name: '陸自 神町駐屯地', lat: 38.4083, lon: 140.4078, branch: 'GSDF', role: 'division', region: 'Tohoku' },
  { name: '陸自 弘前駐屯地', lat: 40.6147, lon: 140.4789, branch: 'GSDF', role: 'regiment', region: 'Tohoku' },
  { name: '陸自 八戸駐屯地', lat: 40.5239, lon: 141.4906, branch: 'GSDF', role: 'aviation', region: 'Tohoku' },
  // GSDF Eastern Army
  { name: '陸自 朝霞駐屯地', lat: 35.7717, lon: 139.5839, branch: 'GSDF', role: 'army_hq', region: 'Kanto' },
  { name: '陸自 練馬駐屯地', lat: 35.7531, lon: 139.6483, branch: 'GSDF', role: 'division', region: 'Kanto' },
  { name: '陸自 市ヶ谷駐屯地', lat: 35.6926, lon: 139.7270, branch: 'GSDF', role: 'mod_hq', region: 'Kanto' },
  { name: '陸自 習志野駐屯地', lat: 35.6792, lon: 140.0306, branch: 'GSDF', role: 'airborne', region: 'Kanto' },
  { name: '陸自 木更津駐屯地', lat: 35.4022, lon: 139.9094, branch: 'GSDF', role: 'aviation', region: 'Kanto' },
  { name: '陸自 大宮駐屯地', lat: 35.9078, lon: 139.6308, branch: 'GSDF', role: 'csbrn', region: 'Kanto' },
  { name: '陸自 宇都宮駐屯地', lat: 36.5550, lon: 139.8814, branch: 'GSDF', role: 'aviation', region: 'Kanto' },
  { name: '陸自 古河駐屯地', lat: 36.1986, lon: 139.7008, branch: 'GSDF', role: 'engineer', region: 'Kanto' },
  // GSDF Central Army
  { name: '陸自 伊丹駐屯地', lat: 34.7836, lon: 135.4119, branch: 'GSDF', role: 'army_hq', region: 'Kansai' },
  { name: '陸自 信太山駐屯地', lat: 34.5078, lon: 135.4722, branch: 'GSDF', role: 'regiment', region: 'Kansai' },
  { name: '陸自 千僧駐屯地', lat: 34.7828, lon: 135.3942, branch: 'GSDF', role: 'division', region: 'Kansai' },
  { name: '陸自 米子駐屯地', lat: 35.4528, lon: 133.3225, branch: 'GSDF', role: 'regiment', region: 'Chugoku' },
  { name: '陸自 善通寺駐屯地', lat: 34.2228, lon: 133.7858, branch: 'GSDF', role: 'brigade', region: 'Shikoku' },
  { name: '陸自 松山駐屯地', lat: 33.8233, lon: 132.7700, branch: 'GSDF', role: 'regiment', region: 'Shikoku' },
  // GSDF Western Army
  { name: '陸自 健軍駐屯地', lat: 32.7783, lon: 130.7597, branch: 'GSDF', role: 'army_hq', region: 'Kyushu' },
  { name: '陸自 北熊本駐屯地', lat: 32.8633, lon: 130.7142, branch: 'GSDF', role: 'division', region: 'Kyushu' },
  { name: '陸自 小郡駐屯地', lat: 33.4044, lon: 130.5650, branch: 'GSDF', role: 'engineer', region: 'Kyushu' },
  { name: '陸自 福岡駐屯地', lat: 33.5894, lon: 130.4500, branch: 'GSDF', role: 'logistics', region: 'Kyushu' },
  { name: '陸自 久留米駐屯地', lat: 33.3158, lon: 130.5092, branch: 'GSDF', role: 'regiment', region: 'Kyushu' },
  { name: '陸自 日出生台演習場', lat: 33.2806, lon: 131.3500, branch: 'GSDF', role: 'training_range', region: 'Kyushu' },
  { name: '陸自 那覇駐屯地', lat: 26.1864, lon: 127.6481, branch: 'GSDF', role: 'brigade', region: 'Okinawa' },
  { name: '陸自 与那国駐屯地', lat: 24.4572, lon: 122.9858, branch: 'GSDF', role: 'coastal', region: 'Okinawa' },
  { name: '陸自 宮古島駐屯地', lat: 24.7800, lon: 125.3000, branch: 'GSDF', role: 'coastal', region: 'Okinawa' },
  { name: '陸自 石垣駐屯地', lat: 24.4339, lon: 124.1647, branch: 'GSDF', role: 'coastal', region: 'Okinawa' },
  // MSDF (Maritime SDF) - main fleet bases
  { name: '海自 横須賀基地', lat: 35.2917, lon: 139.6611, branch: 'MSDF', role: 'fleet_hq', region: 'Kanto' },
  { name: '海自 厚木航空基地', lat: 35.4544, lon: 139.4500, branch: 'MSDF', role: 'air_station', region: 'Kanto' },
  { name: '海自 館山航空基地', lat: 34.9886, lon: 139.8475, branch: 'MSDF', role: 'air_station', region: 'Kanto' },
  { name: '海自 下総航空基地', lat: 35.7906, lon: 140.0164, branch: 'MSDF', role: 'air_station', region: 'Kanto' },
  { name: '海自 木更津航空基地', lat: 35.4022, lon: 139.9094, branch: 'MSDF', role: 'air_station', region: 'Kanto' },
  { name: '海自 大湊基地', lat: 41.2403, lon: 141.1325, branch: 'MSDF', role: 'fleet', region: 'Tohoku' },
  { name: '海自 八戸航空基地', lat: 40.5567, lon: 141.4647, branch: 'MSDF', role: 'air_station', region: 'Tohoku' },
  { name: '海自 舞鶴基地', lat: 35.4833, lon: 135.3833, branch: 'MSDF', role: 'fleet', region: 'Kansai' },
  { name: '海自 呉基地', lat: 34.2356, lon: 132.5572, branch: 'MSDF', role: 'fleet_hq', region: 'Chugoku' },
  { name: '海自 岩国航空基地', lat: 34.1442, lon: 132.2356, branch: 'MSDF', role: 'air_station', region: 'Chugoku' },
  { name: '海自 佐世保基地', lat: 33.1592, lon: 129.7222, branch: 'MSDF', role: 'fleet', region: 'Kyushu' },
  { name: '海自 鹿屋航空基地', lat: 31.3700, lon: 130.8453, branch: 'MSDF', role: 'air_station', region: 'Kyushu' },
  { name: '海自 那覇航空基地', lat: 26.1958, lon: 127.6458, branch: 'MSDF', role: 'air_station', region: 'Okinawa' },
  { name: '海自 沖縄基地', lat: 26.1964, lon: 127.6519, branch: 'MSDF', role: 'fleet', region: 'Okinawa' },
  // ASDF (Air SDF)
  { name: '空自 横田基地（共同）', lat: 35.7486, lon: 139.3486, branch: 'ASDF', role: 'air_hq', region: 'Kanto' },
  { name: '空自 入間基地', lat: 35.8417, lon: 139.4111, branch: 'ASDF', role: 'air_base', region: 'Kanto' },
  { name: '空自 百里基地', lat: 36.1811, lon: 140.4147, branch: 'ASDF', role: 'fighter', region: 'Kanto' },
  { name: '空自 千歳基地', lat: 42.7944, lon: 141.6694, branch: 'ASDF', role: 'fighter', region: 'Hokkaido' },
  { name: '空自 三沢基地', lat: 40.7028, lon: 141.3681, branch: 'ASDF', role: 'fighter', region: 'Tohoku' },
  { name: '空自 松島基地', lat: 38.4042, lon: 141.2200, branch: 'ASDF', role: 'training', region: 'Tohoku' },
  { name: '空自 小松基地', lat: 36.3950, lon: 136.4063, branch: 'ASDF', role: 'fighter', region: 'Hokuriku' },
  { name: '空自 岐阜基地', lat: 35.3942, lon: 136.8694, branch: 'ASDF', role: 'test', region: 'Chubu' },
  { name: '空自 浜松基地', lat: 34.7503, lon: 137.7028, branch: 'ASDF', role: 'training', region: 'Chubu' },
  { name: '空自 美保基地', lat: 35.4922, lon: 133.2367, branch: 'ASDF', role: 'transport', region: 'Chugoku' },
  { name: '空自 防府北基地', lat: 34.0367, lon: 131.5497, branch: 'ASDF', role: 'training', region: 'Chugoku' },
  { name: '空自 春日基地', lat: 33.5408, lon: 130.4528, branch: 'ASDF', role: 'air_hq', region: 'Kyushu' },
  { name: '空自 築城基地', lat: 33.6864, lon: 131.0392, branch: 'ASDF', role: 'fighter', region: 'Kyushu' },
  { name: '空自 新田原基地', lat: 32.0833, lon: 131.4500, branch: 'ASDF', role: 'fighter', region: 'Kyushu' },
  { name: '空自 那覇基地', lat: 26.1958, lon: 127.6458, branch: 'ASDF', role: 'fighter', region: 'Okinawa' },
];

async function tryOverpass() {
  const query = `[out:json][timeout:25];area["ISO3166-1"="JP"]->.jp;(way["landuse"="military"](area.jp);relation["landuse"="military"](area.jp););out center 200;`;
  try {
    const ctrl = new AbortController();
    const timeout = setTimeout(() => ctrl.abort(), 12000);
    const res = await fetch(OVERPASS_URL, {
      method: 'POST',
      signal: ctrl.signal,
      headers: { 'Content-Type': 'text/plain' },
      body: query,
    });
    clearTimeout(timeout);
    if (!res.ok) return null;
    const data = await res.json();
    if (!data?.elements?.length) return null;
    return data.elements
      .map((el) => {
        const lat = el.center?.lat ?? el.lat;
        const lon = el.center?.lon ?? el.lon;
        if (lat == null || lon == null) return null;
        const op = (el.tags?.operator || '').toLowerCase();
        if (op && !/自衛|jsdf|jasdf|jgsdf|jmsdf/.test(op)) return null;
        return {
          type: 'Feature',
          geometry: { type: 'Point', coordinates: [lon, lat] },
          properties: {
            base_id: `OSM_${el.id}`,
            name: el.tags?.name || 'JSDF Base',
            branch: 'JSDF',
            source: 'osm_overpass',
          },
        };
      })
      .filter(Boolean);
  } catch {
    return null;
  }
}

function generateSeedData() {
  return SEED_JSDF.map((b, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [b.lon, b.lat] },
    properties: {
      base_id: `JSDF_${String(i + 1).padStart(5, '0')}`,
      name: b.name,
      branch: b.branch,
      role: b.role,
      region: b.region,
      country: 'JP',
      source: 'jsdf_seed',
    },
  }));
}

export default async function collectJsdfBases() {
  let features = await tryOverpass();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'jsdf_bases',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Japan Self-Defense Forces installations: GSDF, MSDF, ASDF',
    },
    metadata: {},
  };
}
