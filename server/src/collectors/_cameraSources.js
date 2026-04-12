/**
 * Shared camera discovery sources (curated + query catalogs).
 * Used by the unified cameraDiscovery collector to fuse every known
 * public-camera channel in Japan into a single feature stream.
 *
 * Deliberately kept as plain data so the collector can fan out and
 * de-duplicate without each sub-module re-declaring the same lists.
 */

// ─── 1. JMA volcano monitoring cameras (official, always public) ────────────
// https://www.data.jma.go.jp/svd/volcam/data/gazo/
export const JMA_VOLCANO_CAMS = [
  { name: '桜島 (有村)', lat: 31.5553, lon: 130.6800, vid: 'sakurajima_arimura' },
  { name: '桜島 (海潟)', lat: 31.5700, lon: 130.6800, vid: 'sakurajima_kaigata' },
  { name: '阿蘇山 (草千里)', lat: 32.8841, lon: 131.1041, vid: 'aso_kusasenri' },
  { name: '阿蘇山 (本堂)', lat: 32.8866, lon: 131.1047, vid: 'aso_hondou' },
  { name: '浅間山 (追分)', lat: 36.4061, lon: 138.5231, vid: 'asama_oiwake' },
  { name: '箱根山 (大涌谷)', lat: 35.2314, lon: 139.0211, vid: 'hakone_owakudani' },
  { name: '雲仙岳 (仁田峠)', lat: 32.7611, lon: 130.2990, vid: 'unzen_nitatouge' },
  { name: '有珠山', lat: 42.5445, lon: 140.8378, vid: 'usu' },
  { name: '十勝岳', lat: 43.4157, lon: 142.6862, vid: 'tokachi' },
  { name: '霧島山 (新燃岳)', lat: 31.9091, lon: 130.8878, vid: 'kirishima_shinmoe' },
  { name: '草津白根山', lat: 36.6181, lon: 138.5281, vid: 'kusatsu_shirane' },
  { name: '諏訪之瀬島', lat: 29.6381, lon: 129.7141, vid: 'suwanose' },
  { name: '富士山 (富士吉田)', lat: 35.3606, lon: 138.7274, vid: 'fuji_yoshida' },
  { name: '御嶽山 (田の原)', lat: 35.8934, lon: 137.4806, vid: 'ontake_tanohara' },
  { name: '吾妻山', lat: 37.7350, lon: 140.2458, vid: 'azuma' },
  { name: '那須岳', lat: 37.1219, lon: 139.9633, vid: 'nasu' },
  { name: '新潟焼山', lat: 36.9189, lon: 138.0356, vid: 'niigata_yakeyama' },
  { name: '白山', lat: 36.1544, lon: 136.7708, vid: 'hakusan' },
  { name: '蔵王山', lat: 38.1436, lon: 140.4481, vid: 'zao' },
  { name: '口永良部島', lat: 30.4436, lon: 130.2178, vid: 'kuchinoerabu' },
  { name: '三宅島 (雄山)', lat: 34.0789, lon: 139.5292, vid: 'miyakejima' },
  { name: '硫黄島 (薩摩)', lat: 30.7889, lon: 130.3069, vid: 'satsuma_iwojima' },
];

// ─── 2. MLIT river monitoring cameras (prefectural river offices) ────────────
export const MLIT_RIVER_CAMS = [
  { name: '荒川 岩淵水門', lat: 35.7800, lon: 139.7200, office: '関東地方整備局' },
  { name: '荒川 秋ヶ瀬', lat: 35.8333, lon: 139.6000, office: '関東地方整備局' },
  { name: '多摩川 田園調布', lat: 35.5700, lon: 139.6800, office: '関東地方整備局' },
  { name: '多摩川 日野橋', lat: 35.6753, lon: 139.4122, office: '関東地方整備局' },
  { name: '利根川 取手', lat: 36.0200, lon: 139.9500, office: '関東地方整備局' },
  { name: '利根川 栗橋', lat: 36.1319, lon: 139.7031, office: '関東地方整備局' },
  { name: '淀川 枚方', lat: 34.8058, lon: 135.6506, office: '近畿地方整備局' },
  { name: '淀川 毛馬', lat: 34.7200, lon: 135.4500, office: '近畿地方整備局' },
  { name: '信濃川 長岡', lat: 37.4458, lon: 138.8517, office: '北陸地方整備局' },
  { name: '信濃川 下流', lat: 37.9100, lon: 139.0300, office: '北陸地方整備局' },
  { name: '石狩川 江別', lat: 43.1800, lon: 141.3200, office: '北海道開発局' },
  { name: '石狩川 旭川', lat: 43.7711, lon: 142.3650, office: '北海道開発局' },
  { name: '筑後川 久留米', lat: 33.3200, lon: 130.5100, office: '九州地方整備局' },
  { name: '吉野川 徳島', lat: 34.0714, lon: 134.5519, office: '四国地方整備局' },
  { name: '木曽川 犬山', lat: 35.3878, lon: 136.9464, office: '中部地方整備局' },
  { name: '天竜川 浜松', lat: 34.7181, lon: 137.8375, office: '中部地方整備局' },
  { name: '最上川 酒田', lat: 38.9142, lon: 139.8419, office: '東北地方整備局' },
  { name: '北上川 一関', lat: 38.9353, lon: 141.1264, office: '東北地方整備局' },
  { name: '太田川 広島', lat: 34.3920, lon: 132.4580, office: '中国地方整備局' },
];

// ─── 3. Shutoko / Hanshin / NEXCO expressway CCTV (jartic + operator sites) ──
export const EXPRESSWAY_CAMS = [
  { name: '首都高 箱崎JCT', lat: 35.6838, lon: 139.7892, operator: 'Shutoko', url: 'https://www.shutoko.jp/use/realtime/camera/' },
  { name: '首都高 大橋JCT', lat: 35.6510, lon: 139.6862, operator: 'Shutoko' },
  { name: '首都高 辰巳JCT', lat: 35.6457, lon: 139.8125, operator: 'Shutoko' },
  { name: '首都高 浜崎橋JCT', lat: 35.6570, lon: 139.7585, operator: 'Shutoko' },
  { name: '首都高 板橋JCT', lat: 35.7550, lon: 139.7050, operator: 'Shutoko' },
  { name: '阪神高速 環状線', lat: 34.6870, lon: 135.5190, operator: 'Hanshin' },
  { name: '阪神高速 神戸線', lat: 34.7000, lon: 135.2800, operator: 'Hanshin' },
  { name: '名神高速 吹田IC', lat: 34.7711, lon: 135.5174, operator: 'NEXCO西' },
  { name: '東名高速 海老名SA', lat: 35.4428, lon: 139.3898, operator: 'NEXCO中' },
  { name: '東北道 佐野SA', lat: 36.3167, lon: 139.5500, operator: 'NEXCO東' },
  { name: '関越道 三芳PA', lat: 35.8208, lon: 139.5208, operator: 'NEXCO東' },
  { name: '中央道 談合坂SA', lat: 35.6447, lon: 138.9389, operator: 'NEXCO中' },
  { name: '常磐道 守谷SA', lat: 35.9642, lon: 140.0342, operator: 'NEXCO東' },
  { name: '九州道 古賀SA', lat: 33.7203, lon: 130.4697, operator: 'NEXCO西' },
  { name: '北海道道央道 岩見沢IC', lat: 43.1961, lon: 141.7775, operator: 'NEXCO東' },
];

// ─── 4. NHK, YouTube, municipal live stream cameras ──────────────────────────
export const BROADCAST_LIVECAMS = [
  { name: '渋谷スクランブル交差点', type: 'youtube_live', lat: 35.6595, lon: 139.7004, url: 'https://www.youtube.com/results?search_query=shibuya+crossing+live' },
  { name: '新宿歌舞伎町ライブ', type: 'youtube_live', lat: 35.6938, lon: 139.7036, url: 'https://www.youtube.com/results?search_query=shinjuku+kabukicho+live' },
  { name: '秋葉原駅前ライブ', type: 'youtube_live', lat: 35.6984, lon: 139.7731, url: 'https://www.youtube.com/results?search_query=akihabara+live' },
  { name: '道頓堀ライブ', type: 'youtube_live', lat: 34.6687, lon: 135.5013, url: 'https://www.youtube.com/results?search_query=dotonbori+live' },
  { name: '東京タワー眺望', type: 'youtube_live', lat: 35.6586, lon: 139.7454 },
  { name: '東京スカイツリー眺望', type: 'youtube_live', lat: 35.7101, lon: 139.8107 },
  { name: 'NHK 富士山ライブ', type: 'nhk_livecam', lat: 35.3606, lon: 138.7274 },
  { name: 'NHK 桜島ライブ', type: 'nhk_livecam', lat: 31.5853, lon: 130.6569 },
  { name: 'NHK 阿蘇ライブ', type: 'nhk_livecam', lat: 32.8841, lon: 131.1041 },
  { name: 'NHK 国会議事堂ライブ', type: 'nhk_livecam', lat: 35.6758, lon: 139.7449 },
  { name: '気象庁東京', type: 'weather_livecam', lat: 35.6905, lon: 139.7525 },
  { name: '小樽運河ライブ', type: 'municipal_livecam', lat: 43.1974, lon: 140.9942 },
  { name: '白川郷ライブ', type: 'municipal_livecam', lat: 36.2578, lon: 136.9063 },
  { name: '宮島 厳島神社', type: 'municipal_livecam', lat: 34.2960, lon: 132.3196 },
  { name: '沖縄美ら海水族館', type: 'aquarium_livecam', lat: 26.6943, lon: 127.8779 },
];

// ─── 5. Ski / beach / port webcams (tourism operators) ───────────────────────
export const TOURISM_CAMS = [
  { name: 'ニセコ ヒラフ', type: 'ski', lat: 42.8620, lon: 140.6887 },
  { name: '白馬八方尾根', type: 'ski', lat: 36.6969, lon: 137.8329 },
  { name: '蔵王温泉', type: 'ski', lat: 38.1690, lon: 140.3880 },
  { name: '苗場', type: 'ski', lat: 36.8450, lon: 138.7910 },
  { name: '志賀高原', type: 'ski', lat: 36.7730, lon: 138.5260 },
  { name: 'ルスツ', type: 'ski', lat: 42.7480, lon: 140.5560 },
  { name: '湯沢高原', type: 'ski', lat: 36.9361, lon: 138.8186 },
  { name: 'ハチ北', type: 'ski', lat: 35.4572, lon: 134.5458 },
  { name: '湘南茅ヶ崎', type: 'beach', lat: 35.3103, lon: 139.4910 },
  { name: '宮古島前浜', type: 'beach', lat: 24.7914, lon: 125.2777 },
  { name: '沖縄万座ビーチ', type: 'beach', lat: 26.5097, lon: 127.8510 },
  { name: '白浜ビーチ', type: 'beach', lat: 33.6770, lon: 135.3530 },
  { name: '九十九里浜', type: 'beach', lat: 35.5236, lon: 140.4600 },
  { name: '七里ヶ浜', type: 'beach', lat: 35.3056, lon: 139.5142 },
  { name: '東京港 青海', type: 'port', lat: 35.6200, lon: 139.7800 },
  { name: '横浜港 大さん橋', type: 'port', lat: 35.4500, lon: 139.6450 },
  { name: '神戸港 メリケン', type: 'port', lat: 34.6777, lon: 135.1862 },
  { name: '名古屋港', type: 'port', lat: 35.0800, lon: 136.8800 },
  { name: '博多港', type: 'port', lat: 33.6100, lon: 130.4000 },
  { name: '那覇港', type: 'port', lat: 26.2100, lon: 127.6700 },
];

// ─── 6. Overpass queries (surveillance + livecam + tourism webcam) ───────────
export const OVERPASS_CAMERA_QUERIES = [
  // Primary surveillance tags
  'node["man_made"="surveillance"](area.jp);',
  // Secondary surveillance shapes
  'node["surveillance"](area.jp);',
  'node["surveillance:type"="camera"](area.jp);',
  // Tourism viewpoints that have a webcam URL
  'node["tourism"="viewpoint"]["webcam"](area.jp);',
  // General objects tagged as webcam
  'node["webcam"](area.jp);',
  'node["contact:webcam"](area.jp);',
  // Traffic cam explicit
  'node["traffic_signals"="camera"](area.jp);',
];

// ─── 7. Search dorks for public Japanese camera feeds ────────────────────────
// Used across DuckDuckGo HTML and (when key present) other engines.
// Queries avoid anything authentication-gated — only "view me" public feeds.
export const CAMERA_DORKS = [
  // Common vendor default paths
  'site:*.jp inurl:"ViewerFrame?Mode=Motion"',            // Panasonic
  'site:*.jp inurl:"view/viewer_index.shtml"',            // AXIS
  'site:*.jp inurl:"/view/index.shtml"',                   // AXIS
  'site:*.jp intitle:"Live View / - AXIS"',
  'site:*.jp intitle:"Network Camera NetworkCamera"',
  'site:*.jp intitle:"Live NetCam" intext:"snapshot"',
  'site:*.jp inurl:"/control/userimage.html"',
  'site:*.jp intitle:"WebcamXP 5"',
  'site:*.jp intitle:"Live View" "WebViewer"',
  'site:*.jp inurl:"/axis-cgi/jpg/image.cgi"',
  'site:*.jp inurl:"snap.jpg" intitle:"Live"',
  // Japanese-specific camera directories
  'site:*.jp inurl:"livecamera"',
  'site:*.jp inurl:"livecam"',
  'site:*.jp "ライブカメラ" inurl:"camera"',
  'site:*.jp "道路カメラ" inurl:"cctv"',
  'site:*.jp "河川カメラ"',
  // External aggregators with JP content
  'site:windy.com/webcams Japan',
  'site:webcamtaxi.com Japan',
  'site:earthcam.com Japan',
  'site:skylinewebcams.com Japan',
  'site:livecam.asia Japan',
];

// ─── 8. External aggregator directories to scrape for JP cameras ─────────────
export const AGGREGATOR_URLS = [
  'https://www.insecam.org/en/bycountry/JP/',
  'https://www.windy.com/-Webcams/webcams?/45.3,138.3,6',  // Webcams.travel feed
  'https://www.skylinewebcams.com/en/webcam/japan.html',
  'https://livecam.asia/japan/',
  'https://www.earthcam.com/japan/',
];

// ─── 9. Shodan search queries specifically for cameras in JP ─────────────────
// Used when SHODAN_API_KEY is present.
export const SHODAN_CAMERA_QUERIES = [
  'country:JP product:"Hikvision IP Camera"',
  'country:JP product:"Dahua IP Camera"',
  'country:JP "Server: Axis" "200 OK"',
  'country:JP "Server: Hipcam"',
  'country:JP title:"Network Camera" country:JP',
  'country:JP webcamXP',
  'country:JP product:"Panasonic Network Camera"',
  'country:JP port:554 has_screenshot:true',
];

export default {
  JMA_VOLCANO_CAMS,
  MLIT_RIVER_CAMS,
  EXPRESSWAY_CAMS,
  BROADCAST_LIVECAMS,
  TOURISM_CAMS,
  OVERPASS_CAMERA_QUERIES,
  CAMERA_DORKS,
  AGGREGATOR_URLS,
  SHODAN_CAMERA_QUERIES,
};
