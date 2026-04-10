/**
 * Hi-net Seismic Station Collector
 * Maps NIED Hi-net (high-sensitivity) seismograph stations across Japan.
 * Falls back to a curated seed of major Hi-net observation stations.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return await fetchOverpass(
    'node["man_made"="monitoring_station"]["monitoring:seismic_activity"="yes"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        station_id: `HINET_LIVE_${String(i + 1).padStart(4, '0')}`,
        code: el.tags?.ref || el.tags?.name || `H${el.id}`,
        name: el.tags?.name || el.tags?.['name:en'] || `Hi-net ${el.id}`,
        depth_m: null,
        network: 'Hi-net',
        operator: el.tags?.operator || 'NIED',
        prefecture: el.tags?.['addr:state'] || null,
        country: 'JP',
        updated_at: new Date().toISOString(),
        source: 'hinet_live',
      },
    })
  );
}

const SEED_HINET = [
  // Hokkaido
  { code: 'N.WTNH', name: '稚内', lat: 45.4047, lon: 141.6792, depth_m: 100, prefecture: '北海道' },
  { code: 'N.RBSH', name: '利尻', lat: 45.1819, lon: 141.2358, depth_m: 100, prefecture: '北海道' },
  { code: 'N.NMRH', name: '名寄', lat: 44.3603, lon: 142.4633, depth_m: 100, prefecture: '北海道' },
  { code: 'N.MNBH', name: '紋別', lat: 44.3464, lon: 143.3536, depth_m: 100, prefecture: '北海道' },
  { code: 'N.ABSH', name: '網走', lat: 44.0136, lon: 144.2700, depth_m: 100, prefecture: '北海道' },
  { code: 'N.RUMH', name: '留萌', lat: 43.9456, lon: 141.6358, depth_m: 100, prefecture: '北海道' },
  { code: 'N.SBKH', name: '士別', lat: 44.1786, lon: 142.4006, depth_m: 100, prefecture: '北海道' },
  { code: 'N.IBRH', name: '岩見沢', lat: 43.1956, lon: 141.7758, depth_m: 100, prefecture: '北海道' },
  { code: 'N.TKMH', name: '苫小牧', lat: 42.6342, lon: 141.6047, depth_m: 100, prefecture: '北海道' },
  { code: 'N.HSSH', name: '函館', lat: 41.7686, lon: 140.7286, depth_m: 100, prefecture: '北海道' },
  { code: 'N.MUOH', name: '室蘭', lat: 42.3158, lon: 140.9742, depth_m: 100, prefecture: '北海道' },
  { code: 'N.KSRH', name: '釧路', lat: 42.9849, lon: 144.3819, depth_m: 100, prefecture: '北海道' },
  { code: 'N.NEMH', name: '根室', lat: 43.3306, lon: 145.5828, depth_m: 100, prefecture: '北海道' },

  // Tohoku
  { code: 'N.AOMH', name: '青森', lat: 40.8244, lon: 140.7400, depth_m: 100, prefecture: '青森県' },
  { code: 'N.HACH', name: '八戸', lat: 40.5125, lon: 141.4886, depth_m: 100, prefecture: '青森県' },
  { code: 'N.HRSH', name: '弘前', lat: 40.6033, lon: 140.4644, depth_m: 100, prefecture: '青森県' },
  { code: 'N.MRKH', name: '盛岡', lat: 39.7036, lon: 141.1525, depth_m: 100, prefecture: '岩手県' },
  { code: 'N.MIYH', name: '宮古', lat: 39.6411, lon: 141.9525, depth_m: 100, prefecture: '岩手県' },
  { code: 'N.KAMH', name: '釜石', lat: 39.2756, lon: 141.8856, depth_m: 100, prefecture: '岩手県' },
  { code: 'N.SDMH', name: '仙台', lat: 38.2683, lon: 140.8719, depth_m: 100, prefecture: '宮城県' },
  { code: 'N.ISKH', name: '石巻', lat: 38.4344, lon: 141.3028, depth_m: 100, prefecture: '宮城県' },
  { code: 'N.KGNH', name: '気仙沼', lat: 38.9067, lon: 141.5700, depth_m: 100, prefecture: '宮城県' },
  { code: 'N.AKTH', name: '秋田', lat: 39.7186, lon: 140.1024, depth_m: 100, prefecture: '秋田県' },
  { code: 'N.OGAH', name: '男鹿', lat: 39.8839, lon: 139.8458, depth_m: 100, prefecture: '秋田県' },
  { code: 'N.YMTH', name: '山形', lat: 38.2403, lon: 140.3633, depth_m: 100, prefecture: '山形県' },
  { code: 'N.SAKH', name: '酒田', lat: 38.9144, lon: 139.8369, depth_m: 100, prefecture: '山形県' },
  { code: 'N.FKSH', name: '福島', lat: 37.7503, lon: 140.4675, depth_m: 100, prefecture: '福島県' },
  { code: 'N.IWKH', name: 'いわき', lat: 37.0506, lon: 140.8867, depth_m: 100, prefecture: '福島県' },
  { code: 'N.AIZH', name: '会津', lat: 37.4944, lon: 139.9303, depth_m: 100, prefecture: '福島県' },

  // Kanto
  { code: 'N.MTOH', name: '水戸', lat: 36.3658, lon: 140.4711, depth_m: 200, prefecture: '茨城県' },
  { code: 'N.TKBH', name: '筑波', lat: 36.0833, lon: 140.1167, depth_m: 200, prefecture: '茨城県' },
  { code: 'N.UTNH', name: '宇都宮', lat: 36.5658, lon: 139.8836, depth_m: 200, prefecture: '栃木県' },
  { code: 'N.NIKH', name: '日光', lat: 36.7195, lon: 139.6986, depth_m: 200, prefecture: '栃木県' },
  { code: 'N.MAEH', name: '前橋', lat: 36.3911, lon: 139.0608, depth_m: 200, prefecture: '群馬県' },
  { code: 'N.NUMH', name: '沼田', lat: 36.6447, lon: 139.0444, depth_m: 200, prefecture: '群馬県' },
  { code: 'N.URWH', name: '浦和', lat: 35.8617, lon: 139.6455, depth_m: 200, prefecture: '埼玉県' },
  { code: 'N.CHBH', name: '千葉', lat: 35.6083, lon: 140.1233, depth_m: 200, prefecture: '千葉県' },
  { code: 'N.CHCH', name: '銚子', lat: 35.7344, lon: 140.8267, depth_m: 200, prefecture: '千葉県' },
  { code: 'N.TKYH', name: '東京', lat: 35.6812, lon: 139.7671, depth_m: 200, prefecture: '東京都' },
  { code: 'N.HCJH', name: '八王子', lat: 35.6664, lon: 139.3158, depth_m: 200, prefecture: '東京都' },
  { code: 'N.YKHH', name: '横浜', lat: 35.4437, lon: 139.6380, depth_m: 200, prefecture: '神奈川県' },
  { code: 'N.OYMH', name: '小田原', lat: 35.2566, lon: 139.1592, depth_m: 200, prefecture: '神奈川県' },
  { code: 'N.HKWH', name: '箱根', lat: 35.2333, lon: 139.0250, depth_m: 200, prefecture: '神奈川県' },

  // Chubu
  { code: 'N.NGNH', name: '長野', lat: 36.6489, lon: 138.1944, depth_m: 100, prefecture: '長野県' },
  { code: 'N.MTSH', name: '松本', lat: 36.2380, lon: 137.9719, depth_m: 100, prefecture: '長野県' },
  { code: 'N.NIIH', name: '新潟', lat: 37.9161, lon: 139.0364, depth_m: 100, prefecture: '新潟県' },
  { code: 'N.NGOH', name: '長岡', lat: 37.4456, lon: 138.8517, depth_m: 100, prefecture: '新潟県' },
  { code: 'N.JOEH', name: '上越', lat: 37.1486, lon: 138.2367, depth_m: 100, prefecture: '新潟県' },
  { code: 'N.SADH', name: '佐渡', lat: 38.0181, lon: 138.3683, depth_m: 100, prefecture: '新潟県' },
  { code: 'N.TYMH', name: '富山', lat: 36.6953, lon: 137.2113, depth_m: 100, prefecture: '富山県' },
  { code: 'N.KZWH', name: '金沢', lat: 36.5613, lon: 136.6562, depth_m: 100, prefecture: '石川県' },
  { code: 'N.WJMH', name: '輪島', lat: 37.3919, lon: 136.8989, depth_m: 100, prefecture: '石川県' },
  { code: 'N.FKIH', name: '福井', lat: 36.0613, lon: 136.2229, depth_m: 100, prefecture: '福井県' },
  { code: 'N.GIFH', name: '岐阜', lat: 35.4233, lon: 136.7606, depth_m: 100, prefecture: '岐阜県' },
  { code: 'N.TKYH', name: '高山', lat: 36.1458, lon: 137.2519, depth_m: 100, prefecture: '岐阜県' },
  { code: 'N.SZUH', name: '静岡', lat: 34.9756, lon: 138.3828, depth_m: 100, prefecture: '静岡県' },
  { code: 'N.HMMH', name: '浜松', lat: 34.7108, lon: 137.7261, depth_m: 100, prefecture: '静岡県' },
  { code: 'N.IZUH', name: '伊豆', lat: 34.9722, lon: 139.0989, depth_m: 100, prefecture: '静岡県' },
  { code: 'N.NGYH', name: '名古屋', lat: 35.1814, lon: 136.9069, depth_m: 100, prefecture: '愛知県' },
  { code: 'N.TYHH', name: '豊橋', lat: 34.7692, lon: 137.3914, depth_m: 100, prefecture: '愛知県' },

  // Kansai
  { code: 'N.TSUH', name: '津', lat: 34.7184, lon: 136.5067, depth_m: 100, prefecture: '三重県' },
  { code: 'N.OWAH', name: '尾鷲', lat: 34.0700, lon: 136.1900, depth_m: 100, prefecture: '三重県' },
  { code: 'N.OTSH', name: '大津', lat: 35.0044, lon: 135.8686, depth_m: 100, prefecture: '滋賀県' },
  { code: 'N.KYTH', name: '京都', lat: 35.0116, lon: 135.7681, depth_m: 100, prefecture: '京都府' },
  { code: 'N.MIHH', name: '舞鶴', lat: 35.4500, lon: 135.3331, depth_m: 100, prefecture: '京都府' },
  { code: 'N.OSKH', name: '大阪', lat: 34.6864, lon: 135.5197, depth_m: 100, prefecture: '大阪府' },
  { code: 'N.KOBH', name: '神戸', lat: 34.6913, lon: 135.1830, depth_m: 100, prefecture: '兵庫県' },
  { code: 'N.HMJH', name: '姫路', lat: 34.8167, lon: 134.6856, depth_m: 100, prefecture: '兵庫県' },
  { code: 'N.NRAH', name: '奈良', lat: 34.6850, lon: 135.8048, depth_m: 100, prefecture: '奈良県' },
  { code: 'N.WKYH', name: '和歌山', lat: 34.2261, lon: 135.1675, depth_m: 100, prefecture: '和歌山県' },
  { code: 'N.SHGH', name: '潮岬', lat: 33.4514, lon: 135.7619, depth_m: 100, prefecture: '和歌山県' },

  // Chugoku / Shikoku
  { code: 'N.TTRH', name: '鳥取', lat: 35.5036, lon: 134.2356, depth_m: 100, prefecture: '鳥取県' },
  { code: 'N.MTUH', name: '松江', lat: 35.4722, lon: 133.0506, depth_m: 100, prefecture: '島根県' },
  { code: 'N.OKYH', name: '岡山', lat: 34.6628, lon: 133.9197, depth_m: 100, prefecture: '岡山県' },
  { code: 'N.HIRH', name: '広島', lat: 34.3853, lon: 132.4553, depth_m: 100, prefecture: '広島県' },
  { code: 'N.YMGH', name: '山口', lat: 34.1856, lon: 131.4714, depth_m: 100, prefecture: '山口県' },
  { code: 'N.TKSH', name: '徳島', lat: 34.0658, lon: 134.5594, depth_m: 100, prefecture: '徳島県' },
  { code: 'N.TKMH', name: '高松', lat: 34.3401, lon: 134.0434, depth_m: 100, prefecture: '香川県' },
  { code: 'N.MTYH', name: '松山', lat: 33.8392, lon: 132.7656, depth_m: 100, prefecture: '愛媛県' },
  { code: 'N.KCIH', name: '高知', lat: 33.5594, lon: 133.5311, depth_m: 100, prefecture: '高知県' },
  { code: 'N.MROH', name: '室戸', lat: 33.2839, lon: 134.1764, depth_m: 100, prefecture: '高知県' },
  { code: 'N.ASUH', name: '足摺', lat: 32.7233, lon: 133.0125, depth_m: 100, prefecture: '高知県' },

  // Kyushu / Okinawa
  { code: 'N.FKOH', name: '福岡', lat: 33.5904, lon: 130.4017, depth_m: 100, prefecture: '福岡県' },
  { code: 'N.KKQH', name: '北九州', lat: 33.8836, lon: 130.8814, depth_m: 100, prefecture: '福岡県' },
  { code: 'N.SAGH', name: '佐賀', lat: 33.2494, lon: 130.2989, depth_m: 100, prefecture: '佐賀県' },
  { code: 'N.NGSH', name: '長崎', lat: 32.7503, lon: 129.8775, depth_m: 100, prefecture: '長崎県' },
  { code: 'N.GTOH', name: '五島', lat: 32.6953, lon: 128.8417, depth_m: 100, prefecture: '長崎県' },
  { code: 'N.KMTH', name: '熊本', lat: 32.8019, lon: 130.7256, depth_m: 100, prefecture: '熊本県' },
  { code: 'N.AMKH', name: '天草', lat: 32.4541, lon: 130.1958, depth_m: 100, prefecture: '熊本県' },
  { code: 'N.OITH', name: '大分', lat: 33.2381, lon: 131.6126, depth_m: 100, prefecture: '大分県' },
  { code: 'N.MZKH', name: '宮崎', lat: 31.9111, lon: 131.4239, depth_m: 100, prefecture: '宮崎県' },
  { code: 'N.NBOH', name: '延岡', lat: 32.5814, lon: 131.6647, depth_m: 100, prefecture: '宮崎県' },
  { code: 'N.KGSH', name: '鹿児島', lat: 31.5963, lon: 130.5571, depth_m: 100, prefecture: '鹿児島県' },
  { code: 'N.YKUH', name: '屋久島', lat: 30.4408, lon: 130.6580, depth_m: 100, prefecture: '鹿児島県' },
  { code: 'N.AMJH', name: '奄美', lat: 28.3786, lon: 129.5008, depth_m: 100, prefecture: '鹿児島県' },
  { code: 'N.NAHH', name: '那覇', lat: 26.2125, lon: 127.6809, depth_m: 100, prefecture: '沖縄県' },
  { code: 'N.MIYH', name: '宮古島', lat: 24.8053, lon: 125.2811, depth_m: 100, prefecture: '沖縄県' },
  { code: 'N.ISGH', name: '石垣島', lat: 24.3403, lon: 124.1556, depth_m: 100, prefecture: '沖縄県' },
];

function generateSeedData() {
  const now = new Date();
  return SEED_HINET.map((s, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [s.lon, s.lat] },
    properties: {
      station_id: `HINET_${String(i + 1).padStart(4, '0')}`,
      code: s.code,
      name: s.name,
      depth_m: s.depth_m,
      network: 'Hi-net',
      operator: 'NIED',
      prefecture: s.prefecture,
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'hinet_seed',
    },
  }));
}

export default async function collectHiNet() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'hi_net',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'NIED Hi-net high-sensitivity seismograph stations',
    },
    metadata: {},
  };
}
