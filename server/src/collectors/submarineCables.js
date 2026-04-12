/**
 * Submarine Cables Collector
 * Cable landing stations in Japan, with seed of major trans-Pacific +
 * intra-Asia cables landing on Japanese coasts.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return await fetchOverpass(
    'node["telecom"="connection_point"](area.jp);way["submarine"="yes"](area.jp);node["communication:submarine_cable"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        cable_id: `CBL_LIVE_${String(i + 1).padStart(5, '0')}`,
        name: el.tags?.name || el.tags?.['name:en'] || `Cable landing ${el.id}`,
        operator: el.tags?.operator || null,
        region: el.tags?.['addr:state'] || null,
        endpoints: el.tags?.['communication:submarine_cable'] || 'multi',
        country: 'JP',
        source: 'submarine_cables_live',
      },
    })
  );
}

const SEED_CABLES = [
  // Major Trans-Pacific landings
  { name: 'JUPITER (Maruyama landing)', lat: 35.0033, lon: 139.8800, operator: 'Consortium', region: 'Chiba/Maruyama', endpoints: 'JP-US' },
  { name: 'FASTER (Chikura landing)', lat: 34.9700, lon: 139.9500, operator: 'Google/KDDI', region: 'Chiba/Chikura', endpoints: 'JP-US' },
  { name: 'PLCN (Toyohashi)', lat: 34.7669, lon: 137.3919, operator: 'Google/Facebook', region: 'Aichi/Toyohashi', endpoints: 'JP-US' },
  { name: 'New Cross Pacific (NCP) Maruyama', lat: 35.0050, lon: 139.8825, operator: 'Microsoft/SoftBank', region: 'Chiba/Maruyama', endpoints: 'JP-US' },
  { name: 'Unity Cable (Chikura)', lat: 34.9750, lon: 139.9550, operator: 'Google', region: 'Chiba/Chikura', endpoints: 'JP-US' },
  { name: 'Trans-Pacific Express (Maruyama)', lat: 35.0044, lon: 139.8833, operator: 'NTT/KDDI', region: 'Chiba', endpoints: 'JP-CN-US' },
  { name: 'PC-1 East (Ajigaura)', lat: 36.4128, lon: 140.6364, operator: 'NTT/KDDI', region: 'Ibaraki', endpoints: 'JP-US' },
  { name: 'PC-1 West (Ajigaura)', lat: 36.4150, lon: 140.6400, operator: 'NTT/KDDI', region: 'Ibaraki', endpoints: 'JP-US' },
  { name: 'Tata TGN-Pacific (Toyohashi)', lat: 34.7700, lon: 137.3950, operator: 'Tata', region: 'Aichi', endpoints: 'JP-US' },
  // Intra-Asia
  { name: 'APCN-2 (Chikura)', lat: 34.9744, lon: 139.9489, operator: 'Consortium', region: 'Chiba', endpoints: 'JP-KR-CN-TW-PH-SG' },
  { name: 'EAC-C2C (Chikura)', lat: 34.9750, lon: 139.9500, operator: 'Telstra', region: 'Chiba', endpoints: 'JP-KR-CN-PH-TW-SG' },
  { name: 'SJC (Chikura)', lat: 34.9747, lon: 139.9494, operator: 'Consortium', region: 'Chiba', endpoints: 'JP-CN-PH-SG' },
  { name: 'SJC2 (Maruyama)', lat: 35.0033, lon: 139.8794, operator: 'Consortium', region: 'Chiba', endpoints: 'JP-KR-CN-TW-VN-SG' },
  { name: 'APG (Toyohashi)', lat: 34.7689, lon: 137.3936, operator: 'Consortium', region: 'Aichi', endpoints: 'JP-KR-CN-TW-VN-MY-SG-TH' },
  { name: 'ADC (Maruyama)', lat: 35.0028, lon: 139.8783, operator: 'Consortium', region: 'Chiba', endpoints: 'JP-CN-PH-SG-TH' },
  { name: 'JIH (Japan Information Highway, Naha)', lat: 26.2125, lon: 127.6800, operator: 'NTT', region: 'Okinawa/Naha', endpoints: 'JP-domestic' },
  // Russia + N Asia
  { name: 'RJCN (Russia-Japan Cable, Naoetsu)', lat: 37.1781, lon: 138.2528, operator: 'Rostelecom/NTT', region: 'Niigata/Joetsu', endpoints: 'JP-RU' },
  { name: 'JIH Sea of Japan, Wajima', lat: 37.4225, lon: 136.8975, operator: 'NTT', region: 'Ishikawa/Wajima', endpoints: 'JP-domestic' },
  // Korea
  { name: 'KJCN (Korea-Japan, Karatsu)', lat: 33.4500, lon: 129.9750, operator: 'NTT/KT', region: 'Saga/Karatsu', endpoints: 'JP-KR' },
  { name: 'New KJCN (Karatsu)', lat: 33.4525, lon: 129.9775, operator: 'NTT/KT', region: 'Saga', endpoints: 'JP-KR' },
  // China direct
  { name: 'CUCN (China-US, Okinawa)', lat: 26.2150, lon: 127.6850, operator: 'Consortium', region: 'Okinawa', endpoints: 'JP-CN-US' },
  // Recent / future
  { name: 'JGA-N (Japan-Guam-Australia, Maruyama)', lat: 35.0050, lon: 139.8800, operator: 'AARNet/RTI', region: 'Chiba', endpoints: 'JP-GU-AU' },
  { name: 'JGA-S (Maruyama)', lat: 35.0053, lon: 139.8806, operator: 'RTI', region: 'Chiba', endpoints: 'JP-AU' },
  { name: 'Topaz (Mie/Toba)', lat: 34.4836, lon: 136.8419, operator: 'Google', region: 'Mie/Toba', endpoints: 'JP-CA' },
  { name: 'Echo (Eureka-Toyohashi)', lat: 34.7700, lon: 137.3925, operator: 'Google/Facebook', region: 'Aichi', endpoints: 'JP-US-Indonesia' },
  // Cable landing facilities (facilities, not specific cables)
  { name: 'KDDI Maruyama Cable Landing Station', lat: 35.0033, lon: 139.8806, operator: 'KDDI', region: 'Chiba', endpoints: 'multi' },
  { name: 'KDDI Chikura Cable Landing Station', lat: 34.9747, lon: 139.9492, operator: 'KDDI', region: 'Chiba', endpoints: 'multi' },
  { name: 'NTT Toyohashi Cable Landing Station', lat: 34.7689, lon: 137.3933, operator: 'NTT', region: 'Aichi', endpoints: 'multi' },
  { name: 'NTT Ajigaura Cable Landing Station', lat: 36.4131, lon: 140.6361, operator: 'NTT', region: 'Ibaraki', endpoints: 'multi' },
  { name: 'NTT Shima Cable Landing Station', lat: 34.3361, lon: 136.8442, operator: 'NTT', region: 'Mie/Shima', endpoints: 'multi' },
  { name: 'KDDI Naha Cable Landing Station', lat: 26.2150, lon: 127.6814, operator: 'KDDI', region: 'Okinawa', endpoints: 'multi' },
];

function generateSeedData() {
  return SEED_CABLES.map((c, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [c.lon, c.lat] },
    properties: {
      cable_id: `CBL_${String(i + 1).padStart(5, '0')}`,
      name: c.name,
      operator: c.operator,
      region: c.region,
      endpoints: c.endpoints,
      country: 'JP',
      source: 'submarine_cables_seed',
    },
  }));
}

export default async function collectSubmarineCables() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'submarine_cables',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Submarine cable landing stations in Japan: trans-Pacific (Chiba), Asia (Aichi), Russia (Niigata), Korea (Saga), Okinawa',
    },
    metadata: {},
  };
}
