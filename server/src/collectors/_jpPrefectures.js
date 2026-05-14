/**
 * Japan prefecture lookup table.
 *
 * Single source of truth for prefecture-level OSINT collectors that need
 * `{ code, en, ja, lat, lon }` for all 47 prefectures plus the URL of the
 * prefectural police force's public homepage.
 *
 * `code` is the JIS X 0401 numeric prefecture code (01–47).
 * `lat`/`lon` are administrative-centre centroids (rough, not centroids of
 * the polygon — adequate for prefecture-scale rendering).
 * `policePortal` is the URL of the prefectural police force's public homepage.
 * Each force publishes its statistics at a different deep path that rotates
 * over time; pointing at the homepage keeps the directory stable and lets
 * users navigate to the `犯罪統計` / `統計資料` section themselves.
 */

export const JP_PREFECTURES = [
  { code: '01', en: 'Hokkaido',   ja: '北海道',   lat: 43.0628, lon: 141.3478, policePortal: 'https://www.police.pref.hokkaido.lg.jp/' },
  { code: '02', en: 'Aomori',     ja: '青森県',   lat: 40.8244, lon: 140.7400, policePortal: 'https://www.police.pref.aomori.jp/' },
  { code: '03', en: 'Iwate',      ja: '岩手県',   lat: 39.7036, lon: 141.1525, policePortal: 'https://www.pref.iwate.jp/kenkei/' },
  { code: '04', en: 'Miyagi',     ja: '宮城県',   lat: 38.2683, lon: 140.8719, policePortal: 'https://www.police.pref.miyagi.jp/' },
  { code: '05', en: 'Akita',      ja: '秋田県',   lat: 39.7186, lon: 140.1024, policePortal: 'https://www.police.pref.akita.lg.jp/' },
  { code: '06', en: 'Yamagata',   ja: '山形県',   lat: 38.2403, lon: 140.3633, policePortal: 'https://www.pref.yamagata.jp/police/' },
  { code: '07', en: 'Fukushima',  ja: '福島県',   lat: 37.7503, lon: 140.4675, policePortal: 'https://www.police.pref.fukushima.jp/' },
  { code: '08', en: 'Ibaraki',    ja: '茨城県',   lat: 36.3658, lon: 140.4711, policePortal: 'https://www.pref.ibaraki.jp/keisatsu/' },
  { code: '09', en: 'Tochigi',    ja: '栃木県',   lat: 36.5658, lon: 139.8836, policePortal: 'https://www.pref.tochigi.lg.jp/keisatu/' },
  { code: '10', en: 'Gunma',      ja: '群馬県',   lat: 36.3911, lon: 139.0608, policePortal: 'https://www.police.pref.gunma.jp/' },
  { code: '11', en: 'Saitama',    ja: '埼玉県',   lat: 35.8617, lon: 139.6455, policePortal: 'https://www.police.pref.saitama.lg.jp/' },
  { code: '12', en: 'Chiba',      ja: '千葉県',   lat: 35.6083, lon: 140.1233, policePortal: 'https://www.police.pref.chiba.jp/' },
  { code: '13', en: 'Tokyo',      ja: '東京都',   lat: 35.6895, lon: 139.6917, policePortal: 'https://www.keishicho.metro.tokyo.lg.jp/' },
  { code: '14', en: 'Kanagawa',   ja: '神奈川県', lat: 35.4437, lon: 139.6380, policePortal: 'https://www.police.pref.kanagawa.jp/' },
  { code: '15', en: 'Niigata',    ja: '新潟県',   lat: 37.9161, lon: 139.0364, policePortal: 'https://www.pref.niigata.lg.jp/site/kenkei/' },
  { code: '16', en: 'Toyama',     ja: '富山県',   lat: 36.6953, lon: 137.2113, policePortal: 'https://www.pref.toyama.jp/3500/kurashi/anzen/anzen/seian/seian/index.html' },
  { code: '17', en: 'Ishikawa',   ja: '石川県',   lat: 36.5946, lon: 136.6256, policePortal: 'https://www.pref.ishikawa.lg.jp/police/' },
  { code: '18', en: 'Fukui',      ja: '福井県',   lat: 36.0652, lon: 136.2216, policePortal: 'https://www.pref.fukui.lg.jp/doc/kenkei/' },
  { code: '19', en: 'Yamanashi',  ja: '山梨県',   lat: 35.6642, lon: 138.5683, policePortal: 'https://www.pref.yamanashi.jp/police/' },
  { code: '20', en: 'Nagano',     ja: '長野県',   lat: 36.6489, lon: 138.1944, policePortal: 'https://www.pref.nagano.lg.jp/police/' },
  { code: '21', en: 'Gifu',       ja: '岐阜県',   lat: 35.4233, lon: 136.7606, policePortal: 'https://www.pref.gifu.lg.jp/keisatsu/' },
  { code: '22', en: 'Shizuoka',   ja: '静岡県',   lat: 34.9756, lon: 138.3828, policePortal: 'https://www.pref.shizuoka.jp/police/' },
  { code: '23', en: 'Aichi',      ja: '愛知県',   lat: 35.1814, lon: 136.9069, policePortal: 'https://www.pref.aichi.jp/police/' },
  { code: '24', en: 'Mie',        ja: '三重県',   lat: 34.7184, lon: 136.5067, policePortal: 'https://www.police.pref.mie.jp/' },
  { code: '25', en: 'Shiga',      ja: '滋賀県',   lat: 35.0044, lon: 135.8686, policePortal: 'https://www.pref.shiga.lg.jp/police/' },
  { code: '26', en: 'Kyoto',      ja: '京都府',   lat: 35.0116, lon: 135.7681, policePortal: 'https://www.pref.kyoto.jp/fukei/' },
  { code: '27', en: 'Osaka',      ja: '大阪府',   lat: 34.6864, lon: 135.5197, policePortal: 'https://www.police.pref.osaka.lg.jp/' },
  { code: '28', en: 'Hyogo',      ja: '兵庫県',   lat: 34.6913, lon: 135.1830, policePortal: 'https://www.police.pref.hyogo.lg.jp/' },
  { code: '29', en: 'Nara',       ja: '奈良県',   lat: 34.6850, lon: 135.8048, policePortal: 'https://www.police.pref.nara.jp/' },
  { code: '30', en: 'Wakayama',   ja: '和歌山県', lat: 34.2261, lon: 135.1675, policePortal: 'https://www.police.pref.wakayama.lg.jp/' },
  { code: '31', en: 'Tottori',    ja: '鳥取県',   lat: 35.5039, lon: 134.2378, policePortal: 'https://www.pref.tottori.lg.jp/keisatsu/' },
  { code: '32', en: 'Shimane',    ja: '島根県',   lat: 35.4722, lon: 133.0506, policePortal: 'https://www.pref.shimane.lg.jp/police/' },
  { code: '33', en: 'Okayama',    ja: '岡山県',   lat: 34.6628, lon: 133.9197, policePortal: 'https://www.pref.okayama.jp/site/kenkei/' },
  { code: '34', en: 'Hiroshima',  ja: '広島県',   lat: 34.3853, lon: 132.4553, policePortal: 'https://www.pref.hiroshima.lg.jp/site/police/' },
  { code: '35', en: 'Yamaguchi',  ja: '山口県',   lat: 34.1856, lon: 131.4714, policePortal: 'https://www.police.pref.yamaguchi.jp/' },
  { code: '36', en: 'Tokushima',  ja: '徳島県',   lat: 34.0658, lon: 134.5594, policePortal: 'https://www.pref.tokushima.lg.jp/site/police/' },
  { code: '37', en: 'Kagawa',     ja: '香川県',   lat: 34.3401, lon: 134.0434, policePortal: 'https://www.pref.kagawa.lg.jp/police/' },
  { code: '38', en: 'Ehime',      ja: '愛媛県',   lat: 33.8392, lon: 132.7656, policePortal: 'https://www.police.pref.ehime.jp/' },
  { code: '39', en: 'Kochi',      ja: '高知県',   lat: 33.5594, lon: 133.5311, policePortal: 'https://www.police.pref.kochi.lg.jp/' },
  { code: '40', en: 'Fukuoka',    ja: '福岡県',   lat: 33.5904, lon: 130.4017, policePortal: 'https://www.police.pref.fukuoka.jp/' },
  { code: '41', en: 'Saga',       ja: '佐賀県',   lat: 33.2494, lon: 130.2989, policePortal: 'https://www.police.pref.saga.jp/' },
  { code: '42', en: 'Nagasaki',   ja: '長崎県',   lat: 32.7503, lon: 129.8775, policePortal: 'https://www.police.pref.nagasaki.jp/' },
  { code: '43', en: 'Kumamoto',   ja: '熊本県',   lat: 32.8019, lon: 130.7256, policePortal: 'https://www.police.pref.kumamoto.jp/' },
  { code: '44', en: 'Oita',       ja: '大分県',   lat: 33.2381, lon: 131.6126, policePortal: 'https://www.police.pref.oita.jp/' },
  { code: '45', en: 'Miyazaki',   ja: '宮崎県',   lat: 31.9111, lon: 131.4239, policePortal: 'https://www.pref.miyazaki.lg.jp/police/' },
  { code: '46', en: 'Kagoshima',  ja: '鹿児島県', lat: 31.5963, lon: 130.5571, policePortal: 'https://www.pref.kagoshima.jp/police/' },
  { code: '47', en: 'Okinawa',    ja: '沖縄県',   lat: 26.2125, lon: 127.6809, policePortal: 'https://www.police.pref.okinawa.jp/' },
];

const BY_JA = new Map(JP_PREFECTURES.map((p) => [p.ja, p]));
const BY_EN = new Map(JP_PREFECTURES.map((p) => [p.en.toLowerCase(), p]));
const BY_CODE = new Map(JP_PREFECTURES.map((p) => [p.code, p]));

/**
 * Resolve a prefecture by Japanese name (e.g. "東京都"), English name
 * ("Tokyo"), or 2-digit JIS code ("13"). Accepts loose forms — strips
 * common suffixes (都/道/府/県) and is case-insensitive on English. Returns
 * null on no match.
 */
export function resolvePrefecture(input) {
  if (input == null) return null;
  const s = String(input).trim();
  if (!s) return null;
  if (BY_CODE.has(s)) return BY_CODE.get(s);
  if (BY_JA.has(s)) return BY_JA.get(s);
  const lower = s.toLowerCase();
  if (BY_EN.has(lower)) return BY_EN.get(lower);
  const stripped = s.replace(/[都道府県]$/, '');
  for (const p of JP_PREFECTURES) {
    if (p.ja.replace(/[都道府県]$/, '') === stripped) return p;
  }
  return null;
}
