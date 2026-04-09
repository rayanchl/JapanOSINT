/**
 * Satellite Ground Stations Collector
 * JAXA + KDDI + commercial satellite ground stations and tracking sites in Japan.
 */

const SEED_GS = [
  // JAXA primary
  { name: '内之浦宇宙空間観測所 USC', lat: 31.2519, lon: 131.0817, operator: 'JAXA', kind: 'launch_tracking', bands: 'S,X' },
  { name: '種子島宇宙センター TNSC', lat: 30.4000, lon: 130.9700, operator: 'JAXA', kind: 'launch', bands: 'S,X,C' },
  { name: 'JAXA 筑波宇宙センター', lat: 36.0660, lon: 140.1280, operator: 'JAXA', kind: 'mission_control', bands: 'S,X,Ka' },
  { name: 'JAXA 勝浦宇宙通信所', lat: 35.1844, lon: 140.2683, operator: 'JAXA', kind: 'tt&c', bands: 'S,Ka' },
  { name: 'JAXA 沖縄宇宙通信所', lat: 26.3686, lon: 127.7822, operator: 'JAXA', kind: 'tt&c', bands: 'S,Ka' },
  { name: 'JAXA 増田宇宙通信所', lat: 30.5208, lon: 130.9786, operator: 'JAXA', kind: 'tt&c', bands: 'S' },
  { name: 'JAXA 美笹深宇宙探査用地上局', lat: 36.1306, lon: 138.3717, operator: 'JAXA', kind: 'deep_space', bands: 'X,Ka' },
  { name: 'JAXA 臼田宇宙空間観測所', lat: 36.1322, lon: 138.3631, operator: 'JAXA', kind: 'deep_space', bands: 'X' },
  { name: 'JAXA 北海道広尾追跡所', lat: 42.2950, lon: 143.3083, operator: 'JAXA', kind: 'tracking', bands: 'S' },
  { name: 'JAXA 鹿児島宇宙センター', lat: 31.2522, lon: 131.0822, operator: 'JAXA', kind: 'tracking', bands: 'S,X' },
  { name: 'JAXA 父島追跡所', lat: 27.0833, lon: 142.1833, operator: 'JAXA', kind: 'tracking', bands: 'S' },
  { name: 'JAXA 佐渡追跡所', lat: 38.0500, lon: 138.4000, operator: 'JAXA', kind: 'tracking', bands: 'S' },
  // NICT
  { name: 'NICT 鹿島宇宙技術センター', lat: 35.9536, lon: 140.6597, operator: 'NICT', kind: 'vlbi', bands: 'X,K,Q' },
  { name: 'NICT 小金井本部', lat: 35.7100, lon: 139.4900, operator: 'NICT', kind: 'satellite_research', bands: 'multi' },
  // KDDI commercial
  { name: 'KDDI 山口衛星通信所', lat: 34.0567, lon: 131.5544, operator: 'KDDI', kind: 'commercial_satcom', bands: 'C,Ku,Ka' },
  { name: 'KDDI 茨城衛星通信所', lat: 36.2014, lon: 140.6322, operator: 'KDDI', kind: 'commercial_satcom', bands: 'C,Ku,Ka' },
  // SKY Perfect JSAT
  { name: 'SKY Perfect JSAT 横浜衛星管制センター', lat: 35.4500, lon: 139.6500, operator: 'SKY Perfect JSAT', kind: 'satcom', bands: 'C,Ku,Ka' },
  { name: 'SKY Perfect JSAT 茨城衛星管制センター', lat: 36.1922, lon: 140.6231, operator: 'SKY Perfect JSAT', kind: 'satcom', bands: 'C,Ku,Ka' },
  { name: 'SKY Perfect JSAT 群馬衛星管制センター', lat: 36.3500, lon: 138.9500, operator: 'SKY Perfect JSAT', kind: 'satcom', bands: 'C,Ku,Ka' },
  { name: 'SKY Perfect JSAT 山口衛星管制センター', lat: 34.0667, lon: 131.5667, operator: 'SKY Perfect JSAT', kind: 'satcom', bands: 'C,Ku,Ka' },
  // National Astronomical Observatory
  { name: '国立天文台 野辺山宇宙電波観測所', lat: 35.9419, lon: 138.4761, operator: 'NAOJ', kind: 'radio_observatory', bands: 'mm' },
  { name: '国立天文台 水沢VLBI観測所', lat: 39.1339, lon: 141.1325, operator: 'NAOJ', kind: 'vlbi', bands: 'mm,S,X' },
  { name: '国立天文台 入来VLBI観測局', lat: 31.7500, lon: 130.4500, operator: 'NAOJ', kind: 'vlbi', bands: 'S,X' },
  { name: '国立天文台 小笠原VLBI観測局', lat: 27.0900, lon: 142.2167, operator: 'NAOJ', kind: 'vlbi', bands: 'S,X' },
  { name: '国立天文台 石垣島VLBI観測局', lat: 24.4122, lon: 124.1700, operator: 'NAOJ', kind: 'vlbi', bands: 'S,X' },
  // SoftBank / OneWeb gateway
  { name: 'SoftBank 茨城ゲートウェイ (OneWeb)', lat: 36.2050, lon: 140.6300, operator: 'SoftBank', kind: 'leo_gateway', bands: 'Ka' },
  { name: 'KDDI 山口ゲートウェイ (Starlink)', lat: 34.0500, lon: 131.5500, operator: 'KDDI', kind: 'leo_gateway', bands: 'Ka' },
  // Mitsubishi Electric
  { name: '三菱電機 鎌倉製作所 衛星通信', lat: 35.3236, lon: 139.5497, operator: 'Mitsubishi Electric', kind: 'manufacture_test', bands: 'multi' },
];

function generateSeedData() {
  return SEED_GS.map((s, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [s.lon, s.lat] },
    properties: {
      gs_id: `GS_${String(i + 1).padStart(5, '0')}`,
      name: s.name,
      operator: s.operator,
      kind: s.kind,
      bands: s.bands,
      country: 'JP',
      source: 'satellite_gs_seed',
    },
  }));
}

export default async function collectSatelliteGroundStations() {
  const features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'satellite_ground_stations',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live: false,
      description: 'JAXA, NICT, KDDI, SKY Perfect JSAT, NAOJ ground stations and tracking facilities',
    },
    metadata: {},
  };
}
