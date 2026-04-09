/**
 * Gas Network Collector
 * Maps gas infrastructure across Japan:
 * - Major gas companies (Tokyo Gas, Osaka Gas, Toho Gas, Saibu Gas, Hokkaido Gas, etc.)
 * - LNG receiving terminals
 * - Gas plants and storage facilities
 * - Pipeline interconnects
 */

const GAS_FACILITIES = [
  // LNG terminals - Tokyo Gas
  { name: '袖ケ浦工場', operator: '東京ガス', type: 'lng_terminal', lat: 35.4400, lon: 140.0100, capacity_kt: 2660, region: 'Kanto' },
  { name: '根岸工場', operator: '東京ガス', type: 'lng_terminal', lat: 35.4100, lon: 139.6500, capacity_kt: 1180, region: 'Kanto' },
  { name: '日立LNG基地', operator: '東京ガス', type: 'lng_terminal', lat: 36.5000, lon: 140.6500, capacity_kt: 230, region: 'Kanto' },
  { name: '扇島LNG基地', operator: '東京電力/東京ガス', type: 'lng_terminal', lat: 35.4900, lon: 139.7700, capacity_kt: 850, region: 'Kanto' },
  // LNG terminals - JERA / Tokyo Electric
  { name: '富津LNG基地', operator: 'JERA', type: 'lng_terminal', lat: 35.3300, lon: 139.8400, capacity_kt: 2920, region: 'Kanto' },
  { name: '東扇島LNG基地', operator: 'JERA', type: 'lng_terminal', lat: 35.5000, lon: 139.7700, capacity_kt: 540, region: 'Kanto' },
  { name: '鹿島LNG基地', operator: 'JERA', type: 'lng_terminal', lat: 35.9300, lon: 140.6800, capacity_kt: 1080, region: 'Kanto' },
  // LNG terminals - Osaka Gas
  { name: '泉北製造所第一工場', operator: '大阪ガス', type: 'lng_terminal', lat: 34.5300, lon: 135.4200, capacity_kt: 1820, region: 'Kansai' },
  { name: '泉北製造所第二工場', operator: '大阪ガス', type: 'lng_terminal', lat: 34.5400, lon: 135.4300, capacity_kt: 1280, region: 'Kansai' },
  { name: '姫路製造所', operator: '大阪ガス', type: 'lng_terminal', lat: 34.7700, lon: 134.6500, capacity_kt: 520, region: 'Kansai' },
  { name: '姫路LNG基地', operator: '関西電力', type: 'lng_terminal', lat: 34.7900, lon: 134.6400, capacity_kt: 520, region: 'Kansai' },
  // LNG terminals - Toho Gas (Chubu)
  { name: '知多工場', operator: '東邦ガス', type: 'lng_terminal', lat: 34.9500, lon: 136.8400, capacity_kt: 640, region: 'Chubu' },
  { name: '知多緑浜工場', operator: '東邦ガス', type: 'lng_terminal', lat: 34.9300, lon: 136.8200, capacity_kt: 200, region: 'Chubu' },
  { name: '川越LNGセンター', operator: 'JERA', type: 'lng_terminal', lat: 35.0300, lon: 136.6600, capacity_kt: 480, region: 'Chubu' },
  // Other regional LNG terminals
  { name: '新潟東港LNG基地', operator: '日本海エル・エヌ・ジー', type: 'lng_terminal', lat: 38.0500, lon: 139.2400, capacity_kt: 720, region: 'Hokuriku' },
  { name: '上越火力発電所LNG', operator: 'JERA', type: 'lng_terminal', lat: 37.1700, lon: 138.2400, capacity_kt: 360, region: 'Hokuriku' },
  { name: '富山新港', operator: '北陸電力', type: 'lng_terminal', lat: 36.7700, lon: 137.1100, capacity_kt: 144, region: 'Hokuriku' },
  { name: '七尾LNG基地', operator: '北陸電力', type: 'lng_terminal', lat: 37.0300, lon: 136.9700, capacity_kt: 180, region: 'Hokuriku' },
  { name: '仙台港工場', operator: '仙台市ガス局', type: 'lng_terminal', lat: 38.2700, lon: 141.0200, capacity_kt: 80, region: 'Tohoku' },
  { name: '新仙台LNG基地', operator: '東北電力', type: 'lng_terminal', lat: 38.2700, lon: 141.0200, capacity_kt: 320, region: 'Tohoku' },
  { name: '相馬LNG基地', operator: '日本ガス開発', type: 'lng_terminal', lat: 37.8400, lon: 140.9700, capacity_kt: 230, region: 'Tohoku' },
  { name: '苫小牧LNG基地', operator: '北海道ガス', type: 'lng_terminal', lat: 42.6300, lon: 141.6700, capacity_kt: 380, region: 'Hokkaido' },
  { name: '石狩LNG基地', operator: '北海道ガス', type: 'lng_terminal', lat: 43.2200, lon: 141.3400, capacity_kt: 280, region: 'Hokkaido' },
  { name: '室蘭LNG基地', operator: '北海道電力', type: 'lng_terminal', lat: 42.3300, lon: 140.9700, capacity_kt: 180, region: 'Hokkaido' },
  { name: '柳井LNG基地', operator: '中国電力', type: 'lng_terminal', lat: 33.9500, lon: 132.0900, capacity_kt: 480, region: 'Chugoku' },
  { name: '水島LNG基地', operator: '中国電力', type: 'lng_terminal', lat: 34.5200, lon: 133.7500, capacity_kt: 320, region: 'Chugoku' },
  { name: '岡山ガス LNG', operator: '岡山ガス', type: 'lng_terminal', lat: 34.6700, lon: 133.9400, capacity_kt: 90, region: 'Chugoku' },
  { name: '坂出LNG基地', operator: '四国電力', type: 'lng_terminal', lat: 34.3400, lon: 133.8400, capacity_kt: 180, region: 'Shikoku' },
  { name: '大分LNG基地', operator: '九州電力', type: 'lng_terminal', lat: 33.2400, lon: 131.7000, capacity_kt: 460, region: 'Kyushu' },
  { name: '北九州LNG基地', operator: '西部ガス', type: 'lng_terminal', lat: 33.9000, lon: 130.9700, capacity_kt: 480, region: 'Kyushu' },
  { name: '戸畑工場 (西部ガス)', operator: '西部ガス', type: 'lng_terminal', lat: 33.9000, lon: 130.8200, capacity_kt: 240, region: 'Kyushu' },
  { name: 'ひびきLNG基地', operator: '西部ガス', type: 'lng_terminal', lat: 33.9300, lon: 130.7500, capacity_kt: 360, region: 'Kyushu' },
  { name: '長崎工場 (西部ガス)', operator: '西部ガス', type: 'lng_terminal', lat: 32.7500, lon: 129.8800, capacity_kt: 70, region: 'Kyushu' },
  { name: '熊本工場 (西部ガス)', operator: '西部ガス', type: 'lng_terminal', lat: 32.8000, lon: 130.7000, capacity_kt: 60, region: 'Kyushu' },
  { name: '吉の浦LNG基地', operator: '沖縄電力', type: 'lng_terminal', lat: 26.3000, lon: 127.7600, capacity_kt: 280, region: 'Okinawa' },

  // Major distribution centers / city gate stations
  { name: '東京ガス 中央ガバナステーション', operator: '東京ガス', type: 'distribution', lat: 35.6500, lon: 139.7600, capacity_kt: 0, region: 'Kanto' },
  { name: '大阪ガス 西梅田', operator: '大阪ガス', type: 'distribution', lat: 34.7000, lon: 135.4900, capacity_kt: 0, region: 'Kansai' },
  { name: '東邦ガス 名古屋本店', operator: '東邦ガス', type: 'distribution', lat: 35.1700, lon: 136.9000, capacity_kt: 0, region: 'Chubu' },
  { name: '北海道ガス 札幌本社', operator: '北海道ガス', type: 'distribution', lat: 43.0600, lon: 141.3500, capacity_kt: 0, region: 'Hokkaido' },
];

function generateSeedData() {
  const now = new Date();
  return GAS_FACILITIES.map((g, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [g.lon, g.lat] },
    properties: {
      facility_id: `GAS_${String(i + 1).padStart(4, '0')}`,
      name: g.name,
      operator: g.operator,
      facility_type: g.type,
      capacity_kt: g.capacity_kt,
      region: g.region,
      capacity_category: g.capacity_kt > 1000 ? 'mega' : g.capacity_kt > 300 ? 'large' : g.capacity_kt > 100 ? 'medium' : 'small',
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'gas_network',
    },
  }));
}

export default async function collectGasNetwork() {
  const features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'gas_network',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Japan gas network - LNG terminals, distribution stations, regional gas companies',
    },
    metadata: {},
  };
}
