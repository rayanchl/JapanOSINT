/**
 * Full Japan Transport Collector - Nationwide Rail Coverage
 * Replaces Tokyo-only ODPT transport with complete nationwide coverage:
 * - All JR companies (Hokkaido, East, Central, West, Shikoku, Kyushu)
 * - All Shinkansen lines and stations
 * - Private railways (Tokyu, Odakyu, Keio, Hankyu, Hanshin, Kintetsu, Nankai, Meitetsu, Nishitetsu, etc.)
 * - All subway/metro systems (Tokyo Metro, Toei, Osaka Metro, Nagoya, Sapporo, Sendai, Fukuoka, Kyoto, Kobe)
 * - Monorails, trams, people movers
 *
 * Live: OSM Overpass `railway=station` + `station=subway/light_rail/monorail` across Japan.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpass(
    'node["railway"="station"](area.jp);node["station"="subway"](area.jp);node["station"="light_rail"](area.jp);node["station"="monorail"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        station_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || `Station ${i + 1}`,
        name_ja: el.tags?.name || null,
        operator: el.tags?.operator || null,
        line: el.tags?.line || null,
        type: el.tags?.station || el.tags?.['railway:traffic_mode'] || 'railway',
        network: el.tags?.network || null,
        wikidata: el.tags?.wikidata || null,
        source: 'osm_overpass',
      },
    }),
  );
}

const STATIONS = [
  // ═══ SHINKANSEN STATIONS ═══
  // Tokaido Shinkansen (東海道新幹線)
  { name: '東京', line: '東海道新幹線', operator: 'JR Central', lat: 35.6812, lon: 139.7671, type: 'shinkansen', passengers: 462000 },
  { name: '品川', line: '東海道新幹線', operator: 'JR Central', lat: 35.6284, lon: 139.7387, type: 'shinkansen', passengers: 390000 },
  { name: '新横浜', line: '東海道新幹線', operator: 'JR Central', lat: 35.5076, lon: 139.6173, type: 'shinkansen', passengers: 67000 },
  { name: '小田原', line: '東海道新幹線', operator: 'JR Central', lat: 35.2564, lon: 139.1547, type: 'shinkansen', passengers: 12000 },
  { name: '熱海', line: '東海道新幹線', operator: 'JR Central', lat: 35.1044, lon: 139.0778, type: 'shinkansen', passengers: 8000 },
  { name: '三島', line: '東海道新幹線', operator: 'JR Central', lat: 35.1278, lon: 138.9111, type: 'shinkansen', passengers: 11000 },
  { name: '新富士', line: '東海道新幹線', operator: 'JR Central', lat: 35.1419, lon: 138.6667, type: 'shinkansen', passengers: 5000 },
  { name: '静岡', line: '東海道新幹線', operator: 'JR Central', lat: 34.9717, lon: 138.3889, type: 'shinkansen', passengers: 28000 },
  { name: '掛川', line: '東海道新幹線', operator: 'JR Central', lat: 34.7694, lon: 137.9986, type: 'shinkansen', passengers: 5000 },
  { name: '浜松', line: '東海道新幹線', operator: 'JR Central', lat: 34.7039, lon: 137.7350, type: 'shinkansen', passengers: 20000 },
  { name: '豊橋', line: '東海道新幹線', operator: 'JR Central', lat: 34.7633, lon: 137.3831, type: 'shinkansen', passengers: 15000 },
  { name: '三河安城', line: '東海道新幹線', operator: 'JR Central', lat: 34.9561, lon: 137.0639, type: 'shinkansen', passengers: 6000 },
  { name: '名古屋', line: '東海道新幹線', operator: 'JR Central', lat: 35.1709, lon: 136.8815, type: 'shinkansen', passengers: 200000 },
  { name: '岐阜羽島', line: '東海道新幹線', operator: 'JR Central', lat: 35.3142, lon: 136.6856, type: 'shinkansen', passengers: 4000 },
  { name: '米原', line: '東海道新幹線', operator: 'JR Central', lat: 35.3144, lon: 136.2897, type: 'shinkansen', passengers: 3000 },
  { name: '京都', line: '東海道新幹線', operator: 'JR Central', lat: 34.9856, lon: 135.7581, type: 'shinkansen', passengers: 190000 },
  { name: '新大阪', line: '東海道新幹線', operator: 'JR Central', lat: 34.7336, lon: 135.5003, type: 'shinkansen', passengers: 230000 },
  // Sanyo Shinkansen (山陽新幹線)
  { name: '新神戸', line: '山陽新幹線', operator: 'JR West', lat: 34.6913, lon: 135.1960, type: 'shinkansen', passengers: 35000 },
  { name: '西明石', line: '山陽新幹線', operator: 'JR West', lat: 34.6494, lon: 134.9639, type: 'shinkansen', passengers: 5000 },
  { name: '姫路', line: '山陽新幹線', operator: 'JR West', lat: 34.8265, lon: 134.6914, type: 'shinkansen', passengers: 25000 },
  { name: '岡山', line: '山陽新幹線', operator: 'JR West', lat: 34.6654, lon: 133.9180, type: 'shinkansen', passengers: 52000 },
  { name: '福山', line: '山陽新幹線', operator: 'JR West', lat: 34.4893, lon: 133.3631, type: 'shinkansen', passengers: 15000 },
  { name: '広島', line: '山陽新幹線', operator: 'JR West', lat: 34.3978, lon: 132.4752, type: 'shinkansen', passengers: 72000 },
  { name: '新山口', line: '山陽新幹線', operator: 'JR West', lat: 34.1742, lon: 131.5519, type: 'shinkansen', passengers: 8000 },
  { name: '小倉', line: '山陽新幹線', operator: 'JR West', lat: 33.8876, lon: 130.8828, type: 'shinkansen', passengers: 35000 },
  { name: '博多', line: '山陽新幹線', operator: 'JR West', lat: 33.5897, lon: 130.4207, type: 'shinkansen', passengers: 130000 },
  // Tohoku Shinkansen (東北新幹線)
  { name: '上野', line: '東北新幹線', operator: 'JR East', lat: 35.7141, lon: 139.7774, type: 'shinkansen', passengers: 190000 },
  { name: '大宮', line: '東北新幹線', operator: 'JR East', lat: 35.9064, lon: 139.6237, type: 'shinkansen', passengers: 260000 },
  { name: '小山', line: '東北新幹線', operator: 'JR East', lat: 36.3142, lon: 139.8006, type: 'shinkansen', passengers: 8000 },
  { name: '宇都宮', line: '東北新幹線', operator: 'JR East', lat: 36.5594, lon: 139.8981, type: 'shinkansen', passengers: 30000 },
  { name: '那須塩原', line: '東北新幹線', operator: 'JR East', lat: 36.9575, lon: 140.0464, type: 'shinkansen', passengers: 4000 },
  { name: '新白河', line: '東北新幹線', operator: 'JR East', lat: 37.0847, lon: 140.1931, type: 'shinkansen', passengers: 3000 },
  { name: '郡山', line: '東北新幹線', operator: 'JR East', lat: 37.3978, lon: 140.3889, type: 'shinkansen', passengers: 18000 },
  { name: '福島', line: '東北新幹線', operator: 'JR East', lat: 37.7544, lon: 140.4597, type: 'shinkansen', passengers: 16000 },
  { name: '白石蔵王', line: '東北新幹線', operator: 'JR East', lat: 38.0042, lon: 140.6167, type: 'shinkansen', passengers: 2000 },
  { name: '仙台', line: '東北新幹線', operator: 'JR East', lat: 38.2601, lon: 140.8822, type: 'shinkansen', passengers: 90000 },
  { name: '古川', line: '東北新幹線', operator: 'JR East', lat: 38.5711, lon: 140.9583, type: 'shinkansen', passengers: 5000 },
  { name: '一ノ関', line: '東北新幹線', operator: 'JR East', lat: 38.9347, lon: 141.1264, type: 'shinkansen', passengers: 4000 },
  { name: '北上', line: '東北新幹線', operator: 'JR East', lat: 39.2861, lon: 141.1131, type: 'shinkansen', passengers: 4000 },
  { name: '盛岡', line: '東北新幹線', operator: 'JR East', lat: 39.7014, lon: 141.1369, type: 'shinkansen', passengers: 22000 },
  { name: '八戸', line: '東北新幹線', operator: 'JR East', lat: 40.5128, lon: 141.4889, type: 'shinkansen', passengers: 7000 },
  { name: '新青森', line: '東北新幹線', operator: 'JR East', lat: 40.8233, lon: 140.6858, type: 'shinkansen', passengers: 5000 },
  // Hokkaido Shinkansen
  { name: '新函館北斗', line: '北海道新幹線', operator: 'JR Hokkaido', lat: 41.9044, lon: 140.6486, type: 'shinkansen', passengers: 4000 },
  // Hokuriku Shinkansen (北陸新幹線)
  { name: '高崎', line: '北陸新幹線', operator: 'JR East', lat: 36.3219, lon: 139.0106, type: 'shinkansen', passengers: 30000 },
  { name: '軽井沢', line: '北陸新幹線', operator: 'JR East', lat: 36.3467, lon: 138.6350, type: 'shinkansen', passengers: 6000 },
  { name: '長野', line: '北陸新幹線', operator: 'JR East', lat: 36.6433, lon: 138.1886, type: 'shinkansen', passengers: 22000 },
  { name: '富山', line: '北陸新幹線', operator: 'JR West', lat: 36.7014, lon: 137.2131, type: 'shinkansen', passengers: 14000 },
  { name: '金沢', line: '北陸新幹線', operator: 'JR West', lat: 36.5780, lon: 136.6480, type: 'shinkansen', passengers: 22000 },
  { name: '敦賀', line: '北陸新幹線', operator: 'JR West', lat: 35.6453, lon: 136.0556, type: 'shinkansen', passengers: 5000 },
  // Kyushu Shinkansen (九州新幹線)
  { name: '久留米', line: '九州新幹線', operator: 'JR Kyushu', lat: 33.3231, lon: 130.5097, type: 'shinkansen', passengers: 5000 },
  { name: '熊本', line: '九州新幹線', operator: 'JR Kyushu', lat: 32.7897, lon: 130.6864, type: 'shinkansen', passengers: 19000 },
  { name: '鹿児島中央', line: '九州新幹線', operator: 'JR Kyushu', lat: 31.5842, lon: 130.5414, type: 'shinkansen', passengers: 20000 },

  // ═══ TOKYO METRO ═══
  { name: '渋谷 (銀座線)', line: '銀座線', operator: 'Tokyo Metro', lat: 35.6580, lon: 139.7016, type: 'subway', passengers: 230000 },
  { name: '表参道', line: '銀座線', operator: 'Tokyo Metro', lat: 35.6654, lon: 139.7121, type: 'subway', passengers: 160000 },
  { name: '赤坂見附', line: '銀座線', operator: 'Tokyo Metro', lat: 35.6770, lon: 139.7374, type: 'subway', passengers: 90000 },
  { name: '溜池山王', line: '銀座線', operator: 'Tokyo Metro', lat: 35.6740, lon: 139.7412, type: 'subway', passengers: 80000 },
  { name: '新橋 (銀座線)', line: '銀座線', operator: 'Tokyo Metro', lat: 35.6664, lon: 139.7583, type: 'subway', passengers: 120000 },
  { name: '日本橋', line: '銀座線', operator: 'Tokyo Metro', lat: 35.6825, lon: 139.7741, type: 'subway', passengers: 100000 },
  { name: '大手町', line: '丸ノ内線', operator: 'Tokyo Metro', lat: 35.6867, lon: 139.7660, type: 'subway', passengers: 180000 },
  { name: '池袋 (丸ノ内線)', line: '丸ノ内線', operator: 'Tokyo Metro', lat: 35.7295, lon: 139.7109, type: 'subway', passengers: 220000 },
  { name: '東京 (丸ノ内線)', line: '丸ノ内線', operator: 'Tokyo Metro', lat: 35.6812, lon: 139.7671, type: 'subway', passengers: 200000 },
  { name: '霞ケ関', line: '丸ノ内線', operator: 'Tokyo Metro', lat: 35.6730, lon: 139.7510, type: 'subway', passengers: 85000 },
  { name: '新宿三丁目', line: '丸ノ内線', operator: 'Tokyo Metro', lat: 35.6908, lon: 139.7044, type: 'subway', passengers: 140000 },
  { name: '六本木', line: '日比谷線', operator: 'Tokyo Metro', lat: 35.6631, lon: 139.7314, type: 'subway', passengers: 105000 },
  { name: '恵比寿', line: '日比谷線', operator: 'Tokyo Metro', lat: 35.6467, lon: 139.7101, type: 'subway', passengers: 130000 },
  { name: '北千住', line: '日比谷線', operator: 'Tokyo Metro', lat: 35.7497, lon: 139.8047, type: 'subway', passengers: 180000 },
  { name: '中目黒', line: '日比谷線', operator: 'Tokyo Metro', lat: 35.6440, lon: 139.6988, type: 'subway', passengers: 100000 },

  // ═══ TOEI SUBWAY ═══
  { name: '新宿 (大江戸線)', line: '大江戸線', operator: 'Toei', lat: 35.6896, lon: 139.7006, type: 'subway', passengers: 140000 },
  { name: '六本木 (大江戸線)', line: '大江戸線', operator: 'Toei', lat: 35.6631, lon: 139.7314, type: 'subway', passengers: 70000 },
  { name: '月島', line: '大江戸線', operator: 'Toei', lat: 35.6631, lon: 139.7836, type: 'subway', passengers: 55000 },
  { name: '押上', line: '浅草線', operator: 'Toei', lat: 35.7108, lon: 139.8131, type: 'subway', passengers: 70000 },
  { name: '三田', line: '三田線', operator: 'Toei', lat: 35.6489, lon: 139.7456, type: 'subway', passengers: 60000 },

  // ═══ OSAKA METRO ═══
  { name: '梅田', line: '御堂筋線', operator: 'Osaka Metro', lat: 34.7024, lon: 135.4959, type: 'subway', passengers: 440000 },
  { name: '淀屋橋', line: '御堂筋線', operator: 'Osaka Metro', lat: 34.6930, lon: 135.5028, type: 'subway', passengers: 200000 },
  { name: '本町', line: '御堂筋線', operator: 'Osaka Metro', lat: 34.6810, lon: 135.5010, type: 'subway', passengers: 220000 },
  { name: '心斎橋', line: '御堂筋線', operator: 'Osaka Metro', lat: 34.6748, lon: 135.5012, type: 'subway', passengers: 180000 },
  { name: '難波 (御堂筋線)', line: '御堂筋線', operator: 'Osaka Metro', lat: 34.6627, lon: 135.5010, type: 'subway', passengers: 260000 },
  { name: '天王寺', line: '御堂筋線', operator: 'Osaka Metro', lat: 34.6468, lon: 135.5135, type: 'subway', passengers: 190000 },
  { name: '日本橋', line: '堺筋線', operator: 'Osaka Metro', lat: 34.6685, lon: 135.5058, type: 'subway', passengers: 70000 },

  // ═══ NAGOYA SUBWAY ═══
  { name: '名古屋 (東山線)', line: '東山線', operator: 'Nagoya Subway', lat: 35.1709, lon: 136.8815, type: 'subway', passengers: 130000 },
  { name: '栄 (東山線)', line: '東山線', operator: 'Nagoya Subway', lat: 35.1692, lon: 136.9084, type: 'subway', passengers: 100000 },
  { name: '伏見', line: '東山線', operator: 'Nagoya Subway', lat: 35.1669, lon: 136.8922, type: 'subway', passengers: 80000 },
  { name: '金山', line: '名城線', operator: 'Nagoya Subway', lat: 35.1440, lon: 136.9002, type: 'subway', passengers: 70000 },

  // ═══ SAPPORO SUBWAY ═══
  { name: 'さっぽろ', line: '南北線', operator: 'Sapporo Subway', lat: 43.0687, lon: 141.3508, type: 'subway', passengers: 60000 },
  { name: '大通', line: '南北線', operator: 'Sapporo Subway', lat: 43.0606, lon: 141.3560, type: 'subway', passengers: 80000 },
  { name: 'すすきの', line: '南北線', operator: 'Sapporo Subway', lat: 43.0556, lon: 141.3530, type: 'subway', passengers: 40000 },

  // ═══ OTHER SUBWAYS ═══
  { name: '仙台', line: '南北線', operator: 'Sendai Subway', lat: 38.2601, lon: 140.8822, type: 'subway', passengers: 35000 },
  { name: '天神', line: '空港線', operator: 'Fukuoka Subway', lat: 33.5898, lon: 130.3987, type: 'subway', passengers: 55000 },
  { name: '博多 (地下鉄)', line: '空港線', operator: 'Fukuoka Subway', lat: 33.5897, lon: 130.4207, type: 'subway', passengers: 60000 },
  { name: '三宮 (地下鉄)', line: '西神・山手線', operator: 'Kobe Subway', lat: 34.6951, lon: 135.1979, type: 'subway', passengers: 40000 },
  { name: '烏丸御池', line: '烏丸線', operator: 'Kyoto Subway', lat: 35.0095, lon: 135.7594, type: 'subway', passengers: 25000 },
  { name: '四条', line: '烏丸線', operator: 'Kyoto Subway', lat: 35.0030, lon: 135.7594, type: 'subway', passengers: 30000 },

  // ═══ MAJOR JR STATIONS (non-Shinkansen) ═══
  { name: '新宿', line: '中央線/山手線', operator: 'JR East', lat: 35.6896, lon: 139.7006, type: 'jr', passengers: 770000 },
  { name: '池袋', line: '山手線', operator: 'JR East', lat: 35.7295, lon: 139.7109, type: 'jr', passengers: 560000 },
  { name: '横浜', line: '東海道線', operator: 'JR East', lat: 35.4660, lon: 139.6223, type: 'jr', passengers: 420000 },
  { name: '大阪', line: '環状線', operator: 'JR West', lat: 34.7024, lon: 135.4959, type: 'jr', passengers: 430000 },
  { name: '天王寺', line: '環状線', operator: 'JR West', lat: 34.6468, lon: 135.5135, type: 'jr', passengers: 140000 },
  { name: '三ノ宮', line: '東海道線', operator: 'JR West', lat: 34.6951, lon: 135.1979, type: 'jr', passengers: 130000 },
  { name: '札幌', line: '函館本線', operator: 'JR Hokkaido', lat: 43.0687, lon: 141.3508, type: 'jr', passengers: 100000 },
  { name: '旭川', line: '函館本線', operator: 'JR Hokkaido', lat: 43.7631, lon: 142.3581, type: 'jr', passengers: 8000 },
  { name: '函館', line: '函館本線', operator: 'JR Hokkaido', lat: 41.7738, lon: 140.7269, type: 'jr', passengers: 7000 },
  { name: '秋田', line: '奥羽本線', operator: 'JR East', lat: 39.7183, lon: 140.1261, type: 'jr', passengers: 12000 },
  { name: '山形', line: '奥羽本線', operator: 'JR East', lat: 38.2483, lon: 140.3281, type: 'jr', passengers: 10000 },
  { name: '新潟', line: '信越本線', operator: 'JR East', lat: 37.9106, lon: 139.0564, type: 'jr', passengers: 35000 },
  { name: '高松', line: '予讃線', operator: 'JR Shikoku', lat: 34.3501, lon: 134.0467, type: 'jr', passengers: 14000 },
  { name: '松山', line: '予讃線', operator: 'JR Shikoku', lat: 33.8395, lon: 132.7544, type: 'jr', passengers: 12000 },
  { name: '徳島', line: '高徳線', operator: 'JR Shikoku', lat: 34.0744, lon: 134.5517, type: 'jr', passengers: 8000 },
  { name: '高知', line: '土讃線', operator: 'JR Shikoku', lat: 33.5667, lon: 133.5436, type: 'jr', passengers: 6000 },
  { name: '長崎', line: '長崎本線', operator: 'JR Kyushu', lat: 32.7503, lon: 129.8777, type: 'jr', passengers: 10000 },
  { name: '大分', line: '日豊本線', operator: 'JR Kyushu', lat: 33.2328, lon: 131.6067, type: 'jr', passengers: 12000 },
  { name: '宮崎', line: '日豊本線', operator: 'JR Kyushu', lat: 31.9164, lon: 131.4272, type: 'jr', passengers: 7000 },

  // ═══ PRIVATE RAILWAYS ═══
  // Kanto
  { name: '渋谷 (東急)', line: '東急東横線', operator: 'Tokyu', lat: 35.6580, lon: 139.7016, type: 'private', passengers: 370000 },
  { name: '自由が丘', line: '東急東横線', operator: 'Tokyu', lat: 35.6076, lon: 139.6688, type: 'private', passengers: 120000 },
  { name: '武蔵小杉', line: '東急東横線', operator: 'Tokyu', lat: 35.5767, lon: 139.6588, type: 'private', passengers: 210000 },
  { name: '横浜 (東急)', line: '東急東横線', operator: 'Tokyu', lat: 35.4660, lon: 139.6223, type: 'private', passengers: 160000 },
  { name: '二子玉川', line: '東急田園都市線', operator: 'Tokyu', lat: 35.6119, lon: 139.6267, type: 'private', passengers: 140000 },
  { name: '新宿 (小田急)', line: '小田急線', operator: 'Odakyu', lat: 35.6896, lon: 139.6993, type: 'private', passengers: 490000 },
  { name: '町田 (小田急)', line: '小田急線', operator: 'Odakyu', lat: 35.5424, lon: 139.4467, type: 'private', passengers: 130000 },
  { name: '新宿 (京王)', line: '京王線', operator: 'Keio', lat: 35.6903, lon: 139.6988, type: 'private', passengers: 370000 },
  { name: '吉祥寺 (京王)', line: '京王井の頭線', operator: 'Keio', lat: 35.7030, lon: 139.5795, type: 'private', passengers: 150000 },
  { name: '池袋 (西武)', line: '西武池袋線', operator: 'Seibu', lat: 35.7295, lon: 139.7109, type: 'private', passengers: 200000 },
  { name: '池袋 (東武)', line: '東武東上線', operator: 'Tobu', lat: 35.7295, lon: 139.7146, type: 'private', passengers: 180000 },
  { name: '浅草 (東武)', line: '東武スカイツリーライン', operator: 'Tobu', lat: 35.7102, lon: 139.7988, type: 'private', passengers: 60000 },
  { name: '京成上野', line: '京成本線', operator: 'Keisei', lat: 35.7125, lon: 139.7732, type: 'private', passengers: 35000 },
  { name: '押上 (京成)', line: '京成押上線', operator: 'Keisei', lat: 35.7108, lon: 139.8131, type: 'private', passengers: 50000 },
  // Kansai
  { name: '梅田 (阪急)', line: '阪急神戸線', operator: 'Hankyu', lat: 34.7055, lon: 135.4977, type: 'private', passengers: 210000 },
  { name: '河原町', line: '阪急京都線', operator: 'Hankyu', lat: 35.0040, lon: 135.7693, type: 'private', passengers: 50000 },
  { name: '三宮 (阪急)', line: '阪急神戸線', operator: 'Hankyu', lat: 34.6958, lon: 135.1964, type: 'private', passengers: 60000 },
  { name: '梅田 (阪神)', line: '阪神本線', operator: 'Hanshin', lat: 34.7008, lon: 135.4975, type: 'private', passengers: 120000 },
  { name: '難波 (近鉄)', line: '近鉄奈良線', operator: 'Kintetsu', lat: 34.6627, lon: 135.5010, type: 'private', passengers: 200000 },
  { name: '鶴橋', line: '近鉄大阪線', operator: 'Kintetsu', lat: 34.6691, lon: 135.5294, type: 'private', passengers: 110000 },
  { name: '奈良 (近鉄)', line: '近鉄奈良線', operator: 'Kintetsu', lat: 34.6808, lon: 135.8266, type: 'private', passengers: 30000 },
  { name: '京都 (近鉄)', line: '近鉄京都線', operator: 'Kintetsu', lat: 34.9856, lon: 135.7581, type: 'private', passengers: 55000 },
  { name: '難波 (南海)', line: '南海本線', operator: 'Nankai', lat: 34.6627, lon: 135.5010, type: 'private', passengers: 260000 },
  { name: '関西空港 (南海)', line: '南海空港線', operator: 'Nankai', lat: 34.4320, lon: 135.2302, type: 'private', passengers: 35000 },
  // Chubu
  { name: '名鉄名古屋', line: '名鉄名古屋本線', operator: 'Meitetsu', lat: 35.1709, lon: 136.8836, type: 'private', passengers: 280000 },
  { name: '金山 (名鉄)', line: '名鉄名古屋本線', operator: 'Meitetsu', lat: 35.1440, lon: 136.9002, type: 'private', passengers: 70000 },
  // Kyushu
  { name: '天神 (西鉄)', line: '西鉄天神大牟田線', operator: 'Nishitetsu', lat: 33.5898, lon: 130.3987, type: 'private', passengers: 90000 },

  // ═══ TRAMS / LRT / MONORAILS ═══
  { name: '広島駅 (路面電車)', line: '広電', operator: 'Hiroden', lat: 34.3978, lon: 132.4752, type: 'tram', passengers: 10000 },
  { name: '原爆ドーム前', line: '広電', operator: 'Hiroden', lat: 34.3955, lon: 132.4534, type: 'tram', passengers: 8000 },
  { name: 'はりまや橋', line: 'とさでん', operator: 'Tosaden', lat: 33.5600, lon: 133.5425, type: 'tram', passengers: 3000 },
  { name: '鹿児島中央駅前', line: '鹿児島市電', operator: 'Kagoshima City', lat: 31.5842, lon: 130.5414, type: 'tram', passengers: 5000 },
  { name: '天文館通', line: '鹿児島市電', operator: 'Kagoshima City', lat: 31.5901, lon: 130.5558, type: 'tram', passengers: 4000 },
  { name: '松山市駅前', line: '伊予鉄道', operator: 'Iyotetsu', lat: 33.8371, lon: 132.7603, type: 'tram', passengers: 3000 },
  { name: '道後温泉', line: '伊予鉄道', operator: 'Iyotetsu', lat: 33.8520, lon: 132.7874, type: 'tram', passengers: 5000 },
  { name: '長崎駅前', line: '長崎電気軌道', operator: 'Nagasaki Electric', lat: 32.7525, lon: 129.8694, type: 'tram', passengers: 4000 },
  { name: 'グラバー園下', line: '長崎電気軌道', operator: 'Nagasaki Electric', lat: 32.7331, lon: 129.8681, type: 'tram', passengers: 3000 },
  { name: '富山駅', line: '富山ライトレール', operator: 'Toyama LRT', lat: 36.7014, lon: 137.2131, type: 'tram', passengers: 5000 },
  { name: '羽田空港第2ターミナル', line: '東京モノレール', operator: 'Tokyo Monorail', lat: 35.5494, lon: 139.7798, type: 'monorail', passengers: 35000 },
  { name: '浜松町', line: '東京モノレール', operator: 'Tokyo Monorail', lat: 35.6555, lon: 139.7570, type: 'monorail', passengers: 85000 },
  { name: '大阪空港', line: '大阪モノレール', operator: 'Osaka Monorail', lat: 34.7855, lon: 135.4380, type: 'monorail', passengers: 25000 },
  { name: 'ゆいレール 那覇空港', line: 'ゆいレール', operator: 'Okinawa Monorail', lat: 26.2058, lon: 127.6517, type: 'monorail', passengers: 15000 },
  { name: 'ゆいレール 首里', line: 'ゆいレール', operator: 'Okinawa Monorail', lat: 26.2194, lon: 127.7192, type: 'monorail', passengers: 10000 },
];

function generateSeedData() {
  const now = new Date();
  return STATIONS.map((s, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [s.lon, s.lat] },
    properties: {
      station_id: `STN_${String(i + 1).padStart(4, '0')}`,
      name: s.name,
      line: s.line,
      operator: s.operator,
      station_type: s.type,
      daily_passengers: s.passengers,
      passenger_category: s.passengers > 200000 ? 'mega' : s.passengers > 50000 ? 'major' : s.passengers > 10000 ? 'medium' : 'local',
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'full_transport',
    },
  }));
}

export default async function collectFullTransport() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'full_transport',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'osm_overpass' : 'rail_network_seed',
      description: 'Complete Japan rail network - Shinkansen, JR, Metro, Private, Tram, Monorail stations',
    },
    metadata: {},
  };
}
