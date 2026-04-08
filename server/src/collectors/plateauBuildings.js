/**
 * PLATEAU Buildings Collector
 * 3D city model building footprints - Marunouchi/Tokyo Station area
 * Based on MLIT PLATEAU open data
 */

// Building footprints for Marunouchi / Tokyo Station area (~50 buildings)
// Real approximate coordinates and dimensions
const BUILDINGS = [
  { name: '東京駅丸の内駅舎', lat: 35.6812, lon: 139.7671, height: 25, floors: 3, use: '交通施設', year: 1914, w: 0.0020, h: 0.0004 },
  { name: '丸ビル', lat: 35.6815, lon: 139.7634, height: 180, floors: 37, use: '事務所', year: 2002, w: 0.0006, h: 0.0006 },
  { name: '新丸ビル', lat: 35.6828, lon: 139.7638, height: 198, floors: 38, use: '事務所', year: 2007, w: 0.0006, h: 0.0006 },
  { name: 'JPタワー（KITTE）', lat: 35.6793, lon: 139.7636, height: 200, floors: 38, use: '複合施設', year: 2012, w: 0.0006, h: 0.0007 },
  { name: '三菱一号館', lat: 35.6793, lon: 139.7621, height: 15, floors: 3, use: '美術館', year: 2009, w: 0.0004, h: 0.0003 },
  { name: 'パレスホテル東京', lat: 35.6849, lon: 139.7617, height: 115, floors: 23, use: 'ホテル', year: 2012, w: 0.0006, h: 0.0005 },
  { name: '東京ビルTOKIA', lat: 35.6778, lon: 139.7643, height: 150, floors: 33, use: '事務所', year: 2005, w: 0.0005, h: 0.0005 },
  { name: '国際ビル', lat: 35.6770, lon: 139.7620, height: 90, floors: 20, use: '事務所', year: 1966, w: 0.0005, h: 0.0005 },
  { name: '帝国劇場', lat: 35.6770, lon: 139.7610, height: 30, floors: 9, use: '劇場', year: 1966, w: 0.0005, h: 0.0004 },
  { name: '丸の内パークビル', lat: 35.6790, lon: 139.7613, height: 170, floors: 34, use: '事務所', year: 2009, w: 0.0005, h: 0.0005 },
  { name: '丸の内永楽ビル', lat: 35.6838, lon: 139.7650, height: 150, floors: 27, use: '事務所', year: 2012, w: 0.0005, h: 0.0005 },
  { name: '丸の内オアゾ', lat: 35.6833, lon: 139.7662, height: 180, floors: 37, use: '複合施設', year: 2004, w: 0.0006, h: 0.0006 },
  { name: 'サピアタワー', lat: 35.6843, lon: 139.7683, height: 172, floors: 35, use: '事務所', year: 2007, w: 0.0004, h: 0.0004 },
  { name: 'グラントウキョウノースタワー', lat: 35.6800, lon: 139.7690, height: 205, floors: 43, use: '事務所', year: 2007, w: 0.0005, h: 0.0005 },
  { name: 'グラントウキョウサウスタワー', lat: 35.6785, lon: 139.7688, height: 205, floors: 42, use: '事務所', year: 2007, w: 0.0005, h: 0.0005 },
  { name: '八重洲ミッドタウン', lat: 35.6808, lon: 139.7710, height: 240, floors: 45, use: '複合施設', year: 2022, w: 0.0006, h: 0.0006 },
  { name: '東京ミッドタウン八重洲', lat: 35.6795, lon: 139.7712, height: 240, floors: 45, use: '複合施設', year: 2023, w: 0.0006, h: 0.0005 },
  { name: '日本工業倶楽部会館', lat: 35.6838, lon: 139.7642, height: 25, floors: 5, use: '事務所', year: 1920, w: 0.0004, h: 0.0003 },
  { name: '三菱UFJ信託銀行本店ビル', lat: 35.6823, lon: 139.7660, height: 100, floors: 21, use: '事務所', year: 2003, w: 0.0005, h: 0.0004 },
  { name: '読売新聞東京本社ビル', lat: 35.6762, lon: 139.7639, height: 200, floors: 33, use: '事務所', year: 2014, w: 0.0005, h: 0.0005 },
  { name: 'DNタワー21', lat: 35.6756, lon: 139.7605, height: 100, floors: 21, use: '事務所', year: 1995, w: 0.0005, h: 0.0005 },
  { name: '明治安田生命ビル', lat: 35.6778, lon: 139.7604, height: 30, floors: 8, use: '事務所', year: 1934, w: 0.0005, h: 0.0004 },
  { name: '東京海上日動ビル', lat: 35.6822, lon: 139.7622, height: 128, floors: 25, use: '事務所', year: 1974, w: 0.0005, h: 0.0005 },
  { name: '東京商工会議所ビル', lat: 35.6802, lon: 139.7607, height: 75, floors: 15, use: '事務所', year: 2015, w: 0.0004, h: 0.0004 },
  { name: '三井住友銀行本店', lat: 35.6849, lon: 139.7647, height: 78, floors: 18, use: '銀行', year: 2010, w: 0.0005, h: 0.0005 },
  // Otemachi area
  { name: '大手町フィナンシャルシティ', lat: 35.6876, lon: 139.7649, height: 170, floors: 35, use: '事務所', year: 2012, w: 0.0007, h: 0.0005 },
  { name: '経団連会館', lat: 35.6883, lon: 139.7636, height: 120, floors: 23, use: '事務所', year: 2009, w: 0.0004, h: 0.0004 },
  { name: 'NTT大手町ビル', lat: 35.6878, lon: 139.7621, height: 80, floors: 18, use: '通信施設', year: 1958, w: 0.0005, h: 0.0005 },
  { name: '大手町タワー', lat: 35.6865, lon: 139.7612, height: 200, floors: 38, use: '複合施設', year: 2014, w: 0.0005, h: 0.0005 },
  { name: '大手町パークビル', lat: 35.6868, lon: 139.7637, height: 150, floors: 29, use: '事務所', year: 2017, w: 0.0005, h: 0.0005 },
  { name: '大手町プレイス', lat: 35.6886, lon: 139.7668, height: 180, floors: 35, use: '事務所', year: 2018, w: 0.0006, h: 0.0005 },
  { name: 'Otemachi Oneタワー', lat: 35.6892, lon: 139.7650, height: 160, floors: 40, use: '事務所', year: 2020, w: 0.0005, h: 0.0005 },
  // Yurakucho / Hibiya area
  { name: '東京国際フォーラム', lat: 35.6766, lon: 139.7639, height: 60, floors: 11, use: 'ホール', year: 1997, w: 0.0008, h: 0.0004 },
  { name: '帝国ホテル', lat: 35.6733, lon: 139.7585, height: 125, floors: 31, use: 'ホテル', year: 1970, w: 0.0005, h: 0.0006 },
  { name: '日比谷ミッドタウン', lat: 35.6735, lon: 139.7570, height: 192, floors: 35, use: '複合施設', year: 2018, w: 0.0005, h: 0.0005 },
  { name: '日生劇場ビル', lat: 35.6740, lon: 139.7557, height: 55, floors: 10, use: '劇場', year: 1963, w: 0.0004, h: 0.0004 },
  { name: '第一生命日比谷ファースト', lat: 35.6745, lon: 139.7543, height: 155, floors: 31, use: '事務所', year: 2018, w: 0.0005, h: 0.0005 },
  { name: '有楽町イトシア', lat: 35.6748, lon: 139.7630, height: 100, floors: 21, use: '商業施設', year: 2007, w: 0.0004, h: 0.0004 },
  { name: '有楽町マリオン', lat: 35.6740, lon: 139.7637, height: 120, floors: 22, use: '商業施設', year: 1984, w: 0.0005, h: 0.0003 },
  // Nihonbashi area
  { name: '日本橋三越本店', lat: 35.6862, lon: 139.7742, height: 50, floors: 10, use: '商業施設', year: 1935, w: 0.0006, h: 0.0004 },
  { name: '日本橋高島屋', lat: 35.6818, lon: 139.7748, height: 60, floors: 12, use: '商業施設', year: 1933, w: 0.0005, h: 0.0005 },
  { name: '日本橋室町三井タワー', lat: 35.6870, lon: 139.7730, height: 170, floors: 31, use: '複合施設', year: 2019, w: 0.0005, h: 0.0005 },
  { name: 'COREDO室町1', lat: 35.6858, lon: 139.7735, height: 117, floors: 22, use: '商業施設', year: 2010, w: 0.0004, h: 0.0004 },
  { name: 'COREDO日本橋', lat: 35.6822, lon: 139.7740, height: 120, floors: 20, use: '商業施設', year: 2004, w: 0.0004, h: 0.0004 },
  { name: '日本銀行本店', lat: 35.6856, lon: 139.7703, height: 25, floors: 5, use: '銀行', year: 1896, w: 0.0007, h: 0.0006 },
  { name: '三井本館', lat: 35.6862, lon: 139.7725, height: 32, floors: 7, use: '事務所', year: 1929, w: 0.0005, h: 0.0004 },
  { name: '野村ビル', lat: 35.6855, lon: 139.7708, height: 170, floors: 30, use: '事務所', year: 2014, w: 0.0005, h: 0.0004 },
  { name: '常盤橋タワー', lat: 35.6855, lon: 139.7690, height: 212, floors: 40, use: '事務所', year: 2021, w: 0.0005, h: 0.0005 },
  { name: 'Torch Tower (建設中)', lat: 35.6841, lon: 139.7696, height: 390, floors: 63, use: '複合施設', year: 2028, w: 0.0005, h: 0.0005 },
];

function buildPolygon(lat, lon, w, h) {
  const hw = w / 2;
  const hh = h / 2;
  return [
    [lon - hw, lat - hh],
    [lon + hw, lat - hh],
    [lon + hw, lat + hh],
    [lon - hw, lat + hh],
    [lon - hw, lat - hh],
  ];
}

function generateSeedData() {
  return BUILDINGS.map((b, i) => ({
    type: 'Feature',
    geometry: {
      type: 'Polygon',
      coordinates: [buildPolygon(b.lat, b.lon, b.w, b.h)],
    },
    properties: {
      building_id: `PLT_${String(i + 1).padStart(3, '0')}`,
      name: b.name,
      height_m: b.height,
      floors: b.floors,
      building_use: b.use,
      year_built: b.year,
      area_sqm: Math.round(b.w * 111320 * b.h * 110540),
      source: 'plateau_seed',
    },
  }));
}

export default async function collectPlateauBuildings() {
  // PLATEAU data is provided as seed/demo data
  // Full integration would use CityGML from https://www.geospatial.jp/ckan/dataset/plateau
  const features = generateSeedData();

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'plateau_seed',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Building footprints from PLATEAU 3D city model - Marunouchi/Tokyo Station area',
    },
    metadata: {},
  };
}
