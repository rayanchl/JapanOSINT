/**
 * Racetracks Collector
 * JRA, NAR, Keirin (bicycle racing), Kyotei (boat racing), Auto race venues.
 * Live: OSM Overpass `sport=horse_racing|cycling|motor|motorboat`.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpass(
    'node["leisure"="track"]["sport"~"horse_racing|cycling|motor|motorboat"](area.jp);way["leisure"="track"]["sport"~"horse_racing|cycling|motor|motorboat"](area.jp);node["leisure"="track"]["name"~"競馬|競輪|競艇|オート"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        track_id: `OSM_${el.id}`,
        name: el.tags?.name || `Track ${i + 1}`,
        sport: el.tags?.sport || 'racing',
        operator: el.tags?.operator || 'unknown',
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

const SEED_TRACKS = [
  // JRA (Japan Racing Association) - 10 courses
  { name: '東京競馬場', lat: 35.6581, lon: 139.4793, type: 'horse_jra', operator: 'JRA', capacity: 223000 },
  { name: '中山競馬場', lat: 35.7222, lon: 139.9531, type: 'horse_jra', operator: 'JRA', capacity: 165676 },
  { name: '京都競馬場', lat: 34.9194, lon: 135.7469, type: 'horse_jra', operator: 'JRA', capacity: 120000 },
  { name: '阪神競馬場', lat: 34.7956, lon: 135.3694, type: 'horse_jra', operator: 'JRA', capacity: 139000 },
  { name: '中京競馬場', lat: 35.1539, lon: 136.9386, type: 'horse_jra', operator: 'JRA', capacity: 80000 },
  { name: '新潟競馬場', lat: 38.0292, lon: 139.2700, type: 'horse_jra', operator: 'JRA', capacity: 75000 },
  { name: '福島競馬場', lat: 37.7650, lon: 140.4419, type: 'horse_jra', operator: 'JRA', capacity: 41000 },
  { name: '小倉競馬場', lat: 33.8564, lon: 130.8708, type: 'horse_jra', operator: 'JRA', capacity: 40000 },
  { name: '札幌競馬場', lat: 43.0836, lon: 141.3286, type: 'horse_jra', operator: 'JRA', capacity: 25000 },
  { name: '函館競馬場', lat: 41.7864, lon: 140.7525, type: 'horse_jra', operator: 'JRA', capacity: 15000 },
  // NAR (local horse racing)
  { name: '大井競馬場', lat: 35.5878, lon: 139.7564, type: 'horse_nar', operator: '特別区競馬組合', capacity: 16000 },
  { name: '川崎競馬場', lat: 35.5311, lon: 139.7113, type: 'horse_nar', operator: '神奈川県川崎競馬組合', capacity: 30000 },
  { name: '船橋競馬場', lat: 35.6883, lon: 139.9839, type: 'horse_nar', operator: '千葉県競馬組合', capacity: 30000 },
  { name: '浦和競馬場', lat: 35.8617, lon: 139.6656, type: 'horse_nar', operator: '埼玉県浦和競馬組合', capacity: 12000 },
  { name: '門別競馬場', lat: 42.4719, lon: 142.0833, type: 'horse_nar', operator: '北海道', capacity: 10000 },
  { name: '盛岡競馬場', lat: 39.6464, lon: 141.1403, type: 'horse_nar', operator: '岩手県競馬組合', capacity: 15000 },
  { name: '金沢競馬場', lat: 36.5556, lon: 136.6364, type: 'horse_nar', operator: '石川県', capacity: 10000 },
  { name: '笠松競馬場', lat: 35.3653, lon: 136.7878, type: 'horse_nar', operator: '岐阜県地方競馬組合', capacity: 10000 },
  { name: '名古屋競馬場', lat: 35.0950, lon: 136.9400, type: 'horse_nar', operator: '愛知県競馬組合', capacity: 15000 },
  { name: '園田競馬場', lat: 34.7383, lon: 135.4181, type: 'horse_nar', operator: '兵庫県競馬組合', capacity: 30000 },
  { name: '姫路競馬場', lat: 34.8458, lon: 134.7011, type: 'horse_nar', operator: '兵庫県競馬組合', capacity: 15000 },
  { name: '高知競馬場', lat: 33.5461, lon: 133.5817, type: 'horse_nar', operator: '高知県競馬組合', capacity: 8000 },
  { name: '佐賀競馬場', lat: 33.2828, lon: 130.3514, type: 'horse_nar', operator: '佐賀県競馬組合', capacity: 10000 },
  // Keirin (43 velodromes) - top 20
  { name: '京王閣競輪場', lat: 35.6500, lon: 139.5500, type: 'keirin', operator: '東京都', capacity: 25000 },
  { name: '立川競輪場', lat: 35.7050, lon: 139.4100, type: 'keirin', operator: '立川市', capacity: 12000 },
  { name: '松戸競輪場', lat: 35.7900, lon: 139.9000, type: 'keirin', operator: '松戸市', capacity: 8000 },
  { name: '川崎競輪場', lat: 35.5300, lon: 139.7200, type: 'keirin', operator: '川崎市', capacity: 10000 },
  { name: '平塚競輪場', lat: 35.3300, lon: 139.3400, type: 'keirin', operator: '平塚市', capacity: 8000 },
  { name: '小田原競輪場', lat: 35.2400, lon: 139.1700, type: 'keirin', operator: '小田原市', capacity: 6000 },
  { name: '伊東温泉競輪場', lat: 34.9666, lon: 139.0966, type: 'keirin', operator: '伊東市', capacity: 5000 },
  { name: '西武園競輪場', lat: 35.7688, lon: 139.4472, type: 'keirin', operator: '所沢市', capacity: 8000 },
  { name: '京都向日町競輪場', lat: 34.9500, lon: 135.7100, type: 'keirin', operator: '京都府', capacity: 8000 },
  { name: '奈良競輪場', lat: 34.6750, lon: 135.8050, type: 'keirin', operator: '奈良県', capacity: 6000 },
  { name: '岸和田競輪場', lat: 34.4600, lon: 135.3700, type: 'keirin', operator: '岸和田市', capacity: 10000 },
  { name: '和歌山競輪場', lat: 34.2300, lon: 135.1700, type: 'keirin', operator: '和歌山市', capacity: 8000 },
  { name: '広島競輪場', lat: 34.3800, lon: 132.4500, type: 'keirin', operator: '広島市', capacity: 12000 },
  { name: '防府競輪場', lat: 34.0500, lon: 131.5800, type: 'keirin', operator: '防府市', capacity: 6000 },
  { name: '小倉競輪場', lat: 33.8833, lon: 130.8800, type: 'keirin', operator: '北九州市', capacity: 15000 },
  { name: '久留米競輪場', lat: 33.3000, lon: 130.5000, type: 'keirin', operator: '久留米市', capacity: 8000 },
  { name: '佐世保競輪場', lat: 33.1700, lon: 129.7200, type: 'keirin', operator: '佐世保市', capacity: 6000 },
  { name: '熊本競輪場', lat: 32.8000, lon: 130.7000, type: 'keirin', operator: '熊本市', capacity: 8000 },
  // Kyotei (boat racing) - 24 venues
  { name: '戸田競艇場', lat: 35.8200, lon: 139.6700, type: 'kyotei', operator: '戸田競艇組合', capacity: 30000 },
  { name: '江戸川競艇場', lat: 35.6900, lon: 139.8800, type: 'kyotei', operator: '江戸川競艇施行組合', capacity: 20000 },
  { name: '平和島競艇場', lat: 35.5800, lon: 139.7500, type: 'kyotei', operator: '東京都六市競艇事業組合', capacity: 30000 },
  { name: '多摩川競艇場', lat: 35.6500, lon: 139.5300, type: 'kyotei', operator: '多摩川競艇組合', capacity: 20000 },
  { name: '浜名湖競艇場', lat: 34.7500, lon: 137.5500, type: 'kyotei', operator: '湖西市', capacity: 15000 },
  { name: '蒲郡競艇場', lat: 34.8200, lon: 137.2100, type: 'kyotei', operator: '蒲郡市', capacity: 20000 },
  { name: '常滑競艇場', lat: 34.8900, lon: 136.8500, type: 'kyotei', operator: '常滑市', capacity: 15000 },
  { name: '津競艇場', lat: 34.7200, lon: 136.5100, type: 'kyotei', operator: '津市', capacity: 12000 },
  { name: 'びわこ競艇場', lat: 35.0300, lon: 135.8700, type: 'kyotei', operator: '大津市', capacity: 15000 },
  { name: '住之江競艇場', lat: 34.6100, lon: 135.4700, type: 'kyotei', operator: '箕面市・池田市・大阪府都市競艇', capacity: 30000 },
  { name: '尼崎競艇場', lat: 34.7300, lon: 135.4100, type: 'kyotei', operator: '尼崎市', capacity: 20000 },
  { name: '鳴門競艇場', lat: 34.1700, lon: 134.6400, type: 'kyotei', operator: '鳴門市', capacity: 15000 },
  { name: '丸亀競艇場', lat: 34.3000, lon: 133.7900, type: 'kyotei', operator: '丸亀市', capacity: 20000 },
  { name: '児島競艇場', lat: 34.4600, lon: 133.7800, type: 'kyotei', operator: '倉敷市', capacity: 15000 },
  { name: '宮島競艇場', lat: 34.2900, lon: 132.3300, type: 'kyotei', operator: '廿日市市', capacity: 15000 },
  { name: '徳山競艇場', lat: 34.0500, lon: 131.8200, type: 'kyotei', operator: '周南市', capacity: 15000 },
  { name: '下関競艇場', lat: 33.9500, lon: 130.9400, type: 'kyotei', operator: '下関市', capacity: 15000 },
  { name: '若松競艇場', lat: 33.9000, lon: 130.8100, type: 'kyotei', operator: '北九州市', capacity: 20000 },
  { name: '芦屋競艇場', lat: 33.8900, lon: 130.6400, type: 'kyotei', operator: '芦屋町', capacity: 12000 },
  { name: '福岡競艇場', lat: 33.5900, lon: 130.3500, type: 'kyotei', operator: '福岡市', capacity: 20000 },
  { name: '唐津競艇場', lat: 33.4500, lon: 129.9800, type: 'kyotei', operator: '唐津市', capacity: 10000 },
  { name: '大村競艇場', lat: 32.9000, lon: 129.9500, type: 'kyotei', operator: '大村市', capacity: 12000 },
  // Auto race - 5 venues
  { name: '川口オートレース場', lat: 35.8300, lon: 139.7400, type: 'auto_race', operator: '川口市', capacity: 15000 },
  { name: '伊勢崎オートレース場', lat: 36.3100, lon: 139.1900, type: 'auto_race', operator: '伊勢崎市', capacity: 10000 },
  { name: '浜松オートレース場', lat: 34.7200, lon: 137.7200, type: 'auto_race', operator: '浜松市', capacity: 15000 },
  { name: '飯塚オートレース場', lat: 33.6500, lon: 130.7000, type: 'auto_race', operator: '飯塚市', capacity: 10000 },
  { name: '山陽オートレース場', lat: 34.7800, lon: 131.2700, type: 'auto_race', operator: '山口県', capacity: 8000 },
];

function generateSeedData() {
  return SEED_TRACKS.map((t, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [t.lon, t.lat] },
    properties: {
      track_id: `TRACK_${String(i + 1).padStart(5, '0')}`,
      name: t.name,
      sport: t.type,
      operator: t.operator,
      capacity: t.capacity,
      country: 'JP',
      source: 'racetracks_seed',
    },
  }));
}

export default async function collectRacetracks() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'racetracks',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'osm_overpass' : 'racetracks_seed',
      description: 'Japan racetracks - JRA/NAR horse racing, keirin, kyotei, auto race',
    },
  };
}
