/**
 * ODPT Transport Collector
 * Open Data for Public Transportation - Tokyo metro/JR stations
 * Fallback with comprehensive station data
 */

const API_BASE = 'https://api.odpt.org/api/v4/';
const TIMEOUT_MS = 5000;

const STATIONS = [
  // JR Yamanote Line
  { name: '東京', line: 'JR山手線', lat: 35.6812, lon: 139.7671, passengers: 462000 },
  { name: '有楽町', line: 'JR山手線', lat: 35.6748, lon: 139.7630, passengers: 170000 },
  { name: '新橋', line: 'JR山手線', lat: 35.6660, lon: 139.7583, passengers: 275000 },
  { name: '浜松町', line: 'JR山手線', lat: 35.6554, lon: 139.7571, passengers: 157000 },
  { name: '田町', line: 'JR山手線', lat: 35.6459, lon: 139.7475, passengers: 152000 },
  { name: '品川', line: 'JR山手線', lat: 35.6285, lon: 139.7388, passengers: 378000 },
  { name: '大崎', line: 'JR山手線', lat: 35.6197, lon: 139.7284, passengers: 130000 },
  { name: '五反田', line: 'JR山手線', lat: 35.6262, lon: 139.7235, passengers: 130000 },
  { name: '目黒', line: 'JR山手線', lat: 35.6338, lon: 139.7158, passengers: 112000 },
  { name: '恵比寿', line: 'JR山手線', lat: 35.6467, lon: 139.7101, passengers: 136000 },
  { name: '渋谷', line: 'JR山手線', lat: 35.6580, lon: 139.7016, passengers: 366000 },
  { name: '原宿', line: 'JR山手線', lat: 35.6702, lon: 139.7027, passengers: 75000 },
  { name: '代々木', line: 'JR山手線', lat: 35.6834, lon: 139.7020, passengers: 67000 },
  { name: '新宿', line: 'JR山手線', lat: 35.6896, lon: 139.7006, passengers: 775000 },
  { name: '新大久保', line: 'JR山手線', lat: 35.7012, lon: 139.7001, passengers: 53000 },
  { name: '高田馬場', line: 'JR山手線', lat: 35.7127, lon: 139.7038, passengers: 208000 },
  { name: '目白', line: 'JR山手線', lat: 35.7211, lon: 139.7068, passengers: 37000 },
  { name: '池袋', line: 'JR山手線', lat: 35.7295, lon: 139.7109, passengers: 558000 },
  { name: '大塚', line: 'JR山手線', lat: 35.7319, lon: 139.7286, passengers: 52000 },
  { name: '巣鴨', line: 'JR山手線', lat: 35.7334, lon: 139.7393, passengers: 56000 },
  { name: '駒込', line: 'JR山手線', lat: 35.7368, lon: 139.7470, passengers: 47000 },
  { name: '田端', line: 'JR山手線', lat: 35.7381, lon: 139.7609, passengers: 46000 },
  { name: '西日暮里', line: 'JR山手線', lat: 35.7321, lon: 139.7668, passengers: 51000 },
  { name: '日暮里', line: 'JR山手線', lat: 35.7280, lon: 139.7710, passengers: 106000 },
  { name: '鶯谷', line: 'JR山手線', lat: 35.7210, lon: 139.7780, passengers: 25000 },
  { name: '上野', line: 'JR山手線', lat: 35.7141, lon: 139.7774, passengers: 187000 },
  { name: '御徒町', line: 'JR山手線', lat: 35.7075, lon: 139.7748, passengers: 68000 },
  { name: '秋葉原', line: 'JR山手線', lat: 35.6984, lon: 139.7731, passengers: 246000 },
  { name: '神田', line: 'JR山手線', lat: 35.6917, lon: 139.7709, passengers: 108000 },
  // Tokyo Metro key stations
  { name: '銀座', line: '東京メトロ銀座線', lat: 35.6717, lon: 139.7637, passengers: 235000 },
  { name: '表参道', line: '東京メトロ銀座線', lat: 35.6654, lon: 139.7122, passengers: 182000 },
  { name: '赤坂見附', line: '東京メトロ銀座線', lat: 35.6770, lon: 139.7371, passengers: 95000 },
  { name: '溜池山王', line: '東京メトロ銀座線', lat: 35.6739, lon: 139.7415, passengers: 120000 },
  { name: '大手町', line: '東京メトロ丸ノ内線', lat: 35.6860, lon: 139.7636, passengers: 330000 },
  { name: '霞ケ関', line: '東京メトロ丸ノ内線', lat: 35.6733, lon: 139.7502, passengers: 145000 },
  { name: '六本木', line: '東京メトロ日比谷線', lat: 35.6626, lon: 139.7315, passengers: 112000 },
  { name: '中目黒', line: '東京メトロ日比谷線', lat: 35.6443, lon: 139.6989, passengers: 105000 },
  { name: '北千住', line: '東京メトロ日比谷線', lat: 35.7497, lon: 139.8049, passengers: 210000 },
  { name: '飯田橋', line: '東京メトロ東西線', lat: 35.7020, lon: 139.7452, passengers: 152000 },
  { name: '九段下', line: '東京メトロ東西線', lat: 35.6952, lon: 139.7511, passengers: 120000 },
  { name: '日本橋', line: '東京メトロ東西線', lat: 35.6818, lon: 139.7748, passengers: 135000 },
  { name: '門前仲町', line: '東京メトロ東西線', lat: 35.6726, lon: 139.7963, passengers: 98000 },
  { name: '豊洲', line: '東京メトロ有楽町線', lat: 35.6535, lon: 139.7965, passengers: 125000 },
  { name: '月島', line: '東京メトロ有楽町線', lat: 35.6636, lon: 139.7866, passengers: 55000 },
  { name: '永田町', line: '東京メトロ有楽町線', lat: 35.6784, lon: 139.7389, passengers: 75000 },
  { name: '護国寺', line: '東京メトロ有楽町線', lat: 35.7169, lon: 139.7270, passengers: 42000 },
  // Toei Subway
  { name: '新宿三丁目', line: '都営新宿線', lat: 35.6910, lon: 139.7045, passengers: 175000 },
  { name: '馬喰横山', line: '都営新宿線', lat: 35.6929, lon: 139.7834, passengers: 55000 },
  { name: '神保町', line: '都営三田線', lat: 35.6958, lon: 139.7577, passengers: 88000 },
  { name: '三田', line: '都営三田線', lat: 35.6487, lon: 139.7467, passengers: 62000 },
  { name: '大門', line: '都営大江戸線', lat: 35.6557, lon: 139.7555, passengers: 85000 },
  { name: '青山一丁目', line: '都営大江戸線', lat: 35.6726, lon: 139.7244, passengers: 75000 },
  { name: '汐留', line: '都営大江戸線', lat: 35.6609, lon: 139.7621, passengers: 65000 },
  { name: '築地市場', line: '都営大江戸線', lat: 35.6622, lon: 139.7698, passengers: 45000 },
  // Major JR stations outside Yamanote
  { name: '吉祥寺', line: 'JR中央線', lat: 35.7030, lon: 139.5803, passengers: 142000 },
  { name: '立川', line: 'JR中央線', lat: 35.6980, lon: 139.4137, passengers: 160000 },
  { name: '八王子', line: 'JR中央線', lat: 35.6558, lon: 139.3388, passengers: 85000 },
  { name: '町田', line: 'JR横浜線', lat: 35.5423, lon: 139.4466, passengers: 110000 },
  { name: '武蔵小杉', line: 'JR横須賀線', lat: 35.5763, lon: 139.6597, passengers: 120000 },
  { name: '川崎', line: 'JR東海道線', lat: 35.5308, lon: 139.6992, passengers: 210000 },
  { name: '横浜', line: 'JR東海道線', lat: 35.4658, lon: 139.6225, passengers: 420000 },
  // Other major stations
  { name: '大宮', line: 'JR京浜東北線', lat: 35.9062, lon: 139.6237, passengers: 260000 },
  { name: '柏', line: 'JR常磐線', lat: 35.8618, lon: 139.9751, passengers: 125000 },
  { name: '船橋', line: 'JR総武線', lat: 35.7017, lon: 139.9852, passengers: 140000 },
  { name: '千葉', line: 'JR総武線', lat: 35.6131, lon: 140.1134, passengers: 105000 },
  // Private railways
  { name: '二子玉川', line: '東急田園都市線', lat: 35.6116, lon: 139.6264, passengers: 92000 },
  { name: '自由が丘', line: '東急東横線', lat: 35.6077, lon: 139.6688, passengers: 95000 },
  { name: '下北沢', line: '小田急小田原線', lat: 35.6612, lon: 139.6677, passengers: 125000 },
  { name: '登戸', line: '小田急小田原線', lat: 35.6160, lon: 139.5666, passengers: 85000 },
  { name: '所沢', line: '西武池袋線', lat: 35.7878, lon: 139.4691, passengers: 100000 },
  { name: '練馬', line: '西武池袋線', lat: 35.7375, lon: 139.6541, passengers: 65000 },
  { name: '押上', line: '東京メトロ半蔵門線', lat: 35.7108, lon: 139.8133, passengers: 85000 },
  // Kansai area
  { name: '大阪/梅田', line: 'JR大阪環状線', lat: 34.7024, lon: 135.4959, passengers: 430000 },
  { name: '天王寺', line: 'JR大阪環状線', lat: 34.6466, lon: 135.5170, passengers: 155000 },
  { name: '難波', line: '南海本線', lat: 34.6625, lon: 135.5008, passengers: 250000 },
  { name: '京都', line: 'JR東海道線', lat: 34.9858, lon: 135.7588, passengers: 200000 },
  { name: '三ノ宮', line: 'JR東海道線', lat: 34.6937, lon: 135.1953, passengers: 125000 },
  { name: '新大阪', line: 'JR東海道新幹線', lat: 34.7334, lon: 135.5001, passengers: 230000 },
  // Other major cities
  { name: '名古屋', line: 'JR東海道新幹線', lat: 35.1709, lon: 136.8815, passengers: 405000 },
  { name: '博多', line: 'JR山陽新幹線', lat: 33.5897, lon: 130.4207, passengers: 140000 },
  { name: '仙台', line: 'JR東北新幹線', lat: 38.2601, lon: 140.8822, passengers: 90000 },
  { name: '札幌', line: 'JR函館本線', lat: 43.0687, lon: 141.3508, passengers: 95000 },
  { name: '広島', line: 'JR山陽新幹線', lat: 34.3981, lon: 132.4753, passengers: 75000 },
  { name: '岡山', line: 'JR山陽新幹線', lat: 34.6655, lon: 133.9184, passengers: 65000 },
  { name: '新横浜', line: 'JR東海道新幹線', lat: 35.5067, lon: 139.6179, passengers: 125000 },
  // Airports
  { name: '成田空港', line: 'JR成田エクスプレス', lat: 35.7720, lon: 140.3929, passengers: 45000 },
  { name: '羽田空港第1ターミナル', line: '東京モノレール', lat: 35.5494, lon: 139.7836, passengers: 95000 },
  { name: '関西空港', line: 'JR関空快速', lat: 34.4320, lon: 135.2304, passengers: 35000 },
  // More Tokyo Metro
  { name: '後楽園', line: '東京メトロ丸ノ内線', lat: 35.7081, lon: 139.7523, passengers: 92000 },
  { name: '茗荷谷', line: '東京メトロ丸ノ内線', lat: 35.7177, lon: 139.7342, passengers: 45000 },
  { name: '四ツ谷', line: '東京メトロ丸ノ内線', lat: 35.6862, lon: 139.7309, passengers: 78000 },
  { name: '市ケ谷', line: '東京メトロ有楽町線', lat: 35.6928, lon: 139.7355, passengers: 72000 },
  { name: '麻布十番', line: '東京メトロ南北線', lat: 35.6547, lon: 139.7374, passengers: 55000 },
  { name: '白金高輪', line: '東京メトロ南北線', lat: 35.6433, lon: 139.7337, passengers: 48000 },
  { name: '清澄白河', line: '東京メトロ半蔵門線', lat: 35.6811, lon: 139.8014, passengers: 42000 },
  { name: '錦糸町', line: 'JR総武線', lat: 35.6960, lon: 139.8150, passengers: 105000 },
  { name: '亀戸', line: 'JR総武線', lat: 35.6974, lon: 139.8264, passengers: 45000 },
  { name: '西船橋', line: 'JR総武線', lat: 35.7184, lon: 139.9554, passengers: 135000 },

  // ── Osaka Metro ───────────────────────────────────────────
  { name: '心斎橋', line: '大阪メトロ御堂筋線', lat: 34.6748, lon: 135.5014, passengers: 180000 },
  { name: '本町', line: '大阪メトロ御堂筋線', lat: 34.6830, lon: 135.5003, passengers: 155000 },
  { name: '淀屋橋', line: '大阪メトロ御堂筋線', lat: 34.6924, lon: 135.5024, passengers: 135000 },
  { name: '中津', line: '大阪メトロ御堂筋線', lat: 34.7071, lon: 135.4954, passengers: 40000 },
  { name: '新大阪', line: '大阪メトロ御堂筋線', lat: 34.7334, lon: 135.5001, passengers: 120000 },
  { name: '江坂', line: '大阪メトロ御堂筋線', lat: 34.7507, lon: 135.4992, passengers: 72000 },
  { name: '千里中央', line: '北大阪急行', lat: 34.8014, lon: 135.4710, passengers: 65000 },
  { name: '動物園前', line: '大阪メトロ御堂筋線', lat: 34.6525, lon: 135.5065, passengers: 42000 },
  { name: '日本橋', line: '大阪メトロ堺筋線', lat: 34.6598, lon: 135.5082, passengers: 85000 },
  { name: '堺筋本町', line: '大阪メトロ堺筋線', lat: 34.6814, lon: 135.5073, passengers: 68000 },
  { name: '天下茶屋', line: '大阪メトロ堺筋線', lat: 34.6365, lon: 135.4960, passengers: 48000 },
  { name: '谷町四丁目', line: '大阪メトロ谷町線', lat: 34.6816, lon: 135.5164, passengers: 52000 },
  { name: '天満橋', line: '大阪メトロ谷町線', lat: 34.6899, lon: 135.5157, passengers: 60000 },
  { name: '東梅田', line: '大阪メトロ谷町線', lat: 34.7033, lon: 135.5019, passengers: 110000 },
  { name: '大日', line: '大阪メトロ谷町線', lat: 34.7417, lon: 135.5735, passengers: 35000 },
  { name: '森ノ宮', line: '大阪メトロ中央線', lat: 34.6795, lon: 135.5318, passengers: 42000 },
  { name: 'コスモスクエア', line: '大阪メトロ中央線', lat: 34.6366, lon: 135.4125, passengers: 25000 },
  { name: '弁天町', line: '大阪メトロ中央線', lat: 34.6621, lon: 135.4686, passengers: 35000 },
  { name: '住之江公園', line: '大阪メトロ四つ橋線', lat: 34.6127, lon: 135.4883, passengers: 28000 },

  // ── Kansai JR / Private ───────────────────────────────────
  { name: '天王寺', line: 'JR大阪環状線', lat: 34.6466, lon: 135.5170, passengers: 155000 },
  { name: '鶴橋', line: 'JR大阪環状線', lat: 34.6680, lon: 135.5301, passengers: 98000 },
  { name: '京橋', line: 'JR大阪環状線', lat: 34.6943, lon: 135.5362, passengers: 115000 },
  { name: '西九条', line: 'JR大阪環状線', lat: 34.6800, lon: 135.4663, passengers: 42000 },
  { name: '新今宮', line: 'JR大阪環状線', lat: 34.6498, lon: 135.5023, passengers: 65000 },
  { name: '三宮', line: 'JR東海道線', lat: 34.6937, lon: 135.1953, passengers: 125000 },
  { name: '元町', line: 'JR東海道線', lat: 34.6892, lon: 135.1874, passengers: 35000 },
  { name: '神戸', line: 'JR東海道線', lat: 34.6798, lon: 135.1787, passengers: 72000 },
  { name: '姫路', line: 'JR山陽本線', lat: 34.8268, lon: 134.6914, passengers: 52000 },
  { name: '奈良', line: 'JR大和路線', lat: 34.6798, lon: 135.8199, passengers: 42000 },
  { name: '河原町', line: '阪急京都線', lat: 35.0037, lon: 135.7688, passengers: 88000 },
  { name: '烏丸', line: '阪急京都線', lat: 35.0033, lon: 135.7582, passengers: 62000 },
  { name: '桂', line: '阪急京都線', lat: 34.9831, lon: 135.7129, passengers: 45000 },
  { name: '高槻市', line: '阪急京都線', lat: 34.8480, lon: 135.6174, passengers: 55000 },
  { name: '茨木市', line: '阪急京都線', lat: 34.8155, lon: 135.5688, passengers: 42000 },
  { name: '西宮北口', line: '阪急神戸線', lat: 34.7438, lon: 135.3606, passengers: 65000 },
  { name: '宝塚', line: '阪急宝塚線', lat: 34.8093, lon: 135.3410, passengers: 38000 },
  { name: '近鉄奈良', line: '近鉄奈良線', lat: 34.6817, lon: 135.8290, passengers: 52000 },
  { name: '近鉄日本橋', line: '近鉄難波線', lat: 34.6643, lon: 135.5090, passengers: 95000 },
  { name: '鶴橋', line: '近鉄大阪線', lat: 34.6680, lon: 135.5301, passengers: 75000 },
  { name: '南海なんば', line: '南海本線', lat: 34.6625, lon: 135.5008, passengers: 120000 },
  { name: '新今宮', line: '南海本線', lat: 34.6498, lon: 135.5023, passengers: 48000 },
  { name: '堺', line: '南海本線', lat: 34.5813, lon: 135.4856, passengers: 32000 },
  { name: '関西空港', line: '南海空港線', lat: 34.4320, lon: 135.2304, passengers: 35000 },

  // ── Nagoya Area ───────────────────────────────────────────
  { name: '名古屋', line: 'JR東海道新幹線', lat: 35.1709, lon: 136.8815, passengers: 405000 },
  { name: '金山', line: 'JR中央線', lat: 35.1500, lon: 136.9005, passengers: 110000 },
  { name: '栄', line: '名古屋市営東山線', lat: 35.1689, lon: 136.9089, passengers: 175000 },
  { name: '伏見', line: '名古屋市営東山線', lat: 35.1670, lon: 136.8969, passengers: 72000 },
  { name: '藤が丘', line: '名古屋市営東山線', lat: 35.1797, lon: 137.0137, passengers: 45000 },
  { name: '今池', line: '名古屋市営東山線', lat: 35.1658, lon: 136.9322, passengers: 42000 },
  { name: '本山', line: '名古屋市営東山線', lat: 35.1579, lon: 136.9580, passengers: 38000 },
  { name: '久屋大通', line: '名古屋市営名城線', lat: 35.1726, lon: 136.9094, passengers: 52000 },
  { name: '大曽根', line: '名古屋市営名城線', lat: 35.1936, lon: 136.9359, passengers: 35000 },
  { name: '上前津', line: '名古屋市営名城線', lat: 35.1580, lon: 136.9049, passengers: 45000 },
  { name: '名古屋港', line: '名古屋市営名港線', lat: 35.0943, lon: 136.8828, passengers: 18000 },
  { name: '名鉄名古屋', line: '名鉄名古屋本線', lat: 35.1706, lon: 136.8805, passengers: 280000 },
  { name: '近鉄名古屋', line: '近鉄名古屋線', lat: 35.1706, lon: 136.8827, passengers: 95000 },
  { name: '豊橋', line: 'JR東海道線', lat: 34.7639, lon: 137.3831, passengers: 35000 },
  { name: '岐阜', line: 'JR東海道線', lat: 35.4099, lon: 136.7581, passengers: 32000 },
  { name: '刈谷', line: 'JR東海道線', lat: 34.9895, lon: 137.0044, passengers: 28000 },

  // ── Fukuoka / Kyushu ──────────────────────────────────────
  { name: '博多', line: 'JR山陽新幹線', lat: 33.5897, lon: 130.4207, passengers: 140000 },
  { name: '天神', line: '福岡市地下鉄空港線', lat: 33.5892, lon: 130.3983, passengers: 125000 },
  { name: '中洲川端', line: '福岡市地下鉄空港線', lat: 33.5918, lon: 130.4068, passengers: 52000 },
  { name: '赤坂', line: '福岡市地下鉄空港線', lat: 33.5885, lon: 130.3888, passengers: 35000 },
  { name: '福岡空港', line: '福岡市地下鉄空港線', lat: 33.5859, lon: 130.4508, passengers: 42000 },
  { name: '天神南', line: '福岡市地下鉄七隈線', lat: 33.5868, lon: 130.3981, passengers: 35000 },
  { name: '薬院', line: '福岡市地下鉄七隈線', lat: 33.5798, lon: 130.3997, passengers: 28000 },
  { name: '西鉄福岡', line: '西鉄天神大牟田線', lat: 33.5892, lon: 130.3983, passengers: 82000 },
  { name: '西鉄久留米', line: '西鉄天神大牟田線', lat: 33.3148, lon: 130.5073, passengers: 25000 },
  { name: '小倉', line: 'JR山陽新幹線', lat: 33.8860, lon: 130.8825, passengers: 65000 },
  { name: '熊本', line: 'JR九州新幹線', lat: 32.7905, lon: 130.6861, passengers: 42000 },
  { name: '鹿児島中央', line: 'JR九州新幹線', lat: 31.5840, lon: 130.5413, passengers: 38000 },
  { name: '大分', line: 'JR日豊本線', lat: 33.2329, lon: 131.6066, passengers: 25000 },
  { name: '長崎', line: 'JR長崎本線', lat: 32.7519, lon: 129.8697, passengers: 22000 },
  { name: '佐賀', line: 'JR長崎本線', lat: 33.2639, lon: 130.3010, passengers: 15000 },
  { name: '宮崎', line: 'JR日豊本線', lat: 31.9133, lon: 131.4232, passengers: 18000 },

  // ── Sapporo / Hokkaido ────────────────────────────────────
  { name: '札幌', line: 'JR函館本線', lat: 43.0687, lon: 141.3508, passengers: 95000 },
  { name: '大通', line: '札幌市営南北線', lat: 43.0590, lon: 141.3560, passengers: 72000 },
  { name: 'すすきの', line: '札幌市営南北線', lat: 43.0549, lon: 141.3535, passengers: 48000 },
  { name: '麻生', line: '札幌市営南北線', lat: 43.0910, lon: 141.3409, passengers: 28000 },
  { name: '真駒内', line: '札幌市営南北線', lat: 43.0064, lon: 141.3471, passengers: 18000 },
  { name: '東豊線さっぽろ', line: '札幌市営東豊線', lat: 43.0660, lon: 141.3530, passengers: 35000 },
  { name: '新さっぽろ', line: 'JR千歳線', lat: 43.0236, lon: 141.4128, passengers: 35000 },
  { name: '新千歳空港', line: 'JR千歳線', lat: 42.7862, lon: 141.6797, passengers: 32000 },
  { name: '小樽', line: 'JR函館本線', lat: 43.1975, lon: 140.9946, passengers: 12000 },
  { name: '旭川', line: 'JR函館本線', lat: 43.7631, lon: 142.3583, passengers: 15000 },
  { name: '函館', line: 'JR函館本線', lat: 41.7739, lon: 140.7267, passengers: 8500 },
  { name: '帯広', line: 'JR根室本線', lat: 42.9204, lon: 143.2043, passengers: 6000 },
  { name: '釧路', line: 'JR根室本線', lat: 42.9750, lon: 144.3817, passengers: 4500 },

  // ── Sendai / Tohoku ───────────────────────────────────────
  { name: '仙台', line: 'JR東北新幹線', lat: 38.2601, lon: 140.8822, passengers: 90000 },
  { name: '仙台', line: '仙台市地下鉄南北線', lat: 38.2601, lon: 140.8822, passengers: 52000 },
  { name: '勾当台公園', line: '仙台市地下鉄南北線', lat: 38.2669, lon: 140.8687, passengers: 25000 },
  { name: '長町', line: '仙台市地下鉄南北線', lat: 38.2288, lon: 140.8822, passengers: 18000 },
  { name: '泉中央', line: '仙台市地下鉄南北線', lat: 38.3204, lon: 140.8811, passengers: 22000 },
  { name: '盛岡', line: 'JR東北新幹線', lat: 39.7014, lon: 141.1368, passengers: 28000 },
  { name: '秋田', line: 'JR秋田新幹線', lat: 39.7184, lon: 140.1025, passengers: 18000 },
  { name: '青森', line: 'JR奥羽本線', lat: 40.8286, lon: 140.7385, passengers: 12000 },
  { name: '山形', line: 'JR山形新幹線', lat: 38.2485, lon: 140.3281, passengers: 15000 },
  { name: '福島', line: 'JR東北新幹線', lat: 37.7543, lon: 140.4597, passengers: 22000 },
  { name: '郡山', line: 'JR東北新幹線', lat: 37.3973, lon: 140.3893, passengers: 25000 },
  { name: '新青森', line: 'JR東北新幹線', lat: 40.8229, lon: 140.6856, passengers: 8000 },

  // ── Hiroshima / Chugoku ───────────────────────────────────
  { name: '広島', line: 'JR山陽新幹線', lat: 34.3981, lon: 132.4753, passengers: 75000 },
  { name: '紙屋町', line: '広島電鉄', lat: 34.3935, lon: 132.4563, passengers: 35000 },
  { name: '八丁堀', line: '広島電鉄', lat: 34.3925, lon: 132.4632, passengers: 25000 },
  { name: '岡山', line: 'JR山陽新幹線', lat: 34.6655, lon: 133.9184, passengers: 65000 },
  { name: '倉敷', line: 'JR山陽本線', lat: 34.5985, lon: 133.7722, passengers: 22000 },
  { name: '松江', line: 'JR山陰本線', lat: 35.4632, lon: 133.0667, passengers: 8000 },
  { name: '鳥取', line: 'JR山陰本線', lat: 35.4946, lon: 134.2278, passengers: 7000 },
  { name: '山口', line: 'JR山口線', lat: 34.1720, lon: 131.4740, passengers: 5000 },
  { name: '下関', line: 'JR山陽本線', lat: 33.9508, lon: 130.9215, passengers: 12000 },

  // ── Shikoku ───────────────────────────────────────────────
  { name: '松山', line: 'JR予讃線', lat: 33.8393, lon: 132.7656, passengers: 15000 },
  { name: '高松', line: 'JR予讃線', lat: 34.3503, lon: 134.0467, passengers: 18000 },
  { name: '徳島', line: 'JR高徳線', lat: 34.0743, lon: 134.5517, passengers: 12000 },
  { name: '高知', line: 'JR土讃線', lat: 33.5673, lon: 133.5420, passengers: 8000 },

  // ── Hokuriku / Niigata ────────────────────────────────────
  { name: '新潟', line: 'JR上越新幹線', lat: 37.9113, lon: 139.0431, passengers: 42000 },
  { name: '長岡', line: 'JR上越新幹線', lat: 37.4493, lon: 138.8558, passengers: 15000 },
  { name: '金沢', line: 'JR北陸新幹線', lat: 36.5780, lon: 136.6484, passengers: 32000 },
  { name: '富山', line: 'JR北陸新幹線', lat: 36.7012, lon: 137.2131, passengers: 22000 },
  { name: '福井', line: 'JR北陸本線', lat: 36.0621, lon: 136.2232, passengers: 15000 },
  { name: '長野', line: 'JR北陸新幹線', lat: 36.6432, lon: 138.1889, passengers: 28000 },
  { name: '松本', line: 'JR篠ノ井線', lat: 36.2305, lon: 137.9686, passengers: 15000 },
  { name: '甲府', line: 'JR中央本線', lat: 35.6671, lon: 138.5684, passengers: 12000 },

  // ── Shizuoka / Chubu ──────────────────────────────────────
  { name: '静岡', line: 'JR東海道新幹線', lat: 34.9717, lon: 138.3879, passengers: 52000 },
  { name: '浜松', line: 'JR東海道新幹線', lat: 34.7037, lon: 137.7344, passengers: 35000 },
  { name: '沼津', line: 'JR東海道線', lat: 35.1020, lon: 138.8650, passengers: 18000 },
  { name: '三島', line: 'JR東海道新幹線', lat: 35.1269, lon: 138.9120, passengers: 15000 },

  // ── Okinawa ───────────────────────────────────────────────
  { name: '那覇空港', line: 'ゆいレール', lat: 26.2029, lon: 127.6501, passengers: 15000 },
  { name: '県庁前', line: 'ゆいレール', lat: 26.2153, lon: 127.6766, passengers: 12000 },
  { name: '牧志', line: 'ゆいレール', lat: 26.2174, lon: 127.6886, passengers: 8000 },
  { name: 'おもろまち', line: 'ゆいレール', lat: 26.2265, lon: 127.6959, passengers: 10000 },
  { name: '首里', line: 'ゆいレール', lat: 26.2214, lon: 127.7186, passengers: 7000 },
  { name: 'てだこ浦西', line: 'ゆいレール', lat: 26.2456, lon: 127.7525, passengers: 5000 },
];

function generateSeedData() {
  const now = new Date();
  return STATIONS.map((st, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [st.lon, st.lat] },
    properties: {
      station_id: `ODPT_${String(i + 1).padStart(3, '0')}`,
      station_name: st.name,
      line_name: st.line,
      daily_passengers: st.passengers,
      operator: st.line.startsWith('JR') ? 'JR' :
        st.line.includes('東京メトロ') ? '東京メトロ' :
        st.line.includes('都営') ? '都営地下鉄' :
        st.line.includes('東急') ? '東急電鉄' :
        st.line.includes('小田急') ? '小田急電鉄' :
        st.line.includes('西武') ? '西武鉄道' :
        st.line.includes('南海') ? '南海電鉄' :
        st.line.includes('モノレール') ? '東京モノレール' : 'その他',
      wheelchair_accessible: true,
      measured_at: now.toISOString(),
      source: 'odpt_seed',
    },
  }));
}

export default async function collectOdptTransport() {
  let features = [];
  let source = 'odpt_live';

  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(`${API_BASE}odpt:Station?odpt:railway=odpt.Railway:JR-East.Yamanote`, {
      signal: controller.signal,
      headers: { Accept: 'application/json' },
    });
    clearTimeout(timer);

    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.json();

    if (Array.isArray(data) && data.length > 0) {
      features = data
        .filter(d => d.geo_lat && d.geo_long)
        .map(d => ({
          type: 'Feature',
          geometry: { type: 'Point', coordinates: [+d.geo_long, +d.geo_lat] },
          properties: {
            station_id: d['owl:sameAs'] ?? d['@id'],
            station_name: d['dc:title'] ?? d['odpt:stationTitle']?.ja ?? null,
            line_name: d['odpt:railway'] ?? null,
            source: 'odpt_live',
          },
        }));
    }
    if (features.length === 0) throw new Error('No features parsed');
  } catch {
    features = generateSeedData();
    source = 'odpt_seed';
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Public transportation station data from ODPT',
    },
    metadata: {},
  };
}
