/**
 * Hospital Map Collector
 * Maps hospitals and clinics across Japan via OSM Overpass API.
 * Falls back to a curated seed of major hospitals if Overpass is unreachable.
 */

import { fetchOverpassTiled } from './_liveHelpers.js';

const SEED_HOSPITALS = [
  { name: '東京大学医学部附属病院', lat: 35.7128, lon: 139.7619, beds: 1226, prefecture: '東京都', type: 'university' },
  { name: '聖路加国際病院', lat: 35.6647, lon: 139.7811, beds: 520, prefecture: '東京都', type: 'general' },
  { name: '慶應義塾大学病院', lat: 35.6814, lon: 139.7211, beds: 950, prefecture: '東京都', type: 'university' },
  { name: '東京医科歯科大学医学部附属病院', lat: 35.7036, lon: 139.7644, beds: 753, prefecture: '東京都', type: 'university' },
  { name: '虎の門病院', lat: 35.6685, lon: 139.7460, beds: 819, prefecture: '東京都', type: 'general' },
  { name: '東京慈恵会医科大学附属病院', lat: 35.6589, lon: 139.7505, beds: 1075, prefecture: '東京都', type: 'university' },
  { name: '日本医科大学付属病院', lat: 35.7081, lon: 139.7625, beds: 899, prefecture: '東京都', type: 'university' },
  { name: '国立国際医療研究センター', lat: 35.6961, lon: 139.7269, beds: 781, prefecture: '東京都', type: 'national' },
  { name: '癌研有明病院', lat: 35.6347, lon: 139.7950, beds: 700, prefecture: '東京都', type: 'specialized' },
  { name: '横浜市立大学附属病院', lat: 35.3392, lon: 139.6500, beds: 674, prefecture: '神奈川県', type: 'university' },
  { name: '横浜市立大学附属市民総合医療センター', lat: 35.4378, lon: 139.6336, beds: 726, prefecture: '神奈川県', type: 'university' },
  { name: '北里大学病院', lat: 35.5119, lon: 139.4172, beds: 1033, prefecture: '神奈川県', type: 'university' },
  { name: '東海大学医学部付属病院', lat: 35.3819, lon: 139.3247, beds: 804, prefecture: '神奈川県', type: 'university' },
  { name: '聖マリアンナ医科大学病院', lat: 35.6072, lon: 139.5481, beds: 1208, prefecture: '神奈川県', type: 'university' },
  { name: '大阪大学医学部附属病院', lat: 34.8253, lon: 135.5189, beds: 1086, prefecture: '大阪府', type: 'university' },
  { name: '大阪市立大学医学部附属病院', lat: 34.6444, lon: 135.5056, beds: 980, prefecture: '大阪府', type: 'university' },
  { name: '大阪医科薬科大学病院', lat: 34.8550, lon: 135.6164, beds: 832, prefecture: '大阪府', type: 'university' },
  { name: '関西医科大学附属病院', lat: 34.8194, lon: 135.6333, beds: 751, prefecture: '大阪府', type: 'university' },
  { name: '京都大学医学部附属病院', lat: 35.0211, lon: 135.7800, beds: 1121, prefecture: '京都府', type: 'university' },
  { name: '京都府立医科大学附属病院', lat: 35.0244, lon: 135.7681, beds: 1065, prefecture: '京都府', type: 'university' },
  { name: '神戸大学医学部附属病院', lat: 34.6917, lon: 135.1814, beds: 934, prefecture: '兵庫県', type: 'university' },
  { name: '名古屋大学医学部附属病院', lat: 35.1583, lon: 136.9172, beds: 1080, prefecture: '愛知県', type: 'university' },
  { name: '名古屋市立大学病院', lat: 35.1097, lon: 136.9356, beds: 800, prefecture: '愛知県', type: 'university' },
  { name: '藤田医科大学病院', lat: 35.1131, lon: 136.9728, beds: 1376, prefecture: '愛知県', type: 'university' },
  { name: '愛知医科大学病院', lat: 35.1972, lon: 137.0533, beds: 900, prefecture: '愛知県', type: 'university' },
  { name: '北海道大学病院', lat: 43.0731, lon: 141.3389, beds: 944, prefecture: '北海道', type: 'university' },
  { name: '札幌医科大学附属病院', lat: 43.0639, lon: 141.3389, beds: 938, prefecture: '北海道', type: 'university' },
  { name: '東北大学病院', lat: 38.2553, lon: 140.8553, beds: 1225, prefecture: '宮城県', type: 'university' },
  { name: '九州大学病院', lat: 33.6097, lon: 130.4253, beds: 1275, prefecture: '福岡県', type: 'university' },
  { name: '福岡大学病院', lat: 33.5519, lon: 130.3661, beds: 915, prefecture: '福岡県', type: 'university' },
  { name: '産業医科大学病院', lat: 33.8389, lon: 130.7800, beds: 678, prefecture: '福岡県', type: 'university' },
  { name: '長崎大学病院', lat: 32.7708, lon: 129.8722, beds: 862, prefecture: '長崎県', type: 'university' },
  { name: '熊本大学病院', lat: 32.8019, lon: 130.7256, beds: 845, prefecture: '熊本県', type: 'university' },
  { name: '鹿児島大学病院', lat: 31.5578, lon: 130.5439, beds: 707, prefecture: '鹿児島県', type: 'university' },
  { name: '広島大学病院', lat: 34.3961, lon: 132.4519, beds: 740, prefecture: '広島県', type: 'university' },
  { name: '岡山大学病院', lat: 34.6628, lon: 133.9197, beds: 850, prefecture: '岡山県', type: 'university' },
  { name: '山口大学医学部附属病院', lat: 34.0258, lon: 131.4633, beds: 727, prefecture: '山口県', type: 'university' },
  { name: '徳島大学病院', lat: 34.0700, lon: 134.5689, beds: 696, prefecture: '徳島県', type: 'university' },
  { name: '香川大学医学部附属病院', lat: 34.2756, lon: 134.0514, beds: 614, prefecture: '香川県', type: 'university' },
  { name: '愛媛大学医学部附属病院', lat: 33.7811, lon: 132.7903, beds: 644, prefecture: '愛媛県', type: 'university' },
  { name: '高知大学医学部附属病院', lat: 33.6442, lon: 133.5814, beds: 613, prefecture: '高知県', type: 'university' },
  { name: '琉球大学病院', lat: 26.2533, lon: 127.7572, beds: 600, prefecture: '沖縄県', type: 'university' },
  { name: '沖縄県立中部病院', lat: 26.3506, lon: 127.8519, beds: 550, prefecture: '沖縄県', type: 'public' },
  { name: '弘前大学医学部附属病院', lat: 40.5928, lon: 140.4753, beds: 644, prefecture: '青森県', type: 'university' },
  { name: '岩手医科大学附属病院', lat: 39.6306, lon: 141.1481, beds: 1000, prefecture: '岩手県', type: 'university' },
  { name: '秋田大学医学部附属病院', lat: 39.7186, lon: 140.1356, beds: 638, prefecture: '秋田県', type: 'university' },
  { name: '山形大学医学部附属病院', lat: 38.2683, lon: 140.3439, beds: 637, prefecture: '山形県', type: 'university' },
  { name: '福島県立医科大学附属病院', lat: 37.6797, lon: 140.4583, beds: 778, prefecture: '福島県', type: 'university' },
  { name: '筑波大学附属病院', lat: 36.1083, lon: 140.1011, beds: 800, prefecture: '茨城県', type: 'university' },
  { name: '千葉大学医学部附属病院', lat: 35.6253, lon: 140.1031, beds: 850, prefecture: '千葉県', type: 'university' },
  { name: '群馬大学医学部附属病院', lat: 36.4081, lon: 139.0744, beds: 731, prefecture: '群馬県', type: 'university' },
  { name: '埼玉医科大学病院', lat: 35.9194, lon: 139.4150, beds: 953, prefecture: '埼玉県', type: 'university' },
  { name: '自治医科大学附属病院', lat: 36.4153, lon: 139.8744, beds: 1132, prefecture: '栃木県', type: 'university' },
  { name: '信州大学医学部附属病院', lat: 36.2417, lon: 137.9706, beds: 717, prefecture: '長野県', type: 'university' },
  { name: '新潟大学医歯学総合病院', lat: 37.8742, lon: 139.0119, beds: 832, prefecture: '新潟県', type: 'university' },
  { name: '富山大学附属病院', lat: 36.6953, lon: 137.1869, beds: 612, prefecture: '富山県', type: 'university' },
  { name: '金沢大学附属病院', lat: 36.5481, lon: 136.7028, beds: 838, prefecture: '石川県', type: 'university' },
  { name: '福井大学医学部附属病院', lat: 36.0067, lon: 136.2189, beds: 600, prefecture: '福井県', type: 'university' },
  { name: '山梨大学医学部附属病院', lat: 35.6147, lon: 138.5836, beds: 618, prefecture: '山梨県', type: 'university' },
  { name: '岐阜大学医学部附属病院', lat: 35.4675, lon: 136.7361, beds: 614, prefecture: '岐阜県', type: 'university' },
  { name: '三重大学医学部附属病院', lat: 34.7444, lon: 136.5189, beds: 685, prefecture: '三重県', type: 'university' },
  { name: '滋賀医科大学医学部附属病院', lat: 35.0089, lon: 135.9628, beds: 612, prefecture: '滋賀県', type: 'university' },
  { name: '奈良県立医科大学附属病院', lat: 34.5350, lon: 135.7789, beds: 992, prefecture: '奈良県', type: 'university' },
  { name: '和歌山県立医科大学附属病院', lat: 34.2350, lon: 135.1822, beds: 800, prefecture: '和歌山県', type: 'university' },
  { name: '鳥取大学医学部附属病院', lat: 35.4886, lon: 133.3878, beds: 698, prefecture: '鳥取県', type: 'university' },
  { name: '島根大学医学部附属病院', lat: 35.4344, lon: 133.0769, beds: 612, prefecture: '島根県', type: 'university' },
  { name: '宮崎大学医学部附属病院', lat: 31.8331, lon: 131.4097, beds: 632, prefecture: '宮崎県', type: 'university' },
  { name: '大分大学医学部附属病院', lat: 33.1839, lon: 131.6503, beds: 618, prefecture: '大分県', type: 'university' },
  { name: '佐賀大学医学部附属病院', lat: 33.2706, lon: 130.2503, beds: 604, prefecture: '佐賀県', type: 'university' },
];

async function tryOverpass() {
  return fetchOverpassTiled(
    (bbox) => [
      `node["amenity"="hospital"](${bbox});`,
      `way["amenity"="hospital"](${bbox});`,
      `node["healthcare"="hospital"](${bbox});`,
      `way["healthcare"="hospital"](${bbox});`,
      `node["amenity"="clinic"](${bbox});`,
      `way["amenity"="clinic"](${bbox});`,
    ].join(''),
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        facility_id: `HOSP_${el.id}`,
        name: el.tags?.name || el.tags?.['name:en'] || 'Hospital',
        operator: el.tags?.operator || null,
        beds: parseInt(el.tags?.beds) || null,
        emergency: el.tags?.emergency || null,
        healthcare: el.tags?.healthcare || el.tags?.amenity || 'hospital',
        phone: el.tags?.phone || null,
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
    { queryTimeout: 180, timeoutMs: 90_000 },
  );
}

function generateSeedData() {
  const now = new Date();
  return SEED_HOSPITALS.map((h, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [h.lon, h.lat] },
    properties: {
      facility_id: `HOSP_${String(i + 1).padStart(5, '0')}`,
      name: h.name,
      beds: h.beds,
      hospital_type: h.type,
      prefecture: h.prefecture,
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'hospital_seed',
    },
  }));
}

export default async function collectHospitalMap() {
  let features = await tryOverpass();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'hospital_map',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Japan hospitals - major university and general hospitals',
    },
    metadata: {},
  };
}
