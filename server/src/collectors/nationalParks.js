/**
 * National Parks Collector
 * Ministry of Environment National Park boundaries (34 parks) + Quasi-national Parks.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return await fetchOverpass(
    'relation["boundary"="national_park"](area.jp);relation["boundary"="protected_area"]["protect_class"="2"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        park_id: `PARK_LIVE_${String(i + 1).padStart(5, '0')}`,
        name: el.tags?.name || el.tags?.['name:en'] || `Park ${el.id}`,
        kind: el.tags?.boundary === 'national_park' ? 'national' : 'protected_area',
        protect_class: el.tags?.protect_class || null,
        country: 'JP',
        source: 'national_parks_live',
      },
    })
  );
}

const SEED_PARKS = [
  // National Parks (国立公園) — selected centroids
  { name: '利尻礼文サロベツ国立公園', lat: 45.1839, lon: 141.2433, kind: 'national', area_ha: 24166, est: 1974 },
  { name: '知床国立公園', lat: 44.0667, lon: 145.0000, kind: 'national', area_ha: 38636, est: 1964 },
  { name: '阿寒摩周国立公園', lat: 43.4500, lon: 144.0500, kind: 'national', area_ha: 91413, est: 1934 },
  { name: '釧路湿原国立公園', lat: 43.1133, lon: 144.4067, kind: 'national', area_ha: 28788, est: 1987 },
  { name: '大雪山国立公園', lat: 43.6633, lon: 142.8519, kind: 'national', area_ha: 226764, est: 1934 },
  { name: '支笏洞爺国立公園', lat: 42.7667, lon: 141.3333, kind: 'national', area_ha: 99473, est: 1949 },
  { name: '十和田八幡平国立公園', lat: 40.4667, lon: 140.8833, kind: 'national', area_ha: 85551, est: 1936 },
  { name: '三陸復興国立公園', lat: 39.6500, lon: 141.9500, kind: 'national', area_ha: 28537, est: 2013 },
  { name: '磐梯朝日国立公園', lat: 37.6500, lon: 140.0667, kind: 'national', area_ha: 186375, est: 1950 },
  { name: '日光国立公園', lat: 36.7833, lon: 139.5000, kind: 'national', area_ha: 114908, est: 1934 },
  { name: '尾瀬国立公園', lat: 36.9167, lon: 139.2500, kind: 'national', area_ha: 37222, est: 2007 },
  { name: '上信越高原国立公園', lat: 36.6333, lon: 138.5167, kind: 'national', area_ha: 148194, est: 1949 },
  { name: '秩父多摩甲斐国立公園', lat: 35.8833, lon: 138.9167, kind: 'national', area_ha: 126259, est: 1950 },
  { name: '小笠原国立公園', lat: 27.0833, lon: 142.1833, kind: 'national', area_ha: 6629, est: 1972 },
  { name: '富士箱根伊豆国立公園', lat: 35.3606, lon: 138.7311, kind: 'national', area_ha: 121749, est: 1936 },
  { name: '中部山岳国立公園', lat: 36.2900, lon: 137.6500, kind: 'national', area_ha: 174323, est: 1934 },
  { name: '妙高戸隠連山国立公園', lat: 36.8833, lon: 138.1167, kind: 'national', area_ha: 39772, est: 2015 },
  { name: '白山国立公園', lat: 36.1572, lon: 136.7711, kind: 'national', area_ha: 49900, est: 1962 },
  { name: '南アルプス国立公園', lat: 35.6633, lon: 138.2389, kind: 'national', area_ha: 35752, est: 1964 },
  { name: '伊勢志摩国立公園', lat: 34.4500, lon: 136.8333, kind: 'national', area_ha: 55544, est: 1946 },
  { name: '吉野熊野国立公園', lat: 34.0667, lon: 135.9000, kind: 'national', area_ha: 61406, est: 1936 },
  { name: '山陰海岸国立公園', lat: 35.6500, lon: 134.5333, kind: 'national', area_ha: 8783, est: 1963 },
  { name: '瀬戸内海国立公園', lat: 34.3500, lon: 133.7833, kind: 'national', area_ha: 67242, est: 1934 },
  { name: '大山隠岐国立公園', lat: 35.3717, lon: 133.5364, kind: 'national', area_ha: 35353, est: 1936 },
  { name: '足摺宇和海国立公園', lat: 32.7233, lon: 133.0167, kind: 'national', area_ha: 11345, est: 1972 },
  { name: '西海国立公園', lat: 33.1667, lon: 129.6167, kind: 'national', area_ha: 24646, est: 1955 },
  { name: '雲仙天草国立公園', lat: 32.7536, lon: 130.2942, kind: 'national', area_ha: 28279, est: 1934 },
  { name: '阿蘇くじゅう国立公園', lat: 32.8836, lon: 131.1042, kind: 'national', area_ha: 73017, est: 1934 },
  { name: '霧島錦江湾国立公園', lat: 31.8800, lon: 130.8550, kind: 'national', area_ha: 36605, est: 1934 },
  { name: '屋久島国立公園', lat: 30.3500, lon: 130.5167, kind: 'national', area_ha: 24566, est: 2012 },
  { name: '奄美群島国立公園', lat: 28.3000, lon: 129.5000, kind: 'national', area_ha: 42196, est: 2017 },
  { name: 'やんばる国立公園', lat: 26.7500, lon: 128.2167, kind: 'national', area_ha: 17311, est: 2016 },
  { name: '慶良間諸島国立公園', lat: 26.2000, lon: 127.3500, kind: 'national', area_ha: 3520, est: 2014 },
  { name: '西表石垣国立公園', lat: 24.3833, lon: 123.7833, kind: 'national', area_ha: 40653, est: 1972 },
  // Quasi-national Parks (国定公園) — selected
  { name: '網走国定公園', lat: 44.0167, lon: 144.2667, kind: 'quasi', area_ha: 37261, est: 1958 },
  { name: '暑寒別天売焼尻国定公園', lat: 43.7706, lon: 141.5500, kind: 'quasi', area_ha: 43559, est: 1990 },
  { name: '津軽国定公園', lat: 41.0500, lon: 140.3667, kind: 'quasi', area_ha: 25966, est: 1975 },
  { name: '下北半島国定公園', lat: 41.4833, lon: 141.0833, kind: 'quasi', area_ha: 18641, est: 1968 },
  { name: '早池峰国定公園', lat: 39.5667, lon: 141.4833, kind: 'quasi', area_ha: 5463, est: 1982 },
  { name: '栗駒国定公園', lat: 38.9500, lon: 140.7833, kind: 'quasi', area_ha: 77122, est: 1968 },
  { name: '蔵王国定公園', lat: 38.1442, lon: 140.4500, kind: 'quasi', area_ha: 39635, est: 1963 },
  { name: '越後三山只見国定公園', lat: 37.2333, lon: 139.2833, kind: 'quasi', area_ha: 86129, est: 1973 },
  { name: '水郷筑波国定公園', lat: 36.2256, lon: 140.0997, kind: 'quasi', area_ha: 35535, est: 1959 },
  { name: '南房総国定公園', lat: 34.9667, lon: 139.8333, kind: 'quasi', area_ha: 5690, est: 1958 },
  { name: '丹沢大山国定公園', lat: 35.4406, lon: 139.2436, kind: 'quasi', area_ha: 27572, est: 1965 },
  { name: '能登半島国定公園', lat: 37.3167, lon: 136.9167, kind: 'quasi', area_ha: 9672, est: 1968 },
  { name: '若狭湾国定公園', lat: 35.6500, lon: 135.5000, kind: 'quasi', area_ha: 19191, est: 1955 },
  { name: '比叡山琵琶湖国定公園', lat: 35.2167, lon: 135.9000, kind: 'quasi', area_ha: 97601, est: 1950 },
  { name: '高野龍神国定公園', lat: 34.0500, lon: 135.5500, kind: 'quasi', area_ha: 19198, est: 1967 },
  { name: '室戸阿南海岸国定公園', lat: 33.2667, lon: 134.1833, kind: 'quasi', area_ha: 6230, est: 1964 },
  { name: '剣山国定公園', lat: 33.8533, lon: 134.0942, kind: 'quasi', area_ha: 20964, est: 1964 },
  { name: '北九州国定公園', lat: 33.7833, lon: 130.7833, kind: 'quasi', area_ha: 8107, est: 1972 },
  { name: '玄海国定公園', lat: 33.5333, lon: 130.0500, kind: 'quasi', area_ha: 10186, est: 1956 },
  { name: '日豊海岸国定公園', lat: 32.8500, lon: 131.8167, kind: 'quasi', area_ha: 8518, est: 1974 },
  { name: '祖母傾国定公園', lat: 32.8333, lon: 131.3333, kind: 'quasi', area_ha: 22000, est: 1965 },
  { name: '日南海岸国定公園', lat: 31.5833, lon: 131.4000, kind: 'quasi', area_ha: 4542, est: 1955 },
  { name: '甑島国定公園', lat: 31.8167, lon: 129.8833, kind: 'quasi', area_ha: 5447, est: 2015 },
  { name: '沖縄海岸国定公園', lat: 26.6000, lon: 127.9500, kind: 'quasi', area_ha: 4476, est: 1972 },
  { name: '沖縄戦跡国定公園', lat: 26.0833, lon: 127.7167, kind: 'quasi', area_ha: 3127, est: 1972 },
];

function generateSeedData() {
  return SEED_PARKS.map((p, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [p.lon, p.lat] },
    properties: {
      park_id: `PARK_${String(i + 1).padStart(5, '0')}`,
      name: p.name,
      kind: p.kind,
      area_ha: p.area_ha,
      est: p.est,
      country: 'JP',
      source: 'moe_park_seed',
    },
  }));
}

export default async function collectNationalParks() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'national_parks',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Ministry of Environment national parks (34) and quasi-national parks',
    },
  };
}
