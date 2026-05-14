/**
 * K-NET Strong Motion Station Collector
 * Maps NIED K-NET (strong motion) seismograph stations across Japan.
 * Falls back to a curated seed of major K-NET observation stations.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return await fetchOverpass(
    'node["man_made"="monitoring_station"]["monitoring:strong_motion"="yes"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        station_id: `KNET_LIVE_${String(i + 1).padStart(4, '0')}`,
        code: el.tags?.ref || el.tags?.name || `K${el.id}`,
        name: el.tags?.name || el.tags?.['name:en'] || `K-NET ${el.id}`,
        network: 'K-NET',
        operator: el.tags?.operator || 'NIED',
        prefecture: el.tags?.['addr:state'] || null,
        country: 'JP',
        updated_at: new Date().toISOString(),
        source: 'knet_live',
      },
    })
  );
}

const SEED_KNET = [
  // Hokkaido (~10)
  { code: 'HKD001', name: 'Wakkanai', lat: 45.4181, lon: 141.6786, prefecture: '北海道' },
  { code: 'HKD025', name: 'Asahikawa', lat: 43.7706, lon: 142.3650, prefecture: '北海道' },
  { code: 'HKD052', name: 'Kushiro', lat: 42.9849, lon: 144.3819, prefecture: '北海道' },
  { code: 'HKD071', name: 'Sapporo', lat: 43.0628, lon: 141.3478, prefecture: '北海道' },
  { code: 'HKD098', name: 'Tomakomai', lat: 42.6342, lon: 141.6047, prefecture: '北海道' },
  { code: 'HKD120', name: 'Hakodate', lat: 41.7686, lon: 140.7286, prefecture: '北海道' },
  { code: 'HKD036', name: 'Obihiro', lat: 42.9239, lon: 143.1953, prefecture: '北海道' },

  // Tohoku (~12)
  { code: 'AOM001', name: 'Aomori', lat: 40.8244, lon: 140.7400, prefecture: '青森県' },
  { code: 'AOM006', name: 'Hachinohe', lat: 40.5125, lon: 141.4886, prefecture: '青森県' },
  { code: 'IWT003', name: 'Morioka', lat: 39.7036, lon: 141.1525, prefecture: '岩手県' },
  { code: 'IWT012', name: 'Miyako', lat: 39.6411, lon: 141.9525, prefecture: '岩手県' },
  { code: 'IWT020', name: 'Kamaishi', lat: 39.2756, lon: 141.8856, prefecture: '岩手県' },
  { code: 'MYG004', name: 'Sendai', lat: 38.2683, lon: 140.8719, prefecture: '宮城県' },
  { code: 'MYG010', name: 'Ishinomaki', lat: 38.4344, lon: 141.3028, prefecture: '宮城県' },
  { code: 'AKT002', name: 'Akita', lat: 39.7186, lon: 140.1024, prefecture: '秋田県' },
  { code: 'YMT003', name: 'Yamagata', lat: 38.2403, lon: 140.3633, prefecture: '山形県' },
  { code: 'FKS001', name: 'Fukushima', lat: 37.7503, lon: 140.4675, prefecture: '福島県' },
  { code: 'FKS010', name: 'Iwaki', lat: 37.0506, lon: 140.8867, prefecture: '福島県' },

  // Kanto (~14)
  { code: 'IBR003', name: 'Mito', lat: 36.3658, lon: 140.4711, prefecture: '茨城県' },
  { code: 'IBR011', name: 'Tsukuba', lat: 36.0833, lon: 140.1167, prefecture: '茨城県' },
  { code: 'TCG003', name: 'Utsunomiya', lat: 36.5658, lon: 139.8836, prefecture: '栃木県' },
  { code: 'GNM004', name: 'Maebashi', lat: 36.3911, lon: 139.0608, prefecture: '群馬県' },
  { code: 'STM005', name: 'Saitama', lat: 35.8617, lon: 139.6455, prefecture: '埼玉県' },
  { code: 'CHB004', name: 'Chiba', lat: 35.6083, lon: 140.1233, prefecture: '千葉県' },
  { code: 'CHB008', name: 'Choshi', lat: 35.7344, lon: 140.8267, prefecture: '千葉県' },
  { code: 'TKY007', name: 'Tokyo', lat: 35.6812, lon: 139.7671, prefecture: '東京都' },
  { code: 'TKY014', name: 'Hachioji', lat: 35.6664, lon: 139.3158, prefecture: '東京都' },
  { code: 'KNG002', name: 'Yokohama', lat: 35.4437, lon: 139.6380, prefecture: '神奈川県' },
  { code: 'KNG009', name: 'Odawara', lat: 35.2566, lon: 139.1592, prefecture: '神奈川県' },

  // Chubu (~14)
  { code: 'NIG013', name: 'Niigata', lat: 37.9161, lon: 139.0364, prefecture: '新潟県' },
  { code: 'NIG019', name: 'Nagaoka', lat: 37.4456, lon: 138.8517, prefecture: '新潟県' },
  { code: 'TYM003', name: 'Toyama', lat: 36.6953, lon: 137.2113, prefecture: '富山県' },
  { code: 'ISK006', name: 'Kanazawa', lat: 36.5613, lon: 136.6562, prefecture: '石川県' },
  { code: 'ISK001', name: 'Wajima', lat: 37.3919, lon: 136.8989, prefecture: '石川県' },
  { code: 'FKI004', name: 'Fukui', lat: 36.0613, lon: 136.2229, prefecture: '福井県' },
  { code: 'YMN005', name: 'Kofu', lat: 35.6642, lon: 138.5683, prefecture: '山梨県' },
  { code: 'NGN005', name: 'Nagano', lat: 36.6489, lon: 138.1944, prefecture: '長野県' },
  { code: 'NGN013', name: 'Matsumoto', lat: 36.2380, lon: 137.9719, prefecture: '長野県' },
  { code: 'GIF005', name: 'Gifu', lat: 35.4233, lon: 136.7606, prefecture: '岐阜県' },
  { code: 'SZO014', name: 'Shizuoka', lat: 34.9756, lon: 138.3828, prefecture: '静岡県' },
  { code: 'SZO024', name: 'Hamamatsu', lat: 34.7108, lon: 137.7261, prefecture: '静岡県' },
  { code: 'AIC004', name: 'Nagoya', lat: 35.1814, lon: 136.9069, prefecture: '愛知県' },
  { code: 'AIC013', name: 'Toyohashi', lat: 34.7692, lon: 137.3914, prefecture: '愛知県' },

  // Kansai (~10)
  { code: 'MIE003', name: 'Tsu', lat: 34.7184, lon: 136.5067, prefecture: '三重県' },
  { code: 'MIE013', name: 'Owase', lat: 34.0700, lon: 136.1900, prefecture: '三重県' },
  { code: 'SIG002', name: 'Otsu', lat: 35.0044, lon: 135.8686, prefecture: '滋賀県' },
  { code: 'KYT004', name: 'Kyoto', lat: 35.0116, lon: 135.7681, prefecture: '京都府' },
  { code: 'OSK005', name: 'Osaka', lat: 34.6864, lon: 135.5197, prefecture: '大阪府' },
  { code: 'HYG001', name: 'Kobe', lat: 34.6913, lon: 135.1830, prefecture: '兵庫県' },
  { code: 'HYG009', name: 'Himeji', lat: 34.8167, lon: 134.6856, prefecture: '兵庫県' },
  { code: 'NRA003', name: 'Nara', lat: 34.6850, lon: 135.8048, prefecture: '奈良県' },
  { code: 'WKY004', name: 'Wakayama', lat: 34.2261, lon: 135.1675, prefecture: '和歌山県' },
  { code: 'WKY013', name: 'Shionomisaki', lat: 33.4514, lon: 135.7619, prefecture: '和歌山県' },

  // Chugoku/Shikoku (~12)
  { code: 'TTR005', name: 'Tottori', lat: 35.5036, lon: 134.2356, prefecture: '鳥取県' },
  { code: 'SMN006', name: 'Matsue', lat: 35.4722, lon: 133.0506, prefecture: '島根県' },
  { code: 'OKY005', name: 'Okayama', lat: 34.6628, lon: 133.9197, prefecture: '岡山県' },
  { code: 'HRS010', name: 'Hiroshima', lat: 34.3853, lon: 132.4553, prefecture: '広島県' },
  { code: 'YMG004', name: 'Yamaguchi', lat: 34.1856, lon: 131.4714, prefecture: '山口県' },
  { code: 'TKS003', name: 'Tokushima', lat: 34.0658, lon: 134.5594, prefecture: '徳島県' },
  { code: 'KGW003', name: 'Takamatsu', lat: 34.3401, lon: 134.0434, prefecture: '香川県' },
  { code: 'EHM007', name: 'Matsuyama', lat: 33.8392, lon: 132.7656, prefecture: '愛媛県' },
  { code: 'KOC004', name: 'Kochi', lat: 33.5594, lon: 133.5311, prefecture: '高知県' },
  { code: 'KOC013', name: 'Muroto', lat: 33.2839, lon: 134.1764, prefecture: '高知県' },
  { code: 'KOC016', name: 'Ashizuri', lat: 32.7233, lon: 133.0125, prefecture: '高知県' },

  // Kyushu/Okinawa (~14)
  { code: 'FKO003', name: 'Fukuoka', lat: 33.5904, lon: 130.4017, prefecture: '福岡県' },
  { code: 'FKO014', name: 'Kitakyushu', lat: 33.8836, lon: 130.8814, prefecture: '福岡県' },
  { code: 'SAG002', name: 'Saga', lat: 33.2494, lon: 130.2989, prefecture: '佐賀県' },
  { code: 'NGS001', name: 'Nagasaki', lat: 32.7503, lon: 129.8775, prefecture: '長崎県' },
  { code: 'KMM006', name: 'Kumamoto', lat: 32.8019, lon: 130.7256, prefecture: '熊本県' },
  { code: 'KMM010', name: 'Mashiki', lat: 32.7867, lon: 130.8133, prefecture: '熊本県' },
  { code: 'OIT001', name: 'Oita', lat: 33.2381, lon: 131.6126, prefecture: '大分県' },
  { code: 'MYZ006', name: 'Miyazaki', lat: 31.9111, lon: 131.4239, prefecture: '宮崎県' },
  { code: 'MYZ002', name: 'Nobeoka', lat: 32.5814, lon: 131.6647, prefecture: '宮崎県' },
  { code: 'KGS002', name: 'Kagoshima', lat: 31.5963, lon: 130.5571, prefecture: '鹿児島県' },
  { code: 'KGS012', name: 'Tanegashima', lat: 30.7311, lon: 130.9981, prefecture: '鹿児島県' },
  { code: 'KGS017', name: 'Amami', lat: 28.3786, lon: 129.5008, prefecture: '鹿児島県' },
  { code: 'OKW003', name: 'Naha', lat: 26.2125, lon: 127.6809, prefecture: '沖縄県' },
  { code: 'OKW010', name: 'Miyako', lat: 24.8053, lon: 125.2811, prefecture: '沖縄県' },
  { code: 'OKW015', name: 'Ishigaki', lat: 24.3403, lon: 124.1556, prefecture: '沖縄県' },
];

function generateSeedData() {
  const now = new Date();
  return SEED_KNET.map((s, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [s.lon, s.lat] },
    properties: {
      station_id: `KNET_${String(i + 1).padStart(4, '0')}`,
      code: s.code,
      name: s.name,
      network: 'K-NET',
      operator: 'NIED',
      sensor: 'strong_motion',
      prefecture: s.prefecture,
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'knet_seed',
    },
  }));
}

export default async function collectKNet() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'k_net',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'NIED K-NET strong motion seismograph stations',
    },
  };
}
