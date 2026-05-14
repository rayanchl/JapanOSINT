/**
 * Ferry Routes / Terminals Collector
 * Live: OSM Overpass amenity=ferry_terminal across Japan.
 * Fallback: curated inter-island + inland sea + international terminals.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpass(
    'node["amenity"="ferry_terminal"](area.jp);way["amenity"="ferry_terminal"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        ferry_id: `OSM_${el.id}`,
        name: el.tags?.name || `Ferry terminal ${i + 1}`,
        operator: el.tags?.operator || 'unknown',
        type: 'ferry_terminal',
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

const FERRY_TERMINALS = [
  // Hokkaido <-> Honshu
  { name: '函館港フェリーターミナル', operator: '津軽海峡フェリー', lat: 41.7900, lon: 140.7100, type: 'inter_island', routes: ['函館-青森', '函館-大間'] },
  { name: '青森港フェリーターミナル', operator: '津軽海峡フェリー', lat: 40.8400, lon: 140.7500, type: 'inter_island', routes: ['青森-函館'] },
  { name: '大間港', operator: '津軽海峡フェリー', lat: 41.5300, lon: 140.9100, type: 'inter_island', routes: ['大間-函館'] },
  { name: '苫小牧港フェリーターミナル', operator: '商船三井/太平洋フェリー', lat: 42.6300, lon: 141.6300, type: 'inter_island', routes: ['苫小牧-大洗', '苫小牧-仙台-名古屋', '苫小牧-八戸'] },
  { name: '小樽港フェリーターミナル', operator: '新日本海フェリー', lat: 43.2000, lon: 141.0000, type: 'inter_island', routes: ['小樽-舞鶴', '小樽-新潟'] },
  { name: '室蘭港', operator: '商船三井フェリー', lat: 42.3200, lon: 140.9700, type: 'inter_island', routes: ['室蘭-八戸'] },
  { name: '八戸港フェリーターミナル', operator: '川崎近海汽船', lat: 40.5400, lon: 141.5300, type: 'inter_island', routes: ['八戸-苫小牧', '八戸-室蘭'] },
  { name: '大洗港フェリーターミナル', operator: '商船三井フェリー', lat: 36.3100, lon: 140.5800, type: 'inter_island', routes: ['大洗-苫小牧'] },
  { name: '仙台港フェリーターミナル', operator: '太平洋フェリー', lat: 38.2700, lon: 141.0200, type: 'inter_island', routes: ['仙台-名古屋', '仙台-苫小牧'] },
  { name: '新潟港フェリーターミナル', operator: '新日本海フェリー', lat: 37.9500, lon: 139.0600, type: 'inter_island', routes: ['新潟-小樽', '新潟-秋田-苫小牧'] },
  // Honshu <-> Shikoku/Kyushu
  { name: '神戸港 六甲アイランド', operator: '阪九フェリー', lat: 34.7000, lon: 135.2700, type: 'inter_island', routes: ['神戸-新門司'] },
  { name: '大阪南港フェリーターミナル', operator: '名門大洋/阪九フェリー', lat: 34.6300, lon: 135.4400, type: 'inter_island', routes: ['大阪-新門司', '大阪-志布志'] },
  { name: '泉大津港', operator: '阪九フェリー', lat: 34.5100, lon: 135.4100, type: 'inter_island', routes: ['泉大津-新門司'] },
  { name: '舞鶴港', operator: '新日本海フェリー', lat: 35.4700, lon: 135.3800, type: 'inter_island', routes: ['舞鶴-小樽'] },
  { name: '敦賀港', operator: '新日本海フェリー', lat: 35.6500, lon: 136.0700, type: 'inter_island', routes: ['敦賀-苫小牧', '敦賀-新潟'] },
  { name: '新門司港', operator: '阪九/名門大洋', lat: 33.9300, lon: 131.0100, type: 'inter_island', routes: ['新門司-大阪', '新門司-神戸'] },
  // Kyushu domestic
  { name: '鹿児島港 桜島フェリー', operator: '鹿児島市', lat: 31.5800, lon: 130.5700, type: 'island', routes: ['鹿児島-桜島'] },
  { name: '志布志港', operator: '商船三井さんふらわあ', lat: 31.4800, lon: 131.1100, type: 'inter_island', routes: ['志布志-大阪'] },
  { name: '宮崎港フェリーターミナル', operator: 'Marine Express', lat: 31.9700, lon: 131.4700, type: 'inter_island', routes: ['宮崎-神戸'] },
  { name: '別府国際観光港', operator: '関西汽船', lat: 33.2900, lon: 131.4900, type: 'inter_island', routes: ['別府-大阪', '別府-神戸'] },
  { name: '佐多岬港', operator: '南海郵船', lat: 31.0200, lon: 130.6700, type: 'island', routes: ['佐多-種子島'] },
  { name: '指宿港', operator: '商船三井', lat: 31.2500, lon: 130.6400, type: 'island', routes: ['指宿-種子島'] },
  // Seto Inland Sea
  { name: '高松港フェリーターミナル', operator: '四国フェリー/ジャンボフェリー', lat: 34.3500, lon: 134.0500, type: 'inland_sea', routes: ['高松-神戸', '高松-宇野', '高松-小豆島'] },
  { name: '宇野港', operator: '四国フェリー', lat: 34.4900, lon: 133.9500, type: 'inland_sea', routes: ['宇野-高松', '宇野-小豆島'] },
  { name: '土庄港 (小豆島)', operator: '四国フェリー', lat: 34.4900, lon: 134.1800, type: 'island', routes: ['土庄-高松', '土庄-神戸'] },
  { name: '坂手港 (小豆島)', operator: 'ジャンボフェリー', lat: 34.4800, lon: 134.3000, type: 'island', routes: ['坂手-神戸'] },
  { name: '宮島港', operator: 'JR西日本宮島フェリー', lat: 34.2960, lon: 132.3196, type: 'island', routes: ['宮島-宮島口'] },
  { name: '宮島口港', operator: 'JR西日本/松大汽船', lat: 34.3050, lon: 132.3200, type: 'island', routes: ['宮島口-宮島'] },
  { name: '広島港 宇品', operator: '瀬戸内シーライン', lat: 34.3500, lon: 132.4600, type: 'inland_sea', routes: ['広島-松山', '広島-呉'] },
  { name: '松山観光港', operator: '石崎汽船/瀬戸内海汽船', lat: 33.8700, lon: 132.7000, type: 'inland_sea', routes: ['松山-広島', '松山-小倉'] },
  { name: '今治港', operator: 'しまなみ海道航路', lat: 34.0700, lon: 132.9900, type: 'inland_sea', routes: ['今治-大三島'] },
  { name: '尾道港', operator: 'おのみち海運', lat: 34.4100, lon: 133.2000, type: 'inland_sea', routes: ['尾道-向島'] },
  // Honshu <-> Honshu islands
  { name: '熱海港', operator: '東海汽船', lat: 35.0900, lon: 139.0800, type: 'island', routes: ['熱海-初島', '熱海-大島'] },
  { name: '東京港 竹芝桟橋', operator: '東海汽船', lat: 35.6550, lon: 139.7600, type: 'island', routes: ['竹芝-大島', '竹芝-八丈島', '竹芝-小笠原'] },
  { name: '館山港', operator: '東京湾フェリー', lat: 34.9800, lon: 139.8300, type: 'inland_sea', routes: ['館山-久里浜'] },
  { name: '久里浜港', operator: '東京湾フェリー', lat: 35.2200, lon: 139.7100, type: 'inland_sea', routes: ['久里浜-金谷'] },
  { name: '金谷港', operator: '東京湾フェリー', lat: 35.1400, lon: 139.8200, type: 'inland_sea', routes: ['金谷-久里浜'] },
  // Okinawa
  { name: '那覇港 泊埠頭', operator: '琉球海運', lat: 26.2300, lon: 127.6800, type: 'island', routes: ['那覇-渡嘉敷', '那覇-座間味', '那覇-久米島', '那覇-宮古', '那覇-石垣'] },
  { name: '石垣港離島ターミナル', operator: '安栄観光', lat: 24.3400, lon: 124.1500, type: 'island', routes: ['石垣-竹富', '石垣-西表', '石垣-波照間'] },
  { name: '宮古港', operator: '琉球海運', lat: 24.7900, lon: 125.2800, type: 'island', routes: ['宮古-多良間', '宮古-那覇'] },
  { name: '本部港', operator: '伊江村営', lat: 26.6900, lon: 127.8800, type: 'island', routes: ['本部-伊江島'] },
  // Sea of Japan / Korea routes
  { name: '博多港 国際ターミナル', operator: 'JR九州高速船', lat: 33.6100, lon: 130.4000, type: 'international', routes: ['博多-釜山(BEETLE)'] },
  { name: '下関港 国際ターミナル', operator: '関釜フェリー', lat: 33.9500, lon: 130.9200, type: 'international', routes: ['下関-釜山'] },
  { name: '境港', operator: 'DBSクルーズフェリー', lat: 35.5400, lon: 133.2300, type: 'international', routes: ['境港-東海(韓国)-ウラジオストク'] },
];

function generateSeedData() {
  const now = new Date();
  return FERRY_TERMINALS.map((f, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [f.lon, f.lat] },
    properties: {
      ferry_id: `FERRY_${String(i + 1).padStart(4, '0')}`,
      name: f.name,
      operator: f.operator,
      ferry_type: f.type,
      routes: f.routes,
      route_count: f.routes.length,
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'ferry_routes',
    },
  }));
}

export default async function collectFerryRoutes() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'ferry_routes',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Ferry terminals across Japan - inter-island, inland sea, international routes',
    },
  };
}
