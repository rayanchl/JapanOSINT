/**
 * e-Stat Census Collector
 * Fetches mesh-based demographic data from Japan's e-Stat API.
 * Falls back to a curated seed of major prefecture-level census points.
 */

import { fetchOverpass } from './_liveHelpers.js';

const ESTAT_APP_ID = process.env.ESTAT_APP_ID || '';
const ESTAT_URL = 'https://api.e-stat.go.jp/rest/3.0/app/json/getStatsData';

// Each row carries its JIS X 0401 prefecture code (used as "@area" key by the
// e-Stat API) so that remote values can be joined back to a real centroid
// instead of being dropped onto a synthetic lattice.
const SEED_CENSUS = [
  { code: '13000', name: '東京都', lat: 35.6896, lon: 139.6917, population: 13960000, households: 7227000, prefecture: '東京都' },
  { code: '14000', name: '神奈川県', lat: 35.4437, lon: 139.6380, population: 9237000, households: 4140000, prefecture: '神奈川県' },
  { code: '27000', name: '大阪府', lat: 34.6864, lon: 135.5197, population: 8838000, households: 3925000, prefecture: '大阪府' },
  { code: '23000', name: '愛知県', lat: 35.1814, lon: 136.9069, population: 7542000, households: 3155000, prefecture: '愛知県' },
  { code: '11000', name: '埼玉県', lat: 35.8617, lon: 139.6455, population: 7345000, households: 3132000, prefecture: '埼玉県' },
  { code: '12000', name: '千葉県', lat: 35.6083, lon: 140.1233, population: 6284000, households: 2773000, prefecture: '千葉県' },
  { code: '28000', name: '兵庫県', lat: 34.6913, lon: 135.1830, population: 5465000, households: 2310000, prefecture: '兵庫県' },
  { code: '01000', name: '北海道', lat: 43.0628, lon: 141.3478, population: 5224000, households: 2467000, prefecture: '北海道' },
  { code: '40000', name: '福岡県', lat: 33.5904, lon: 130.4017, population: 5135000, households: 2200000, prefecture: '福岡県' },
  { code: '22000', name: '静岡県', lat: 34.9756, lon: 138.3828, population: 3633000, households: 1480000, prefecture: '静岡県' },
  { code: '08000', name: '茨城県', lat: 36.3658, lon: 140.4711, population: 2867000, households: 1180000, prefecture: '茨城県' },
  { code: '34000', name: '広島県', lat: 34.3853, lon: 132.4553, population: 2800000, households: 1240000, prefecture: '広島県' },
  { code: '26000', name: '京都府', lat: 35.0116, lon: 135.7681, population: 2578000, households: 1180000, prefecture: '京都府' },
  { code: '04000', name: '宮城県', lat: 38.2683, lon: 140.8719, population: 2302000, households: 980000, prefecture: '宮城県' },
  { code: '15000', name: '新潟県', lat: 37.9161, lon: 139.0364, population: 2201000, households: 870000, prefecture: '新潟県' },
  { code: '20000', name: '長野県', lat: 36.6489, lon: 138.1944, population: 2049000, households: 830000, prefecture: '長野県' },
  { code: '21000', name: '岐阜県', lat: 35.4233, lon: 136.7606, population: 1979000, households: 770000, prefecture: '岐阜県' },
  { code: '09000', name: '栃木県', lat: 36.5658, lon: 139.8836, population: 1934000, households: 790000, prefecture: '栃木県' },
  { code: '10000', name: '群馬県', lat: 36.3911, lon: 139.0608, population: 1937000, households: 810000, prefecture: '群馬県' },
  { code: '33000', name: '岡山県', lat: 34.6628, lon: 133.9197, population: 1888000, households: 800000, prefecture: '岡山県' },
  { code: '07000', name: '福島県', lat: 37.7503, lon: 140.4675, population: 1833000, households: 740000, prefecture: '福島県' },
  { code: '24000', name: '三重県', lat: 34.7184, lon: 136.5067, population: 1781000, households: 730000, prefecture: '三重県' },
  { code: '43000', name: '熊本県', lat: 32.8019, lon: 130.7256, population: 1738000, households: 720000, prefecture: '熊本県' },
  { code: '46000', name: '鹿児島県', lat: 31.5963, lon: 130.5571, population: 1588000, households: 730000, prefecture: '鹿児島県' },
  { code: '47000', name: '沖縄県', lat: 26.2125, lon: 127.6809, population: 1467000, households: 610000, prefecture: '沖縄県' },
  { code: '25000', name: '滋賀県', lat: 35.0044, lon: 135.8686, population: 1414000, households: 540000, prefecture: '滋賀県' },
  { code: '35000', name: '山口県', lat: 34.1856, lon: 131.4714, population: 1342000, households: 600000, prefecture: '山口県' },
  { code: '38000', name: '愛媛県', lat: 33.8392, lon: 132.7656, population: 1335000, households: 590000, prefecture: '愛媛県' },
  { code: '42000', name: '長崎県', lat: 32.7503, lon: 129.8775, population: 1313000, households: 570000, prefecture: '長崎県' },
  { code: '29000', name: '奈良県', lat: 34.6850, lon: 135.8048, population: 1324000, households: 540000, prefecture: '奈良県' },
  { code: '02000', name: '青森県', lat: 40.8244, lon: 140.7400, population: 1238000, households: 510000, prefecture: '青森県' },
  { code: '03000', name: '岩手県', lat: 39.7036, lon: 141.1525, population: 1211000, households: 490000, prefecture: '岩手県' },
  { code: '44000', name: '大分県', lat: 33.2381, lon: 131.6126, population: 1124000, households: 490000, prefecture: '大分県' },
  { code: '17000', name: '石川県', lat: 36.5613, lon: 136.6562, population: 1133000, households: 470000, prefecture: '石川県' },
  { code: '06000', name: '山形県', lat: 38.2403, lon: 140.3633, population: 1068000, households: 400000, prefecture: '山形県' },
  { code: '45000', name: '宮崎県', lat: 31.9111, lon: 131.4239, population: 1070000, households: 470000, prefecture: '宮崎県' },
  { code: '16000', name: '富山県', lat: 36.6953, lon: 137.2113, population: 1035000, households: 410000, prefecture: '富山県' },
  { code: '37000', name: '香川県', lat: 34.3401, lon: 134.0434, population: 950000, households: 410000, prefecture: '香川県' },
  { code: '05000', name: '秋田県', lat: 39.7186, lon: 140.1024, population: 956000, households: 390000, prefecture: '秋田県' },
  { code: '30000', name: '和歌山県', lat: 34.2261, lon: 135.1675, population: 925000, households: 400000, prefecture: '和歌山県' },
  { code: '19000', name: '山梨県', lat: 35.6642, lon: 138.5683, population: 810000, households: 340000, prefecture: '山梨県' },
  { code: '41000', name: '佐賀県', lat: 33.2494, lon: 130.2989, population: 811000, households: 320000, prefecture: '佐賀県' },
  { code: '18000', name: '福井県', lat: 36.0613, lon: 136.2229, population: 766000, households: 290000, prefecture: '福井県' },
  { code: '36000', name: '徳島県', lat: 34.0658, lon: 134.5594, population: 720000, households: 310000, prefecture: '徳島県' },
  { code: '39000', name: '高知県', lat: 33.5594, lon: 133.5311, population: 691000, households: 320000, prefecture: '高知県' },
  { code: '32000', name: '島根県', lat: 35.4722, lon: 133.0506, population: 671000, households: 270000, prefecture: '島根県' },
  { code: '31000', name: '鳥取県', lat: 35.5036, lon: 134.2356, population: 553000, households: 220000, prefecture: '鳥取県' },
];

const PREF_INDEX = new Map(SEED_CENSUS.map((p) => [p.code, p]));

async function tryEstat() {
  if (!ESTAT_APP_ID) return null;
  try {
    const ctrl = new AbortController();
    const timeout = setTimeout(() => ctrl.abort(), 10000);
    // Stats data ID 0003448237 = 2020 census population by prefecture
    const url = `${ESTAT_URL}?appId=${ESTAT_APP_ID}&statsDataId=0003448237&limit=200`;
    const res = await fetch(url, { signal: ctrl.signal });
    clearTimeout(timeout);
    if (!res.ok) return null;
    const data = await res.json();
    const values = data.GET_STATS_DATA?.STATISTICAL_DATA?.DATA_INF?.VALUE || [];
    if (values.length === 0) return null;

    // Join each VALUE back to its prefecture centroid via the JIS X 0401
    // code returned in @area. Skip national totals and unknown codes.
    const out = [];
    for (const v of values) {
      const areaCode = v['@area'];
      const pref = areaCode ? PREF_INDEX.get(areaCode) : null;
      if (!pref) continue;
      const parsed = parseInt(v.$, 10);
      out.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [pref.lon, pref.lat] },
        properties: {
          mesh_id: `CENSUS_${pref.code}`,
          name: pref.name,
          prefecture: pref.prefecture,
          area_code: areaCode,
          value: Number.isFinite(parsed) ? parsed : null,
          unit: v['@unit'] || null,
          time: v['@time'] || null,
          country: 'JP',
          source: 'estat_api',
        },
      });
    }
    return out.length ? out : null;
  } catch {
    return null;
  }
}

function generateSeedData() {
  const now = new Date();
  return SEED_CENSUS.map((c, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [c.lon, c.lat] },
    properties: {
      mesh_id: `CENSUS_${String(i + 1).padStart(5, '0')}`,
      name: c.name,
      population: c.population,
      households: c.households,
      prefecture: c.prefecture,
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'estat_census_seed',
    },
  }));
}

async function tryOSMPrefectures() {
  // admin_level=4 → prefectures. Each has a population tag set from recent census.
  return fetchOverpass(
    'relation["admin_level"="4"]["boundary"="administrative"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        mesh_id: `OSM_PREF_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || `Prefecture ${i + 1}`,
        name_ja: el.tags?.name || null,
        population: parseInt(el.tags?.population) || null,
        iso_code: el.tags?.['ISO3166-2'] || null,
        wikidata: el.tags?.wikidata || null,
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

export default async function collectEstatCensus() {
  let features = await tryEstat();
  let liveSource = 'estat_api';
  if (!features || features.length === 0) {
    features = await tryOSMPrefectures();
    liveSource = 'osm_overpass';
  }
  const live = !!(features && features.length > 0);
  if (!live) {
    features = generateSeedData();
    liveSource = 'estat_census_seed';
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'estat_census',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: liveSource,
      description: 'Japanese census - e-Stat API + OSM prefecture admin boundaries',
    },
    metadata: {},
  };
}
