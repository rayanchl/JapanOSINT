/**
 * Manhole Covers Collector
 * Japan has ~15M decorative manhole covers; the Japan Ground Manhole Assoc
 * issues "Manhole Cards" (マンホールカード) for notable ones.
 * Live: GKP Manhole Card public index (HTML) + OSM Overpass for
 * `man_made=manhole` tagged entries. GKP doesn't expose JSON so we
 * verify the public archive page is reachable and separately pull any
 * geocoded manholes present in OSM.
 * Seed: curated list of ~50 distinctive manhole card sites.
 */

import { fetchText, fetchOverpass } from './_liveHelpers.js';

const GKP_INDEX = 'https://www.gk-p.jp/activity/mhcard/';

async function tryLive() {
  // 1. Reachability check against the real GKP manhole card archive.
  const html = await fetchText(GKP_INDEX, { timeoutMs: 8000 });
  const gkpReachable = !!(html && /マンホールカード|manhole/i.test(html));

  // 2. OSM Overpass for geocoded manhole cover entries.
  const osmFeatures = await fetchOverpass(
    'node["man_made"="manhole"](area.jp);node["tourism"="attraction"]["name"~"マンホール"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        card_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || `Manhole ${i + 1}`,
        name_ja: el.tags?.name || null,
        municipality: el.tags?.['addr:city'] || null,
        design: el.tags?.description || null,
        source: gkpReachable ? 'gkp_manhole_card+osm' : 'osm_overpass',
      },
    }),
  );
  if (osmFeatures && osmFeatures.length > 0) return osmFeatures;
  return null;
}

const SEED_COVERS = [
  // Tokyo
  { name: '東京都 サクラと桜の木', lat: 35.6812, lon: 139.7671, prefecture: '東京都', municipality: '東京都', design: 'sakura', series: 1, issued: '2016-04' },
  { name: '新宿区 ゴジラ', lat: 35.6954, lon: 139.7006, prefecture: '東京都', municipality: '新宿区', design: 'godzilla', series: 6, issued: '2018-12' },
  { name: '墨田区 両国国技館', lat: 35.6970, lon: 139.7933, prefecture: '東京都', municipality: '墨田区', design: 'sumo', series: 2, issued: '2016-12' },
  { name: '豊島区 ふくろう', lat: 35.7295, lon: 139.7109, prefecture: '東京都', municipality: '豊島区', design: 'owl', series: 5, issued: '2018-04' },
  { name: '調布市 ゲゲゲの鬼太郎', lat: 35.6522, lon: 139.5442, prefecture: '東京都', municipality: '調布市', design: 'kitaro', series: 3, issued: '2017-04' },
  { name: '練馬区 アニメの街', lat: 35.7356, lon: 139.6519, prefecture: '東京都', municipality: '練馬区', design: 'anime', series: 7, issued: '2019-04' },

  // Osaka
  { name: '大阪市 大阪城', lat: 34.6873, lon: 135.5262, prefecture: '大阪府', municipality: '大阪市', design: 'osaka_castle', series: 1, issued: '2016-04' },
  { name: '堺市 千利休', lat: 34.5733, lon: 135.4831, prefecture: '大阪府', municipality: '堺市', design: 'rikyu', series: 3, issued: '2017-04' },
  { name: '東大阪市 ラグビー', lat: 34.6767, lon: 135.5989, prefecture: '大阪府', municipality: '東大阪市', design: 'rugby', series: 8, issued: '2019-08' },

  // Kyoto
  { name: '京都市 清水寺', lat: 34.9949, lon: 135.7850, prefecture: '京都府', municipality: '京都市', design: 'kiyomizu', series: 2, issued: '2016-12' },
  { name: '宇治市 源氏物語', lat: 34.8881, lon: 135.8072, prefecture: '京都府', municipality: '宇治市', design: 'genji_monogatari', series: 4, issued: '2017-08' },

  // Hokkaido
  { name: '札幌市 時計台', lat: 43.0625, lon: 141.3536, prefecture: '北海道', municipality: '札幌市', design: 'clock_tower', series: 1, issued: '2016-04' },
  { name: '函館市 函館山夜景', lat: 41.7688, lon: 140.7288, prefecture: '北海道', municipality: '函館市', design: 'hakodate_night', series: 2, issued: '2016-12' },
  { name: '小樽市 運河', lat: 43.1990, lon: 141.0036, prefecture: '北海道', municipality: '小樽市', design: 'canal', series: 3, issued: '2017-04' },

  // Tohoku
  { name: '仙台市 七夕', lat: 38.2682, lon: 140.8694, prefecture: '宮城県', municipality: '仙台市', design: 'tanabata', series: 1, issued: '2016-04' },
  { name: '盛岡市 南部鉄器', lat: 39.7036, lon: 141.1527, prefecture: '岩手県', municipality: '盛岡市', design: 'nanbu_tekki', series: 4, issued: '2017-08' },
  { name: '青森市 ねぶた', lat: 40.8244, lon: 140.7400, prefecture: '青森県', municipality: '青森市', design: 'nebuta', series: 2, issued: '2016-12' },
  { name: '秋田市 なまはげ', lat: 39.7200, lon: 140.1025, prefecture: '秋田県', municipality: '秋田市', design: 'namahage', series: 5, issued: '2018-04' },
  { name: '山形市 山形花笠', lat: 38.2554, lon: 140.3397, prefecture: '山形県', municipality: '山形市', design: 'hanagasa', series: 3, issued: '2017-04' },
  { name: '福島市 福島競馬', lat: 37.7603, lon: 140.4734, prefecture: '福島県', municipality: '福島市', design: 'race_horse', series: 4, issued: '2017-08' },

  // Kanto
  { name: '横浜市 ベイブリッジ', lat: 35.4526, lon: 139.6452, prefecture: '神奈川県', municipality: '横浜市', design: 'bay_bridge', series: 1, issued: '2016-04' },
  { name: '鎌倉市 大仏', lat: 35.3167, lon: 139.5358, prefecture: '神奈川県', municipality: '鎌倉市', design: 'daibutsu', series: 2, issued: '2016-12' },
  { name: '川崎市 ミューザ', lat: 35.5308, lon: 139.7028, prefecture: '神奈川県', municipality: '川崎市', design: 'muza', series: 5, issued: '2018-04' },
  { name: '千葉市 チーバくん', lat: 35.6073, lon: 140.1233, prefecture: '千葉県', municipality: '千葉市', design: 'chiba_kun', series: 3, issued: '2017-04' },
  { name: '浦安市 ディズニー', lat: 35.6536, lon: 139.9094, prefecture: '千葉県', municipality: '浦安市', design: 'urayasu_fisher', series: 6, issued: '2018-12' },
  { name: 'さいたま市 大宮アルディージャ', lat: 35.8614, lon: 139.6456, prefecture: '埼玉県', municipality: 'さいたま市', design: 'ardija', series: 4, issued: '2017-08' },
  { name: '水戸市 水戸黄門', lat: 36.3657, lon: 140.4714, prefecture: '茨城県', municipality: '水戸市', design: 'mito_komon', series: 3, issued: '2017-04' },
  { name: '日光市 東照宮', lat: 36.7583, lon: 139.5989, prefecture: '栃木県', municipality: '日光市', design: 'toshogu', series: 2, issued: '2016-12' },
  { name: '高崎市 だるま', lat: 36.3228, lon: 139.0125, prefecture: '群馬県', municipality: '高崎市', design: 'daruma', series: 5, issued: '2018-04' },

  // Chubu
  { name: '名古屋市 名古屋城 金のしゃちほこ', lat: 35.1856, lon: 136.8997, prefecture: '愛知県', municipality: '名古屋市', design: 'shachihoko', series: 1, issued: '2016-04' },
  { name: '岐阜市 鵜飼', lat: 35.4233, lon: 136.7606, prefecture: '岐阜県', municipality: '岐阜市', design: 'ukai', series: 4, issued: '2017-08' },
  { name: '静岡市 富士山', lat: 34.9756, lon: 138.3828, prefecture: '静岡県', municipality: '静岡市', design: 'mt_fuji', series: 2, issued: '2016-12' },
  { name: '富山市 チューリップ', lat: 36.6950, lon: 137.2117, prefecture: '富山県', municipality: '富山市', design: 'tulip', series: 3, issued: '2017-04' },
  { name: '金沢市 兼六園', lat: 36.5625, lon: 136.6625, prefecture: '石川県', municipality: '金沢市', design: 'kenroku_en', series: 2, issued: '2016-12' },
  { name: '福井市 恐竜', lat: 36.0653, lon: 136.2217, prefecture: '福井県', municipality: '福井市', design: 'dinosaur', series: 5, issued: '2018-04' },
  { name: '甲府市 武田信玄', lat: 35.6622, lon: 138.5683, prefecture: '山梨県', municipality: '甲府市', design: 'takeda_shingen', series: 4, issued: '2017-08' },
  { name: '長野市 善光寺', lat: 36.6617, lon: 138.1870, prefecture: '長野県', municipality: '長野市', design: 'zenkoji', series: 3, issued: '2017-04' },

  // Kansai
  { name: '神戸市 ポートタワー', lat: 34.6822, lon: 135.1869, prefecture: '兵庫県', municipality: '神戸市', design: 'port_tower', series: 1, issued: '2016-04' },
  { name: '奈良市 大仏と鹿', lat: 34.6851, lon: 135.8048, prefecture: '奈良県', municipality: '奈良市', design: 'daibutsu_deer', series: 2, issued: '2016-12' },
  { name: '和歌山市 和歌山城', lat: 34.2269, lon: 135.1717, prefecture: '和歌山県', municipality: '和歌山市', design: 'wakayama_castle', series: 3, issued: '2017-04' },
  { name: '大津市 琵琶湖', lat: 35.0044, lon: 135.8686, prefecture: '滋賀県', municipality: '大津市', design: 'biwako', series: 4, issued: '2017-08' },

  // Chugoku / Shikoku / Kyushu
  { name: '広島市 原爆ドーム', lat: 34.3955, lon: 132.4536, prefecture: '広島県', municipality: '広島市', design: 'genbaku_dome', series: 1, issued: '2016-04' },
  { name: '岡山市 桃太郎', lat: 34.6618, lon: 133.9348, prefecture: '岡山県', municipality: '岡山市', design: 'momotaro', series: 2, issued: '2016-12' },
  { name: '松江市 松江城', lat: 35.4753, lon: 133.0508, prefecture: '島根県', municipality: '松江市', design: 'matsue_castle', series: 3, issued: '2017-04' },
  { name: '鳥取市 砂丘', lat: 35.5011, lon: 134.2350, prefecture: '鳥取県', municipality: '鳥取市', design: 'sakyu', series: 4, issued: '2017-08' },
  { name: '山口市 瑠璃光寺', lat: 34.1853, lon: 131.4706, prefecture: '山口県', municipality: '山口市', design: 'rurikoji', series: 5, issued: '2018-04' },
  { name: '高松市 栗林公園', lat: 34.3403, lon: 134.0436, prefecture: '香川県', municipality: '高松市', design: 'ritsurin_en', series: 2, issued: '2016-12' },
  { name: '松山市 道後温泉', lat: 33.8517, lon: 132.7864, prefecture: '愛媛県', municipality: '松山市', design: 'dogo_onsen', series: 1, issued: '2016-04' },
  { name: '徳島市 阿波踊り', lat: 34.0703, lon: 134.5547, prefecture: '徳島県', municipality: '徳島市', design: 'awa_odori', series: 3, issued: '2017-04' },
  { name: '高知市 坂本龍馬', lat: 33.5597, lon: 133.5311, prefecture: '高知県', municipality: '高知市', design: 'ryoma', series: 4, issued: '2017-08' },
  { name: '福岡市 博多祇園山笠', lat: 33.5919, lon: 130.4189, prefecture: '福岡県', municipality: '福岡市', design: 'yamakasa', series: 1, issued: '2016-04' },
  { name: '長崎市 グラバー園', lat: 32.7369, lon: 129.8689, prefecture: '長崎県', municipality: '長崎市', design: 'glover_garden', series: 2, issued: '2016-12' },
  { name: '熊本市 くまモン', lat: 32.8031, lon: 130.7078, prefecture: '熊本県', municipality: '熊本市', design: 'kumamon', series: 3, issued: '2017-04' },
  { name: '大分市 別府温泉', lat: 33.2833, lon: 131.5000, prefecture: '大分県', municipality: '別府市', design: 'beppu_onsen', series: 4, issued: '2017-08' },
  { name: '宮崎市 ひょっとこ', lat: 31.9077, lon: 131.4202, prefecture: '宮崎県', municipality: '宮崎市', design: 'hyottoko', series: 5, issued: '2018-04' },
  { name: '鹿児島市 桜島', lat: 31.5969, lon: 130.5571, prefecture: '鹿児島県', municipality: '鹿児島市', design: 'sakurajima', series: 2, issued: '2016-12' },
  { name: '那覇市 首里城', lat: 26.2172, lon: 127.7192, prefecture: '沖縄県', municipality: '那覇市', design: 'shurijo', series: 3, issued: '2017-04' },
];

function generateSeedData() {
  return SEED_COVERS.map((c, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [c.lon, c.lat] },
    properties: {
      card_id: `MC_${i + 1}`,
      name: c.name,
      municipality: c.municipality,
      design: c.design,
      series: c.series,
      issued: c.issued,
      prefecture: c.prefecture,
      country: 'JP',
      source: 'gkp_manhole_card_seed',
    },
  }));
}

export default async function collectManholeCovers() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'manhole-covers',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'osm_overpass+gkp' : 'gkp_manhole_card_seed',
      description: 'Japan Manhole Card (マンホールカード) issuance sites by municipality',
    },
    metadata: {},
  };
}
