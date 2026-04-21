/**
 * Data Centers Collector
 * Major Japanese commercial data centers (Equinix, NTT, KDDI, IDC Frontier, etc).
 * OSM Overpass `telecom=data_center` fallback to curated list.
 */

import { fetchOverpass } from './_liveHelpers.js';

const SEED_DC = [
  // Equinix
  { name: 'Equinix TY1 Tokyo', lat: 35.6669, lon: 139.7656, operator: 'Equinix', tier: 3, region: 'Tokyo' },
  { name: 'Equinix TY2 Tokyo', lat: 35.6517, lon: 139.7572, operator: 'Equinix', tier: 3, region: 'Tokyo' },
  { name: 'Equinix TY4 Tokyo', lat: 35.6336, lon: 139.7906, operator: 'Equinix', tier: 4, region: 'Tokyo' },
  { name: 'Equinix TY5 Koto', lat: 35.6700, lon: 139.8175, operator: 'Equinix', tier: 4, region: 'Tokyo' },
  { name: 'Equinix TY6 Tokyo', lat: 35.6611, lon: 139.7892, operator: 'Equinix', tier: 4, region: 'Tokyo' },
  { name: 'Equinix TY8 Tokyo', lat: 35.6731, lon: 139.8175, operator: 'Equinix', tier: 4, region: 'Tokyo' },
  { name: 'Equinix TY11 Tokyo', lat: 35.6700, lon: 139.7900, operator: 'Equinix', tier: 4, region: 'Tokyo' },
  { name: 'Equinix OS1 Osaka', lat: 34.6913, lon: 135.5023, operator: 'Equinix', tier: 4, region: 'Osaka' },
  // NTT Communications / Global Data Centers
  { name: 'NTT Tokyo 1', lat: 35.6925, lon: 139.7000, operator: 'NTT', tier: 4, region: 'Tokyo' },
  { name: 'NTT Tokyo Otemachi', lat: 35.6886, lon: 139.7647, operator: 'NTT', tier: 4, region: 'Tokyo' },
  { name: 'NTT Tokyo Saitama 1', lat: 35.9000, lon: 139.6800, operator: 'NTT', tier: 4, region: 'Saitama' },
  { name: 'NTT Tokyo Saitama 2', lat: 35.9100, lon: 139.6900, operator: 'NTT', tier: 4, region: 'Saitama' },
  { name: 'NTT Tokyo Yokohama 1', lat: 35.4438, lon: 139.6383, operator: 'NTT', tier: 4, region: 'Yokohama' },
  { name: 'NTT Inzai 1', lat: 35.8311, lon: 140.1486, operator: 'NTT', tier: 4, region: 'Chiba' },
  { name: 'NTT Inzai 2', lat: 35.8400, lon: 140.1500, operator: 'NTT', tier: 4, region: 'Chiba' },
  { name: 'NTT Inzai 3', lat: 35.8350, lon: 140.1550, operator: 'NTT', tier: 4, region: 'Chiba' },
  { name: 'NTT Osaka Dojima', lat: 34.6917, lon: 135.4917, operator: 'NTT', tier: 4, region: 'Osaka' },
  { name: 'NTT Osaka Keihanna', lat: 34.7400, lon: 135.7461, operator: 'NTT', tier: 4, region: 'Kyoto' },
  // KDDI / TELEHOUSE
  { name: 'TELEHOUSE TOKYO Tama 3', lat: 35.6378, lon: 139.4250, operator: 'KDDI/Telehouse', tier: 4, region: 'Tokyo' },
  { name: 'TELEHOUSE TOKYO Otemachi', lat: 35.6889, lon: 139.7639, operator: 'KDDI/Telehouse', tier: 4, region: 'Tokyo' },
  { name: 'TELEHOUSE TOKYO Shinjuku', lat: 35.6896, lon: 139.7036, operator: 'KDDI/Telehouse', tier: 3, region: 'Tokyo' },
  { name: 'TELEHOUSE OSAKA 1', lat: 34.6864, lon: 135.5300, operator: 'KDDI/Telehouse', tier: 4, region: 'Osaka' },
  // IDC Frontier (SoftBank)
  { name: 'IDC Frontier Tokyo Nishi 1', lat: 35.7456, lon: 139.6772, operator: 'IDC Frontier', tier: 3, region: 'Tokyo' },
  { name: 'IDC Frontier Tokyo Fuchu', lat: 35.6694, lon: 139.4806, operator: 'IDC Frontier', tier: 3, region: 'Tokyo' },
  { name: 'IDC Frontier Shirakawa 1', lat: 37.1267, lon: 140.2114, operator: 'IDC Frontier', tier: 3, region: 'Fukushima' },
  { name: 'IDC Frontier Kitakyushu 1', lat: 33.8508, lon: 130.8775, operator: 'IDC Frontier', tier: 3, region: 'Kyushu' },
  // SoftBank
  { name: 'SoftBank Hachioji', lat: 35.6681, lon: 139.3411, operator: 'SoftBank', tier: 3, region: 'Tokyo' },
  // Google
  { name: 'Google Inzai DC', lat: 35.8400, lon: 140.1400, operator: 'Google', tier: 4, region: 'Chiba' },
  // AWS
  { name: 'AWS Osaka Local Zone', lat: 34.6900, lon: 135.5000, operator: 'AWS', tier: 4, region: 'Osaka' },
  { name: 'AWS Tokyo AZ1', lat: 35.6800, lon: 139.7700, operator: 'AWS', tier: 4, region: 'Tokyo' },
  { name: 'AWS Tokyo AZ2', lat: 35.7000, lon: 139.7900, operator: 'AWS', tier: 4, region: 'Tokyo' },
  { name: 'AWS Tokyo AZ3', lat: 35.6900, lon: 139.8100, operator: 'AWS', tier: 4, region: 'Tokyo' },
  // Microsoft Azure
  { name: 'Azure Japan East (Tokyo)', lat: 35.6800, lon: 139.7700, operator: 'Microsoft', tier: 4, region: 'Tokyo' },
  { name: 'Azure Japan West (Osaka)', lat: 34.6900, lon: 135.5000, operator: 'Microsoft', tier: 4, region: 'Osaka' },
  // Colt
  { name: 'Colt Tokyo Inzai', lat: 35.8350, lon: 140.1450, operator: 'Colt', tier: 4, region: 'Chiba' },
  { name: 'Colt Tokyo Shiba', lat: 35.6533, lon: 139.7497, operator: 'Colt', tier: 3, region: 'Tokyo' },
  // Digital Realty / MC Digital Realty
  { name: 'MC Digital Realty NRT10', lat: 35.7700, lon: 140.4400, operator: 'MC Digital Realty', tier: 4, region: 'Chiba' },
  { name: 'MC Digital Realty NRT11', lat: 35.7750, lon: 140.4450, operator: 'MC Digital Realty', tier: 4, region: 'Chiba' },
  // Internet Initiative Japan (IIJ)
  { name: 'IIJ Shiroi DC Campus 1', lat: 35.7900, lon: 140.0556, operator: 'IIJ', tier: 4, region: 'Chiba' },
  { name: 'IIJ Matsue Yasugi DC', lat: 35.4283, lon: 133.2456, operator: 'IIJ', tier: 3, region: 'Shimane' },
  // SAKURA Internet
  { name: 'SAKURA Ishikari DC', lat: 43.2406, lon: 141.3081, operator: 'SAKURA', tier: 3, region: 'Hokkaido' },
  { name: 'SAKURA Tokyo DC', lat: 35.6936, lon: 139.7039, operator: 'SAKURA', tier: 3, region: 'Tokyo' },
  { name: 'SAKURA Osaka DC', lat: 34.6900, lon: 135.5000, operator: 'SAKURA', tier: 3, region: 'Osaka' },
  // Other
  { name: '@Tokyo Center for Cloud (CC1)', lat: 35.6700, lon: 139.7600, operator: 'AT Tokyo', tier: 4, region: 'Tokyo' },
  { name: '@Tokyo Center West', lat: 35.6500, lon: 139.7400, operator: 'AT Tokyo', tier: 4, region: 'Tokyo' },
  { name: 'BroadBand Tower Tokyo', lat: 35.6700, lon: 139.7400, operator: 'BB Tower', tier: 3, region: 'Tokyo' },
  { name: 'PIPELINE Tokyo', lat: 35.6633, lon: 139.7647, operator: 'Pipeline', tier: 3, region: 'Tokyo' },
];

async function tryOverpass() {
  return fetchOverpass(
    'node["telecom"="data_center"](area.jp);way["telecom"="data_center"](area.jp);node["building"="data_center"](area.jp);',
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        dc_id: `OSM_${el.id}`,
        name: el.tags?.name || 'Data Center',
        operator: el.tags?.operator || 'unknown',
        source: 'osm_overpass',
      },
    }),
  );
}

function generateSeedData() {
  return SEED_DC.map((d, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [d.lon, d.lat] },
    properties: {
      dc_id: `DC_${String(i + 1).padStart(5, '0')}`,
      name: d.name,
      operator: d.operator,
      tier: d.tier,
      region: d.region,
      country: 'JP',
      source: 'data_centers_seed',
    },
  }));
}

export default async function collectDataCenters() {
  let features = await tryOverpass();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'data_centers',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Major commercial data centers: Equinix, NTT, KDDI/Telehouse, IDC Frontier, IIJ, SAKURA, AWS, Azure, Google, Colt, MC Digital Realty',
    },
    metadata: {},
  };
}
