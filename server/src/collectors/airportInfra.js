/**
 * Airport Infrastructure Collector
 * Maps aviation infrastructure across Japan:
 * - Aerodromes (civil and military)
 * - Runways with length/surface data
 * - Navigation aids (ILS, VOR, NDB)
 * - Terminals and control towers
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryOSMAirportInfra() {
  return fetchOverpass(
    'node["aeroway"="aerodrome"](area.jp);way["aeroway"="aerodrome"](area.jp);node["aeroway"="navigationaid"](area.jp);node["man_made"="tower"]["tower:type"="aircraft_control"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        facility_id: `OSM_AIR_${el.id}`,
        name: el.tags?.name || el.tags?.['name:en'] || `Airport facility ${i + 1}`,
        icao: el.tags?.icao || null,
        iata: el.tags?.iata || null,
        facility_type: el.tags?.['man_made'] === 'tower' ? 'control_tower'
          : el.tags?.aeroway === 'navigationaid' ? 'navaid'
          : 'aerodrome',
        aeroway: el.tags?.aeroway || null,
        elevation_ft: el.tags?.ele ? parseFloat(el.tags.ele) : null,
        runway_length_m: el.tags?.['runway:length'] ? parseFloat(el.tags['runway:length']) : null,
        operator: el.tags?.operator || 'unknown',
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

const AIRPORT_FACILITIES = [
  // === Major International Airports ===
  { name: '成田国際空港', name_en: 'Narita International Airport', icao: 'RJAA', iata: 'NRT', facility_type: 'aerodrome', lat: 35.7647, lon: 140.3864, elevation_ft: 141, runway_count: 2, runway_length_m: 4000, surface: 'asphalt', operator: 'Narita International Airport Corporation' },
  { name: '東京国際空港 (羽田)', name_en: 'Tokyo Haneda International Airport', icao: 'RJTT', iata: 'HND', facility_type: 'aerodrome', lat: 35.5494, lon: 139.7798, elevation_ft: 35, runway_count: 4, runway_length_m: 3360, surface: 'asphalt', operator: 'Japan Airport Terminal Co.' },
  { name: '関西国際空港', name_en: 'Kansai International Airport', icao: 'RJBB', iata: 'KIX', facility_type: 'aerodrome', lat: 34.4347, lon: 135.2441, elevation_ft: 26, runway_count: 2, runway_length_m: 4000, surface: 'asphalt', operator: 'Kansai Airports' },
  { name: '大阪国際空港 (伊丹)', name_en: 'Osaka Itami Airport', icao: 'RJOO', iata: 'ITM', facility_type: 'aerodrome', lat: 34.7855, lon: 135.4380, elevation_ft: 39, runway_count: 2, runway_length_m: 3000, surface: 'asphalt', operator: 'Kansai Airports' },
  { name: '中部国際空港 (セントレア)', name_en: 'Chubu Centrair International Airport', icao: 'RJGG', iata: 'NGO', facility_type: 'aerodrome', lat: 34.8584, lon: 136.8125, elevation_ft: 15, runway_count: 1, runway_length_m: 3500, surface: 'asphalt', operator: 'Central Japan International Airport Co.' },

  // === Major Domestic Airports ===
  { name: '福岡空港', name_en: 'Fukuoka Airport', icao: 'RJFF', iata: 'FUK', facility_type: 'aerodrome', lat: 33.5859, lon: 130.4508, elevation_ft: 30, runway_count: 1, runway_length_m: 2800, surface: 'asphalt', operator: 'Fukuoka Airport Holdings' },
  { name: '新千歳空港', name_en: 'New Chitose Airport', icao: 'RJCC', iata: 'CTS', facility_type: 'aerodrome', lat: 42.7752, lon: 141.6922, elevation_ft: 82, runway_count: 2, runway_length_m: 3000, surface: 'asphalt', operator: 'Hokkaido Airport Co.' },
  { name: '那覇空港', name_en: 'Naha Airport', icao: 'ROAH', iata: 'OKA', facility_type: 'aerodrome', lat: 26.1958, lon: 127.6459, elevation_ft: 12, runway_count: 1, runway_length_m: 3000, surface: 'asphalt', operator: 'MLIT' },
  { name: '仙台空港', name_en: 'Sendai Airport', icao: 'RJSS', iata: 'SDJ', facility_type: 'aerodrome', lat: 38.1397, lon: 140.9170, elevation_ft: 15, runway_count: 1, runway_length_m: 3000, surface: 'asphalt', operator: 'Sendai International Airport Co.' },
  { name: '広島空港', name_en: 'Hiroshima Airport', icao: 'RJOA', iata: 'HIJ', facility_type: 'aerodrome', lat: 34.4361, lon: 132.9194, elevation_ft: 1088, runway_count: 1, runway_length_m: 3000, surface: 'asphalt', operator: 'MLIT' },
  { name: '熊本空港', name_en: 'Kumamoto Airport', icao: 'RJFT', iata: 'KMJ', facility_type: 'aerodrome', lat: 32.8373, lon: 130.8551, elevation_ft: 642, runway_count: 1, runway_length_m: 3000, surface: 'asphalt', operator: 'MLIT' },
  { name: '鹿児島空港', name_en: 'Kagoshima Airport', icao: 'RJFK', iata: 'KOJ', facility_type: 'aerodrome', lat: 31.8034, lon: 130.7195, elevation_ft: 906, runway_count: 1, runway_length_m: 3000, surface: 'asphalt', operator: 'MLIT' },
  { name: '松山空港', name_en: 'Matsuyama Airport', icao: 'RJOM', iata: 'MYJ', facility_type: 'aerodrome', lat: 33.8272, lon: 132.6997, elevation_ft: 25, runway_count: 1, runway_length_m: 2500, surface: 'asphalt', operator: 'MLIT' },
  { name: '高松空港', name_en: 'Takamatsu Airport', icao: 'RJOT', iata: 'TAK', facility_type: 'aerodrome', lat: 34.2142, lon: 134.0156, elevation_ft: 607, runway_count: 1, runway_length_m: 2500, surface: 'asphalt', operator: 'MLIT' },
  { name: '大分空港', name_en: 'Oita Airport', icao: 'RJFO', iata: 'OIT', facility_type: 'aerodrome', lat: 33.4794, lon: 131.7372, elevation_ft: 19, runway_count: 1, runway_length_m: 3000, surface: 'asphalt', operator: 'MLIT' },
  { name: '長崎空港', name_en: 'Nagasaki Airport', icao: 'RJFU', iata: 'NGS', facility_type: 'aerodrome', lat: 32.9169, lon: 129.9136, elevation_ft: 15, runway_count: 1, runway_length_m: 3000, surface: 'asphalt', operator: 'MLIT' },
  { name: '宮崎空港', name_en: 'Miyazaki Airport', icao: 'RJFM', iata: 'KMI', facility_type: 'aerodrome', lat: 31.8772, lon: 131.4494, elevation_ft: 20, runway_count: 1, runway_length_m: 2500, surface: 'asphalt', operator: 'MLIT' },
  { name: '青森空港', name_en: 'Aomori Airport', icao: 'RJSA', iata: 'AOJ', facility_type: 'aerodrome', lat: 40.7347, lon: 140.6908, elevation_ft: 664, runway_count: 1, runway_length_m: 3000, surface: 'asphalt', operator: 'Aomori Prefecture' },
  { name: '旭川空港', name_en: 'Asahikawa Airport', icao: 'RJEC', iata: 'AKJ', facility_type: 'aerodrome', lat: 43.6708, lon: 142.4475, elevation_ft: 721, runway_count: 1, runway_length_m: 2500, surface: 'asphalt', operator: 'Asahikawa City' },
  { name: '北九州空港', name_en: 'Kitakyushu Airport', icao: 'RJFR', iata: 'KKJ', facility_type: 'aerodrome', lat: 33.8459, lon: 131.0348, elevation_ft: 21, runway_count: 1, runway_length_m: 2500, surface: 'asphalt', operator: 'MLIT' },
  { name: '新石垣空港', name_en: 'New Ishigaki Airport', icao: 'ROIG', iata: 'ISG', facility_type: 'aerodrome', lat: 24.3964, lon: 124.2450, elevation_ft: 102, runway_count: 1, runway_length_m: 2000, surface: 'asphalt', operator: 'MLIT' },
  { name: '宮古空港', name_en: 'Miyako Airport', icao: 'ROMY', iata: 'MMY', facility_type: 'aerodrome', lat: 24.7828, lon: 125.2950, elevation_ft: 150, runway_count: 1, runway_length_m: 2000, surface: 'asphalt', operator: 'MLIT' },
  { name: '札幌丘珠空港', name_en: 'Sapporo Okadama Airport', icao: 'RJCO', iata: 'OKD', facility_type: 'aerodrome', lat: 43.1161, lon: 141.3814, elevation_ft: 26, runway_count: 1, runway_length_m: 1500, surface: 'asphalt', operator: 'MLIT' },
  { name: '富山空港', name_en: 'Toyama Airport', icao: 'RJNT', iata: 'TOY', facility_type: 'aerodrome', lat: 36.6483, lon: 137.1875, elevation_ft: 95, runway_count: 1, runway_length_m: 2000, surface: 'asphalt', operator: 'Toyama Prefecture' },
  { name: '小松飛行場', name_en: 'Komatsu Airport', icao: 'RJNK', iata: 'KMQ', facility_type: 'aerodrome', lat: 36.3946, lon: 136.4068, elevation_ft: 36, runway_count: 1, runway_length_m: 2700, surface: 'concrete', operator: 'JASDF / Civil' },

  // === Military / JSDF Bases ===
  { name: '横田飛行場', name_en: 'Yokota Air Base', icao: 'RJTY', iata: null, facility_type: 'military_base', lat: 35.7485, lon: 139.3484, elevation_ft: 463, runway_count: 1, runway_length_m: 3353, surface: 'concrete', operator: 'USAF' },
  { name: '三沢飛行場', name_en: 'Misawa Air Base', icao: 'RJSM', iata: null, facility_type: 'military_base', lat: 40.7032, lon: 141.3686, elevation_ft: 119, runway_count: 1, runway_length_m: 3050, surface: 'concrete', operator: 'USAF / JASDF' },
  { name: '百里飛行場', name_en: 'Hyakuri Air Base', icao: 'RJAH', iata: null, facility_type: 'military_base', lat: 36.1811, lon: 140.4147, elevation_ft: 105, runway_count: 1, runway_length_m: 2700, surface: 'concrete', operator: 'JASDF' },
  { name: '新田原基地', name_en: 'Nyutabaru Air Base', icao: 'RJFN', iata: null, facility_type: 'military_base', lat: 32.0836, lon: 131.4511, elevation_ft: 250, runway_count: 1, runway_length_m: 2700, surface: 'concrete', operator: 'JASDF' },
  { name: '千歳基地', name_en: 'Chitose Air Base', icao: 'RJCJ', iata: null, facility_type: 'military_base', lat: 42.7945, lon: 141.6664, elevation_ft: 87, runway_count: 1, runway_length_m: 3000, surface: 'concrete', operator: 'JASDF' },
  { name: '岐阜基地', name_en: 'Gifu Air Base', icao: 'RJNG', iata: null, facility_type: 'military_base', lat: 35.3942, lon: 136.8706, elevation_ft: 128, runway_count: 1, runway_length_m: 2700, surface: 'concrete', operator: 'JASDF' },
  { name: '那覇基地 (軍用区画)', name_en: 'Naha Air Base (Military Section)', icao: 'ROAH_MIL', iata: null, facility_type: 'military_base', lat: 26.1990, lon: 127.6480, elevation_ft: 12, runway_count: 1, runway_length_m: 3000, surface: 'concrete', operator: 'JASDF' },
  { name: '岩国飛行場', name_en: 'MCAS Iwakuni', icao: 'RJOI', iata: null, facility_type: 'military_base', lat: 34.1464, lon: 132.2361, elevation_ft: 7, runway_count: 1, runway_length_m: 2440, surface: 'concrete', operator: 'USMC' },

  // === Navigation Aids ===
  { name: '東京VOR/DME (TLE)', name_en: 'Tokyo VOR/DME', icao: null, iata: null, facility_type: 'navaid', lat: 35.68, lon: 139.77, elevation_ft: null, runway_count: null, runway_length_m: null, surface: null, operator: 'MLIT', navaid_type: 'VOR/DME' },
  { name: '成田ILS RWY34L', name_en: 'Narita ILS RWY34L', icao: null, iata: null, facility_type: 'navaid', lat: 35.7750, lon: 140.3900, elevation_ft: null, runway_count: null, runway_length_m: null, surface: null, operator: 'MLIT', navaid_type: 'ILS' },
  { name: '羽田ILS RWY34R', name_en: 'Haneda ILS RWY34R', icao: null, iata: null, facility_type: 'navaid', lat: 35.5550, lon: 139.7850, elevation_ft: null, runway_count: null, runway_length_m: null, surface: null, operator: 'MLIT', navaid_type: 'ILS' },
  { name: '関西VOR/DME', name_en: 'Kansai VOR/DME', icao: null, iata: null, facility_type: 'navaid', lat: 34.4400, lon: 135.2500, elevation_ft: null, runway_count: null, runway_length_m: null, surface: null, operator: 'MLIT', navaid_type: 'VOR/DME' },
  { name: '中部VOR/DME', name_en: 'Chubu VOR/DME', icao: null, iata: null, facility_type: 'navaid', lat: 34.8600, lon: 136.8200, elevation_ft: null, runway_count: null, runway_length_m: null, surface: null, operator: 'MLIT', navaid_type: 'VOR/DME' },
  { name: '福岡VOR/DME', name_en: 'Fukuoka VOR/DME', icao: null, iata: null, facility_type: 'navaid', lat: 33.5900, lon: 130.4500, elevation_ft: null, runway_count: null, runway_length_m: null, surface: null, operator: 'MLIT', navaid_type: 'VOR/DME' },
  { name: '千歳VOR/DME', name_en: 'Chitose VOR/DME', icao: null, iata: null, facility_type: 'navaid', lat: 42.7800, lon: 141.6900, elevation_ft: null, runway_count: null, runway_length_m: null, surface: null, operator: 'MLIT', navaid_type: 'VOR/DME' },
  { name: '那覇VOR/DME', name_en: 'Naha VOR/DME', icao: null, iata: null, facility_type: 'navaid', lat: 26.2000, lon: 127.6500, elevation_ft: null, runway_count: null, runway_length_m: null, surface: null, operator: 'MLIT', navaid_type: 'VOR/DME' },
  { name: '仙台NDB', name_en: 'Sendai NDB', icao: null, iata: null, facility_type: 'navaid', lat: 38.1400, lon: 140.9200, elevation_ft: null, runway_count: null, runway_length_m: null, surface: null, operator: 'MLIT', navaid_type: 'NDB' },
  { name: '広島NDB', name_en: 'Hiroshima NDB', icao: null, iata: null, facility_type: 'navaid', lat: 34.4400, lon: 132.9200, elevation_ft: null, runway_count: null, runway_length_m: null, surface: null, operator: 'MLIT', navaid_type: 'NDB' },
  { name: '鹿児島NDB', name_en: 'Kagoshima NDB', icao: null, iata: null, facility_type: 'navaid', lat: 31.8000, lon: 130.7200, elevation_ft: null, runway_count: null, runway_length_m: null, surface: null, operator: 'MLIT', navaid_type: 'NDB' },
  { name: '熊本NDB', name_en: 'Kumamoto NDB', icao: null, iata: null, facility_type: 'navaid', lat: 32.8400, lon: 130.8600, elevation_ft: null, runway_count: null, runway_length_m: null, surface: null, operator: 'MLIT', navaid_type: 'NDB' },
];

function generateSeedData() {
  const now = new Date();
  return AIRPORT_FACILITIES.map((a, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [a.lon, a.lat] },
    properties: {
      facility_id: `AIR_${String(i + 1).padStart(4, '0')}`,
      name: `${a.name} (${a.name_en})`,
      icao: a.icao,
      iata: a.iata,
      facility_type: a.facility_type,
      operator: a.operator,
      elevation_ft: a.elevation_ft,
      runway_count: a.runway_count,
      runway_length_m: a.runway_length_m,
      surface: a.surface,
      navaid_type: a.navaid_type || null,
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'airport_infra_seed',
    },
  }));
}

export default async function collectAirportInfra() {
  let features = await tryOSMAirportInfra();
  const live = !!(features && features.length > 0);

  if (live) {
    // Merge: add seed entries whose ICAO is not already present in live data
    const liveIcao = new Set(features.map(f => f.properties.icao).filter(Boolean));
    const seed = generateSeedData();
    for (const s of seed) {
      if (!s.properties.icao || !liveIcao.has(s.properties.icao)) {
        features.push(s);
      }
    }
  } else {
    features = generateSeedData();
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'airport_infra',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Japan airport infrastructure - aerodromes, military bases, navigation aids, control towers',
    },
    metadata: {},
  };
}
