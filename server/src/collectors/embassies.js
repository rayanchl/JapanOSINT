/**
 * Embassies Collector
 * OSM Overpass diplomatic=embassy in Japan, with seed of Tokyo embassies as fallback.
 */

import { fetchOverpass } from './_liveHelpers.js';

const SEED_EMBASSIES = [
  { country: 'United States', country_ja: 'アメリカ合衆国', lat: 35.6691, lon: 139.7416 },
  { country: 'China', country_ja: '中華人民共和国', lat: 35.6597, lon: 139.7322 },
  { country: 'Russia', country_ja: 'ロシア連邦', lat: 35.6669, lon: 139.7332 },
  { country: 'United Kingdom', country_ja: 'イギリス', lat: 35.6856, lon: 139.7472 },
  { country: 'Germany', country_ja: 'ドイツ', lat: 35.6555, lon: 139.7280 },
  { country: 'France', country_ja: 'フランス', lat: 35.6577, lon: 139.7261 },
  { country: 'Italy', country_ja: 'イタリア', lat: 35.6520, lon: 139.7330 },
  { country: 'Canada', country_ja: 'カナダ', lat: 35.6708, lon: 139.7259 },
  { country: 'Australia', country_ja: 'オーストラリア', lat: 35.6422, lon: 139.7300 },
  { country: 'New Zealand', country_ja: 'ニュージーランド', lat: 35.6594, lon: 139.7186 },
  { country: 'India', country_ja: 'インド', lat: 35.6708, lon: 139.7475 },
  { country: 'South Korea', country_ja: '大韓民国', lat: 35.6692, lon: 139.7194 },
  { country: 'North Korea (general assoc)', country_ja: '朝鮮総連', lat: 35.7036, lon: 139.7308 },
  { country: 'Indonesia', country_ja: 'インドネシア', lat: 35.6586, lon: 139.7330 },
  { country: 'Philippines', country_ja: 'フィリピン', lat: 35.6517, lon: 139.7247 },
  { country: 'Thailand', country_ja: 'タイ', lat: 35.6444, lon: 139.7375 },
  { country: 'Vietnam', country_ja: 'ベトナム', lat: 35.6428, lon: 139.7064 },
  { country: 'Malaysia', country_ja: 'マレーシア', lat: 35.6469, lon: 139.7150 },
  { country: 'Singapore', country_ja: 'シンガポール', lat: 35.6606, lon: 139.7322 },
  { country: 'Brunei', country_ja: 'ブルネイ', lat: 35.6520, lon: 139.7400 },
  { country: 'Cambodia', country_ja: 'カンボジア', lat: 35.6694, lon: 139.6953 },
  { country: 'Laos', country_ja: 'ラオス', lat: 35.6492, lon: 139.7311 },
  { country: 'Myanmar', country_ja: 'ミャンマー', lat: 35.6736, lon: 139.7050 },
  { country: 'Mongolia', country_ja: 'モンゴル', lat: 35.6720, lon: 139.7444 },
  { country: 'Pakistan', country_ja: 'パキスタン', lat: 35.6444, lon: 139.6817 },
  { country: 'Bangladesh', country_ja: 'バングラデシュ', lat: 35.6447, lon: 139.7372 },
  { country: 'Sri Lanka', country_ja: 'スリランカ', lat: 35.6664, lon: 139.6786 },
  { country: 'Nepal', country_ja: 'ネパール', lat: 35.6447, lon: 139.7128 },
  { country: 'Iran', country_ja: 'イラン', lat: 35.6583, lon: 139.6833 },
  { country: 'Iraq', country_ja: 'イラク', lat: 35.6650, lon: 139.7239 },
  { country: 'Israel', country_ja: 'イスラエル', lat: 35.6747, lon: 139.7397 },
  { country: 'Saudi Arabia', country_ja: 'サウジアラビア', lat: 35.6728, lon: 139.7322 },
  { country: 'UAE', country_ja: 'アラブ首長国連邦', lat: 35.6447, lon: 139.7283 },
  { country: 'Qatar', country_ja: 'カタール', lat: 35.6447, lon: 139.7253 },
  { country: 'Kuwait', country_ja: 'クウェート', lat: 35.6608, lon: 139.7250 },
  { country: 'Türkiye', country_ja: 'トルコ', lat: 35.6553, lon: 139.7042 },
  { country: 'Egypt', country_ja: 'エジプト', lat: 35.6503, lon: 139.6997 },
  { country: 'South Africa', country_ja: '南アフリカ', lat: 35.6603, lon: 139.7339 },
  { country: 'Nigeria', country_ja: 'ナイジェリア', lat: 35.6803, lon: 139.7028 },
  { country: 'Kenya', country_ja: 'ケニア', lat: 35.6433, lon: 139.6531 },
  { country: 'Brazil', country_ja: 'ブラジル', lat: 35.6428, lon: 139.7314 },
  { country: 'Argentina', country_ja: 'アルゼンチン', lat: 35.6628, lon: 139.7150 },
  { country: 'Mexico', country_ja: 'メキシコ', lat: 35.6736, lon: 139.7297 },
  { country: 'Chile', country_ja: 'チリ', lat: 35.6686, lon: 139.7314 },
  { country: 'Peru', country_ja: 'ペルー', lat: 35.6683, lon: 139.7250 },
  { country: 'Cuba', country_ja: 'キューバ', lat: 35.6444, lon: 139.7244 },
  { country: 'Spain', country_ja: 'スペイン', lat: 35.6586, lon: 139.7158 },
  { country: 'Portugal', country_ja: 'ポルトガル', lat: 35.6553, lon: 139.7250 },
  { country: 'Netherlands', country_ja: 'オランダ', lat: 35.6792, lon: 139.7333 },
  { country: 'Belgium', country_ja: 'ベルギー', lat: 35.6803, lon: 139.7331 },
  { country: 'Sweden', country_ja: 'スウェーデン', lat: 35.6428, lon: 139.7253 },
  { country: 'Norway', country_ja: 'ノルウェー', lat: 35.6444, lon: 139.7128 },
  { country: 'Denmark', country_ja: 'デンマーク', lat: 35.6531, lon: 139.7322 },
  { country: 'Finland', country_ja: 'フィンランド', lat: 35.6586, lon: 139.7242 },
  { country: 'Switzerland', country_ja: 'スイス', lat: 35.6747, lon: 139.7414 },
  { country: 'Austria', country_ja: 'オーストリア', lat: 35.6608, lon: 139.7253 },
  { country: 'Poland', country_ja: 'ポーランド', lat: 35.6536, lon: 139.7261 },
  { country: 'Czech Republic', country_ja: 'チェコ', lat: 35.6539, lon: 139.7275 },
  { country: 'Hungary', country_ja: 'ハンガリー', lat: 35.6553, lon: 139.7286 },
  { country: 'Romania', country_ja: 'ルーマニア', lat: 35.6447, lon: 139.7197 },
  { country: 'Greece', country_ja: 'ギリシャ', lat: 35.6444, lon: 139.7178 },
  { country: 'Ukraine', country_ja: 'ウクライナ', lat: 35.6553, lon: 139.7144 },
  { country: 'Holy See (Vatican)', country_ja: 'バチカン', lat: 35.6736, lon: 139.7372 },
];

async function tryOverpass() {
  return fetchOverpass(
    'node["diplomatic"="embassy"](area.jp);way["diplomatic"="embassy"](area.jp);',
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        embassy_id: `OSM_${el.id}`,
        name: el.tags?.name || 'Embassy',
        country: el.tags?.country || el.tags?.['target:country'] || 'unknown',
        source: 'osm_overpass',
      },
    }),
  );
}

function generateSeedData() {
  return SEED_EMBASSIES.map((e, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [e.lon, e.lat] },
    properties: {
      embassy_id: `EMB_${String(i + 1).padStart(5, '0')}`,
      name: `Embassy of ${e.country}`,
      country: e.country,
      country_ja: e.country_ja,
      city: 'Tokyo',
      source: 'embassies_seed',
    },
  }));
}

export default async function collectEmbassies() {
  let features = await tryOverpass();
  const live = !!(features && features.length > 0);
  if (!live) features = [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'embassies',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Foreign embassies in Japan (concentrated in Tokyo)',
    },
  };
}
