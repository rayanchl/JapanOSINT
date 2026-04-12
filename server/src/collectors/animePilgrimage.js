/**
 * Anime Pilgrimage (聖地巡礼) Collector
 * Real-world locations featured in anime/manga — Seichi Junrei sites.
 * Live: OSM Overpass for Wikidata-linked POIs; falls back to curated Animedia list.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpass(
    'node["subject:wikidata"](area.jp);node["tourism"="attraction"]["subject"~"anime|manga"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        site_id: `OSM_${el.id}`,
        name: el.tags?.['name:en'] || el.tags?.name || `Pilgrimage ${i + 1}`,
        name_ja: el.tags?.name || null,
        subject: el.tags?.subject || el.tags?.['subject:wikidata'] || null,
        wikidata: el.tags?.wikidata || null,
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

const SEED_PILGRIMAGE = [
  // ── Your Name (君の名は) ──────────────────────────────
  { name: '須賀神社 男坂 (君の名は)', lat: 35.6878, lon: 139.7228, work: '君の名は。', studio: 'CoMix Wave', prefecture: '東京都' },
  { name: '新宿 NTTドコモ代々木ビル (君の名は)', lat: 35.6856, lon: 139.7014, work: '君の名は。', studio: 'CoMix Wave', prefecture: '東京都' },
  { name: '飛騨古川駅 (君の名は)', lat: 36.2392, lon: 137.1861, work: '君の名は。', studio: 'CoMix Wave', prefecture: '岐阜県' },
  { name: '飛騨市図書館 (君の名は)', lat: 36.2325, lon: 137.1836, work: '君の名は。', studio: 'CoMix Wave', prefecture: '岐阜県' },
  { name: '気多若宮神社 (君の名は)', lat: 36.2475, lon: 137.1808, work: '君の名は。', studio: 'CoMix Wave', prefecture: '岐阜県' },

  // ── Weathering With You (天気の子) ──────────────────
  { name: '代々木会館跡地 (天気の子)', lat: 35.6817, lon: 139.7031, work: '天気の子', studio: 'CoMix Wave', prefecture: '東京都' },
  { name: '田端駅 (天気の子)', lat: 35.7378, lon: 139.7614, work: '天気の子', studio: 'CoMix Wave', prefecture: '東京都' },
  { name: '朝日稲荷神社 (天気の子)', lat: 35.6725, lon: 139.7694, work: '天気の子', studio: 'CoMix Wave', prefecture: '東京都' },

  // ── Suzume (すずめの戸締まり) ───────────────────────
  { name: '大洗磯前神社 (すずめ)', lat: 36.3131, lon: 140.5822, work: 'すずめの戸締まり', studio: 'CoMix Wave', prefecture: '茨城県' },
  { name: '神戸ハーバーランド (すずめ)', lat: 34.6789, lon: 135.1861, work: 'すずめの戸締まり', studio: 'CoMix Wave', prefecture: '兵庫県' },
  { name: '愛媛県八幡浜市 (すずめ)', lat: 33.4619, lon: 132.4231, work: 'すずめの戸締まり', studio: 'CoMix Wave', prefecture: '愛媛県' },

  // ── K-On! (けいおん!) ──────────────────────────────
  { name: '豊郷小学校旧校舎 (けいおん!)', lat: 35.1922, lon: 136.2425, work: 'けいおん!', studio: 'Kyoto Animation', prefecture: '滋賀県' },

  // ── Lucky Star (らき☆すた) ───────────────────────────
  { name: '鷲宮神社 (らき☆すた)', lat: 36.1000, lon: 139.6586, work: 'らき☆すた', studio: 'Kyoto Animation', prefecture: '埼玉県' },

  // ── Girls und Panzer (ガールズ&パンツァー) ───────
  { name: '大洗磯前神社 (ガルパン)', lat: 36.3131, lon: 140.5822, work: 'ガールズ&パンツァー', studio: 'Actas', prefecture: '茨城県' },
  { name: '大洗駅 (ガルパン)', lat: 36.3125, lon: 140.5633, work: 'ガールズ&パンツァー', studio: 'Actas', prefecture: '茨城県' },
  { name: 'マリンタワー大洗 (ガルパン)', lat: 36.3136, lon: 140.5786, work: 'ガールズ&パンツァー', studio: 'Actas', prefecture: '茨城県' },

  // ── Love Live! (ラブライブ!) ─────────────────────────
  { name: '神田明神 (ラブライブ!)', lat: 35.7025, lon: 139.7678, work: 'ラブライブ!', studio: 'Sunrise', prefecture: '東京都' },
  { name: '沼津あわしまマリンパーク (Sunshine!!)', lat: 35.0444, lon: 138.8542, work: 'ラブライブ!サンシャイン!!', studio: 'Sunrise', prefecture: '静岡県' },
  { name: '内浦漁港 (Sunshine!!)', lat: 35.0206, lon: 138.8447, work: 'ラブライブ!サンシャイン!!', studio: 'Sunrise', prefecture: '静岡県' },
  { name: '沼津駅 (Sunshine!!)', lat: 35.1056, lon: 138.8639, work: 'ラブライブ!サンシャイン!!', studio: 'Sunrise', prefecture: '静岡県' },

  // ── Haikyuu!! (ハイキュー!!) ─────────────────────────
  { name: '烏野高校モデル (烏川高校)', lat: 38.8406, lon: 140.7750, work: 'ハイキュー!!', studio: 'Production I.G', prefecture: '宮城県' },

  // ── Demon Slayer (鬼滅の刃) ──────────────────────────
  { name: '雲取山 (鬼滅の刃)', lat: 35.8528, lon: 138.9433, work: '鬼滅の刃', studio: 'ufotable', prefecture: '東京都' },
  { name: '宝満宮 竈門神社 (鬼滅の刃)', lat: 33.5397, lon: 130.5433, work: '鬼滅の刃', studio: 'ufotable', prefecture: '福岡県' },

  // ── Jujutsu Kaisen (呪術廻戦) ────────────────────────
  { name: '渋谷駅 (呪術廻戦)', lat: 35.6580, lon: 139.7016, work: '呪術廻戦', studio: 'MAPPA', prefecture: '東京都' },
  { name: '原宿竹下通り (呪術廻戦)', lat: 35.6708, lon: 139.7039, work: '呪術廻戦', studio: 'MAPPA', prefecture: '東京都' },

  // ── Attack on Titan (進撃の巨人) ─────────────────────
  { name: '日田市大山ダム (進撃の巨人)', lat: 33.3344, lon: 130.9336, work: '進撃の巨人', studio: 'Wit Studio', prefecture: '大分県' },

  // ── Your Lie in April (四月は君の嘘) ─────────────────
  { name: '練馬区立野銀座通り (四月は君の嘘)', lat: 35.7278, lon: 139.6517, work: '四月は君の嘘', studio: 'A-1 Pictures', prefecture: '東京都' },

  // ── A Silent Voice (聲の形) ──────────────────────────
  { name: '大垣駅 (聲の形)', lat: 35.3653, lon: 136.6181, work: '聲の形', studio: 'Kyoto Animation', prefecture: '岐阜県' },
  { name: '四季の広場 (聲の形)', lat: 35.3628, lon: 136.6178, work: '聲の形', studio: 'Kyoto Animation', prefecture: '岐阜県' },
  { name: '美登鯉橋 (聲の形)', lat: 35.3639, lon: 136.6208, work: '聲の形', studio: 'Kyoto Animation', prefecture: '岐阜県' },

  // ── Hanasaku Iroha (花咲くいろは) ────────────────────
  { name: '湯涌温泉 (花咲くいろは)', lat: 36.4969, lon: 136.7358, work: '花咲くいろは', studio: 'P.A. Works', prefecture: '石川県' },

  // ── True Tears / Tari Tari / Another ─────────────────
  { name: '城端駅 (true tears)', lat: 36.5886, lon: 136.9083, work: 'true tears', studio: 'P.A. Works', prefecture: '富山県' },
  { name: '江ノ島 (TARI TARI)', lat: 35.2989, lon: 139.4783, work: 'TARI TARI', studio: 'P.A. Works', prefecture: '神奈川県' },
  { name: '夜見山市立夜見北中学校モデル (Another)', lat: 35.4097, lon: 137.0458, work: 'Another', studio: 'P.A. Works', prefecture: '愛知県' },

  // ── Free! (Iwatobi Swim Club) ────────────────────────
  { name: '岩美駅 (Free!)', lat: 35.5694, lon: 134.3075, work: 'Free!', studio: 'Kyoto Animation', prefecture: '鳥取県' },
  { name: '網代漁港 (Free!)', lat: 35.5619, lon: 134.3214, work: 'Free!', studio: 'Kyoto Animation', prefecture: '鳥取県' },

  // ── Nagi no Asukara (凪のあすから) ───────────────────
  { name: '三重県志摩市 (凪のあすから)', lat: 34.3269, lon: 136.8417, work: '凪のあすから', studio: 'P.A. Works', prefecture: '三重県' },

  // ── Silver Spoon (銀の匙) ────────────────────────────
  { name: '帯広農業高等学校 (銀の匙)', lat: 42.9053, lon: 143.1939, work: '銀の匙', studio: 'A-1 Pictures', prefecture: '北海道' },

  // ── Non Non Biyori (のんのんびより) ──────────────────
  { name: '旧木沢小学校 (のんのんびより)', lat: 35.2525, lon: 137.7706, work: 'のんのんびより', studio: 'Silver Link', prefecture: '長野県' },

  // ── Barakamon (ばらかもん) ───────────────────────────
  { name: '福江島 (ばらかもん)', lat: 32.6914, lon: 128.8419, work: 'ばらかもん', studio: 'Kinema Citrus', prefecture: '長崎県' },

  // ── Kimi ni Todoke (君に届け) ────────────────────────
  { name: '旭川 (君に届け)', lat: 43.7706, lon: 142.3650, work: '君に届け', studio: 'Production I.G', prefecture: '北海道' },

  // ── The Melancholy of Haruhi Suzumiya (涼宮ハルヒ) ──
  { name: '阪急甲陽園駅 (ハルヒ)', lat: 34.7544, lon: 135.3386, work: '涼宮ハルヒの憂鬱', studio: 'Kyoto Animation', prefecture: '兵庫県' },
  { name: '西宮北高校 (ハルヒ)', lat: 34.7597, lon: 135.3272, work: '涼宮ハルヒの憂鬱', studio: 'Kyoto Animation', prefecture: '兵庫県' },

  // ── Place Promised in Our Early Days (雲のむこう) ───
  { name: '津軽大橋 (雲のむこう、約束の場所)', lat: 40.6997, lon: 140.7611, work: '雲のむこう、約束の場所', studio: 'CoMix Wave', prefecture: '青森県' },

  // ── 5 Centimeters per Second (秒速5センチメートル) ──
  { name: '参宮橋駅 (秒速5センチメートル)', lat: 35.6828, lon: 139.6925, work: '秒速5センチメートル', studio: 'CoMix Wave', prefecture: '東京都' },
  { name: '岩舟駅 (秒速5センチメートル)', lat: 36.3461, lon: 139.6419, work: '秒速5センチメートル', studio: 'CoMix Wave', prefecture: '栃木県' },

  // ── Grave of the Fireflies / Totoro / Ghibli ─────────
  { name: '三鷹の森ジブリ美術館', lat: 35.6964, lon: 139.5703, work: 'ジブリ全般', studio: 'Studio Ghibli', prefecture: '東京都' },
  { name: 'トトロの森 八国山緑地', lat: 35.7622, lon: 139.4711, work: 'となりのトトロ', studio: 'Studio Ghibli', prefecture: '埼玉県' },
  { name: '湯本屋 (千と千尋)', lat: 24.3544, lon: 124.1939, work: '千と千尋の神隠し', studio: 'Studio Ghibli', prefecture: '愛媛県' },

  // ── Violet Evergarden (ヴァイオレット・エヴァーガーデン) ──
  { name: '京都府立図書館 (Violet)', lat: 35.0136, lon: 135.7819, work: 'ヴァイオレット・エヴァーガーデン', studio: 'Kyoto Animation', prefecture: '京都府' },

  // ── Koe no Katachi / Wolf Children / Summer Wars (Hosoda) ──
  { name: '上田市 (サマーウォーズ)', lat: 36.4016, lon: 138.2486, work: 'サマーウォーズ', studio: 'Studio Chizu', prefecture: '長野県' },
  { name: '富山県南砺市 (おおかみこどもの雨と雪)', lat: 36.5836, lon: 136.9097, work: 'おおかみこどもの雨と雪', studio: 'Studio Chizu', prefecture: '富山県' },

  // ── Madoka Magica / Re:Zero / SAO ────────────────────
  { name: '見滝原モデル 新都心 (まどマギ)', lat: 35.8936, lon: 139.6297, work: '魔法少女まどか☆マギカ', studio: 'SHAFT', prefecture: '埼玉県' },

  // ── Yuru Camp (ゆるキャン△) ─────────────────────────
  { name: '本栖湖 浩庵キャンプ場 (ゆるキャン△)', lat: 35.4558, lon: 138.5944, work: 'ゆるキャン△', studio: 'C-Station', prefecture: '山梨県' },
  { name: '麓キャンプ場 (ゆるキャン△)', lat: 35.4431, lon: 138.6417, work: 'ゆるキャン△', studio: 'C-Station', prefecture: '山梨県' },

  // ── Barakamon / Poco / Mushishi / Spice and Wolf ─────
  { name: '広島県 尾道 (朝霧の巫女)', lat: 34.4089, lon: 133.2053, work: '朝霧の巫女', studio: 'GONZO', prefecture: '広島県' },

  // ── Flying Witch (ふらいんぐうぃっち) ────────────────
  { name: '弘前 岩木山 (ふらいんぐうぃっち)', lat: 40.6565, lon: 140.3044, work: 'ふらいんぐうぃっち', studio: 'J.C.Staff', prefecture: '青森県' },

  // ── Tamako Market (たまこまーけっと) ─────────────────
  { name: '出町柳 桝形商店街 (たまこまーけっと)', lat: 35.0306, lon: 135.7722, work: 'たまこまーけっと', studio: 'Kyoto Animation', prefecture: '京都府' },
];

function generateSeedData() {
  return SEED_PILGRIMAGE.map((p, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [p.lon, p.lat] },
    properties: {
      site_id: `ANIME_${String(i + 1).padStart(5, '0')}`,
      name: p.name,
      work: p.work,
      studio: p.studio,
      prefecture: p.prefecture,
      country: 'JP',
      source: 'anime_pilgrimage_seed',
    },
  }));
}

export default async function collectAnimePilgrimage() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'anime-pilgrimage',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'osm_overpass' : 'anime_pilgrimage_seed',
      description: 'Anime seichi junrei - real-world locations featured in anime/manga',
    },
    metadata: {},
  };
}
