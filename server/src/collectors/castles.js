/**
 * Castles Collector
 * 100 Famous Japanese Castles + Top 100 Continued — historic castles across Japan.
 */

import { fetchOverpass } from './_liveHelpers.js';

const SEED_CASTLES = [
  // 100 Famous Castles selected list (existing structures or major reconstructions)
  { name: '根室半島チャシ跡群', lat: 43.3300, lon: 145.5333, era: 'Ainu', cls: 'famous_100' },
  { name: '五稜郭', lat: 41.7972, lon: 140.7569, era: 'Edo', cls: 'famous_100' },
  { name: '松前城', lat: 41.4283, lon: 140.1086, era: 'Edo', cls: 'famous_100' },
  { name: '弘前城', lat: 40.6075, lon: 140.4644, era: 'Edo', cls: 'famous_100' },
  { name: '根城', lat: 40.4889, lon: 141.4742, era: 'Sengoku', cls: 'famous_100' },
  { name: '盛岡城', lat: 39.7000, lon: 141.1500, era: 'Edo', cls: 'famous_100' },
  { name: '多賀城', lat: 38.3083, lon: 140.9889, era: 'Nara', cls: 'famous_100' },
  { name: '仙台城', lat: 38.2522, lon: 140.8556, era: 'Edo', cls: 'famous_100' },
  { name: '久保田城', lat: 39.7178, lon: 140.1242, era: 'Edo', cls: 'famous_100' },
  { name: '山形城', lat: 38.2531, lon: 140.3261, era: 'Edo', cls: 'famous_100' },
  { name: '二本松城', lat: 37.5878, lon: 140.4314, era: 'Edo', cls: 'famous_100' },
  { name: '会津若松城', lat: 37.4869, lon: 139.9297, era: 'Edo', cls: 'famous_100' },
  { name: '白河小峰城', lat: 37.1314, lon: 140.2197, era: 'Edo', cls: 'famous_100' },
  { name: '水戸城', lat: 36.3722, lon: 140.4750, era: 'Edo', cls: 'famous_100' },
  { name: '足利氏館', lat: 36.3375, lon: 139.4592, era: 'Kamakura', cls: 'famous_100' },
  { name: '箕輪城', lat: 36.4233, lon: 138.9286, era: 'Sengoku', cls: 'famous_100' },
  { name: '金山城', lat: 36.2997, lon: 139.3825, era: 'Sengoku', cls: 'famous_100' },
  { name: '鉢形城', lat: 36.1186, lon: 139.1858, era: 'Sengoku', cls: 'famous_100' },
  { name: '川越城', lat: 35.9272, lon: 139.4878, era: 'Edo', cls: 'famous_100' },
  { name: '佐倉城', lat: 35.7239, lon: 140.2208, era: 'Edo', cls: 'famous_100' },
  { name: '江戸城', lat: 35.6852, lon: 139.7528, era: 'Edo', cls: 'famous_100' },
  { name: '八王子城', lat: 35.6586, lon: 139.2606, era: 'Sengoku', cls: 'famous_100' },
  { name: '小田原城', lat: 35.2503, lon: 139.1531, era: 'Edo', cls: 'famous_100' },
  { name: '武田氏館', lat: 35.6711, lon: 138.5783, era: 'Sengoku', cls: 'famous_100' },
  { name: '甲府城', lat: 35.6622, lon: 138.5697, era: 'Edo', cls: 'famous_100' },
  { name: '松代城', lat: 36.5717, lon: 138.1942, era: 'Edo', cls: 'famous_100' },
  { name: '上田城', lat: 36.4039, lon: 138.2475, era: 'Edo', cls: 'famous_100' },
  { name: '小諸城', lat: 36.3275, lon: 138.4214, era: 'Edo', cls: 'famous_100' },
  { name: '松本城', lat: 36.2386, lon: 137.9692, era: 'Edo', cls: 'famous_100' },
  { name: '高遠城', lat: 35.8344, lon: 138.0664, era: 'Edo', cls: 'famous_100' },
  { name: '新発田城', lat: 37.9514, lon: 139.3294, era: 'Edo', cls: 'famous_100' },
  { name: '春日山城', lat: 37.1456, lon: 138.2369, era: 'Sengoku', cls: 'famous_100' },
  { name: '高岡城', lat: 36.7508, lon: 137.0244, era: 'Edo', cls: 'famous_100' },
  { name: '七尾城', lat: 37.0192, lon: 136.9789, era: 'Sengoku', cls: 'famous_100' },
  { name: '金沢城', lat: 36.5650, lon: 136.6594, era: 'Edo', cls: 'famous_100' },
  { name: '丸岡城', lat: 36.1542, lon: 136.2722, era: 'Edo', cls: 'famous_100' },
  { name: '一乗谷城', lat: 35.9836, lon: 136.3056, era: 'Sengoku', cls: 'famous_100' },
  { name: '岩村城', lat: 35.3550, lon: 137.4636, era: 'Sengoku', cls: 'famous_100' },
  { name: '岐阜城', lat: 35.4339, lon: 136.7822, era: 'Sengoku', cls: 'famous_100' },
  { name: '山中城', lat: 35.1183, lon: 138.9319, era: 'Sengoku', cls: 'famous_100' },
  { name: '駿府城', lat: 34.9789, lon: 138.3833, era: 'Edo', cls: 'famous_100' },
  { name: '掛川城', lat: 34.7686, lon: 138.0150, era: 'Edo', cls: 'famous_100' },
  { name: '犬山城', lat: 35.3886, lon: 136.9392, era: 'Edo', cls: 'famous_100' },
  { name: '名古屋城', lat: 35.1853, lon: 136.8994, era: 'Edo', cls: 'famous_100' },
  { name: '岡崎城', lat: 34.9569, lon: 137.1583, era: 'Edo', cls: 'famous_100' },
  { name: '長篠城', lat: 34.9275, lon: 137.5572, era: 'Sengoku', cls: 'famous_100' },
  { name: '伊賀上野城', lat: 34.7672, lon: 136.1311, era: 'Edo', cls: 'famous_100' },
  { name: '松阪城', lat: 34.5797, lon: 136.5292, era: 'Edo', cls: 'famous_100' },
  { name: '小谷城', lat: 35.4708, lon: 136.2786, era: 'Sengoku', cls: 'famous_100' },
  { name: '彦根城', lat: 35.2764, lon: 136.2517, era: 'Edo', cls: 'famous_100' },
  { name: '安土城', lat: 35.1561, lon: 136.1394, era: 'Sengoku', cls: 'famous_100' },
  { name: '観音寺城', lat: 35.1572, lon: 136.1681, era: 'Sengoku', cls: 'famous_100' },
  { name: '二条城', lat: 35.0142, lon: 135.7475, era: 'Edo', cls: 'famous_100' },
  { name: '大阪城', lat: 34.6873, lon: 135.5262, era: 'Edo', cls: 'famous_100' },
  { name: '千早城', lat: 34.4189, lon: 135.6611, era: 'Kamakura', cls: 'famous_100' },
  { name: '竹田城', lat: 35.3000, lon: 134.8294, era: 'Sengoku', cls: 'famous_100' },
  { name: '篠山城', lat: 35.0750, lon: 135.2189, era: 'Edo', cls: 'famous_100' },
  { name: '明石城', lat: 34.6553, lon: 134.9914, era: 'Edo', cls: 'famous_100' },
  { name: '姫路城', lat: 34.8394, lon: 134.6939, era: 'Edo', cls: 'famous_100' },
  { name: '赤穂城', lat: 34.7461, lon: 134.3917, era: 'Edo', cls: 'famous_100' },
  { name: '高取城', lat: 34.4369, lon: 135.8158, era: 'Sengoku', cls: 'famous_100' },
  { name: '和歌山城', lat: 34.2275, lon: 135.1717, era: 'Edo', cls: 'famous_100' },
  { name: '鳥取城', lat: 35.5050, lon: 134.2406, era: 'Edo', cls: 'famous_100' },
  { name: '松江城', lat: 35.4750, lon: 133.0508, era: 'Edo', cls: 'famous_100' },
  { name: '月山富田城', lat: 35.4108, lon: 133.1833, era: 'Sengoku', cls: 'famous_100' },
  { name: '津和野城', lat: 34.4675, lon: 131.7700, era: 'Edo', cls: 'famous_100' },
  { name: '津山城', lat: 35.0697, lon: 134.0036, era: 'Edo', cls: 'famous_100' },
  { name: '備中松山城', lat: 34.8092, lon: 133.6197, era: 'Edo', cls: 'famous_100' },
  { name: '鬼ノ城', lat: 34.7547, lon: 133.7906, era: 'Asuka', cls: 'famous_100' },
  { name: '岡山城', lat: 34.6650, lon: 133.9358, era: 'Edo', cls: 'famous_100' },
  { name: '福山城', lat: 34.4892, lon: 133.3617, era: 'Edo', cls: 'famous_100' },
  { name: '郡山城', lat: 34.6747, lon: 132.7142, era: 'Sengoku', cls: 'famous_100' },
  { name: '広島城', lat: 34.4028, lon: 132.4592, era: 'Edo', cls: 'famous_100' },
  { name: '岩国城', lat: 34.1672, lon: 132.1742, era: 'Edo', cls: 'famous_100' },
  { name: '萩城', lat: 34.4178, lon: 131.3922, era: 'Edo', cls: 'famous_100' },
  { name: '徳島城', lat: 34.0750, lon: 134.5550, era: 'Edo', cls: 'famous_100' },
  { name: '高松城', lat: 34.3486, lon: 134.0500, era: 'Edo', cls: 'famous_100' },
  { name: '丸亀城', lat: 34.2864, lon: 133.8033, era: 'Edo', cls: 'famous_100' },
  { name: '今治城', lat: 34.0653, lon: 132.9956, era: 'Edo', cls: 'famous_100' },
  { name: '湯築城', lat: 33.8458, lon: 132.7858, era: 'Sengoku', cls: 'famous_100' },
  { name: '松山城 (伊予)', lat: 33.8453, lon: 132.7656, era: 'Edo', cls: 'famous_100' },
  { name: '大洲城', lat: 33.5050, lon: 132.5447, era: 'Edo', cls: 'famous_100' },
  { name: '宇和島城', lat: 33.2197, lon: 132.5639, era: 'Edo', cls: 'famous_100' },
  { name: '高知城', lat: 33.5611, lon: 133.5314, era: 'Edo', cls: 'famous_100' },
  { name: '福岡城', lat: 33.5847, lon: 130.3819, era: 'Edo', cls: 'famous_100' },
  { name: '大野城', lat: 33.5444, lon: 130.4883, era: 'Asuka', cls: 'famous_100' },
  { name: '名護屋城', lat: 33.5294, lon: 129.8536, era: 'Sengoku', cls: 'famous_100' },
  { name: '吉野ヶ里', lat: 33.3225, lon: 130.3850, era: 'Yayoi', cls: 'famous_100' },
  { name: '佐賀城', lat: 33.2447, lon: 130.3017, era: 'Edo', cls: 'famous_100' },
  { name: '平戸城', lat: 33.3697, lon: 129.5536, era: 'Edo', cls: 'famous_100' },
  { name: '島原城', lat: 32.7878, lon: 130.3611, era: 'Edo', cls: 'famous_100' },
  { name: '熊本城', lat: 32.8064, lon: 130.7053, era: 'Edo', cls: 'famous_100' },
  { name: '人吉城', lat: 32.2089, lon: 130.7611, era: 'Edo', cls: 'famous_100' },
  { name: '大分府内城', lat: 33.2369, lon: 131.6086, era: 'Edo', cls: 'famous_100' },
  { name: '岡城', lat: 32.9700, lon: 131.4117, era: 'Edo', cls: 'famous_100' },
  { name: '飫肥城', lat: 31.6228, lon: 131.3536, era: 'Edo', cls: 'famous_100' },
  { name: '鹿児島城', lat: 31.5969, lon: 130.5547, era: 'Edo', cls: 'famous_100' },
  { name: '今帰仁城', lat: 26.6906, lon: 127.9267, era: 'Ryukyu', cls: 'famous_100' },
  { name: '中城城', lat: 26.2858, lon: 127.7917, era: 'Ryukyu', cls: 'famous_100' },
  { name: '首里城', lat: 26.2169, lon: 127.7194, era: 'Ryukyu', cls: 'famous_100' },
];

async function tryOSMOverpass() {
  const features = await fetchOverpass(
    'node["historic"="castle"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        castle_id: `OSM_${el.id}`,
        name: el.tags?.name || el.tags?.['name:en'] || `Castle ${i + 1}`,
        era: el.tags?.start_date || 'unknown',
        cls: 'osm',
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
  if (!features) return null;
  return features.slice(0, 200);
}

function generateSeedData() {
  return SEED_CASTLES.map((c, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [c.lon, c.lat] },
    properties: {
      castle_id: `CASTLE_${String(i + 1).padStart(5, '0')}`,
      name: c.name,
      era: c.era,
      cls: c.cls,
      country: 'JP',
      source: 'famous100_castle_seed',
    },
  }));
}

export default async function collectCastles() {
  let features = await tryOSMOverpass();
  if (!features || features.length === 0) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'castles',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: features?.[0]?.properties?.source === 'osm_overpass',
      description: '100 Famous Castles + 100 Continued — historic Japanese castles',
    },
    metadata: {},
  };
}
