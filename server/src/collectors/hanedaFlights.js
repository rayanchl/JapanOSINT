/**
 * Haneda Airport Flights Collector
 * Fetches arrivals/departures from Haneda Airport using AeroDataBox API.
 * Falls back to seed of representative scheduled flights.
 */

import { fetchJson } from './_liveHelpers.js';

const AERODATABOX_KEY = process.env.AERODATABOX_KEY || '';
const AERODATABOX_URL = 'https://aerodatabox.p.rapidapi.com/flights/airports/icao/RJTT';

const HANEDA_LAT = 35.5494;
const HANEDA_LON = 139.7798;

const SEED_FLIGHTS = [
  // Domestic ANA/JAL trunk routes
  { flight: 'NH011', airline: 'ANA', origin: 'CTS', destination: 'HND', status: 'arrived', terminal: 'T2', type: 'arrival' },
  { flight: 'NH012', airline: 'ANA', origin: 'HND', destination: 'CTS', status: 'departed', terminal: 'T2', type: 'departure' },
  { flight: 'JL501', airline: 'JAL', origin: 'CTS', destination: 'HND', status: 'arrived', terminal: 'T1', type: 'arrival' },
  { flight: 'JL502', airline: 'JAL', origin: 'HND', destination: 'CTS', status: 'departed', terminal: 'T1', type: 'departure' },
  { flight: 'NH021', airline: 'ANA', origin: 'OKA', destination: 'HND', status: 'enroute', terminal: 'T2', type: 'arrival' },
  { flight: 'JL901', airline: 'JAL', origin: 'OKA', destination: 'HND', status: 'enroute', terminal: 'T1', type: 'arrival' },
  { flight: 'NH031', airline: 'ANA', origin: 'ITM', destination: 'HND', status: 'enroute', terminal: 'T2', type: 'arrival' },
  { flight: 'JL104', airline: 'JAL', origin: 'ITM', destination: 'HND', status: 'arrived', terminal: 'T1', type: 'arrival' },
  { flight: 'NH051', airline: 'ANA', origin: 'FUK', destination: 'HND', status: 'enroute', terminal: 'T2', type: 'arrival' },
  { flight: 'JL301', airline: 'JAL', origin: 'FUK', destination: 'HND', status: 'arrived', terminal: 'T1', type: 'arrival' },
  { flight: 'SKY101', airline: 'Skymark', origin: 'HND', destination: 'CTS', status: 'departed', terminal: 'T1', type: 'departure' },
  { flight: 'BC031', airline: 'Skymark', origin: 'HND', destination: 'FUK', status: 'departed', terminal: 'T1', type: 'departure' },

  // International (Haneda T3)
  { flight: 'NH106', airline: 'ANA', origin: 'LAX', destination: 'HND', status: 'enroute', terminal: 'T3', type: 'arrival' },
  { flight: 'NH107', airline: 'ANA', origin: 'HND', destination: 'LAX', status: 'departed', terminal: 'T3', type: 'departure' },
  { flight: 'JL061', airline: 'JAL', origin: 'LAX', destination: 'HND', status: 'enroute', terminal: 'T3', type: 'arrival' },
  { flight: 'JL062', airline: 'JAL', origin: 'HND', destination: 'LAX', status: 'departed', terminal: 'T3', type: 'departure' },
  { flight: 'AA169', airline: 'American', origin: 'LAX', destination: 'HND', status: 'enroute', terminal: 'T3', type: 'arrival' },
  { flight: 'DL167', airline: 'Delta', origin: 'LAX', destination: 'HND', status: 'enroute', terminal: 'T3', type: 'arrival' },
  { flight: 'UA837', airline: 'United', origin: 'SFO', destination: 'HND', status: 'enroute', terminal: 'T3', type: 'arrival' },
  { flight: 'BA007', airline: 'British Airways', origin: 'LHR', destination: 'HND', status: 'enroute', terminal: 'T3', type: 'arrival' },
  { flight: 'LH716', airline: 'Lufthansa', origin: 'FRA', destination: 'HND', status: 'enroute', terminal: 'T3', type: 'arrival' },
  { flight: 'AF292', airline: 'Air France', origin: 'CDG', destination: 'HND', status: 'enroute', terminal: 'T3', type: 'arrival' },
  { flight: 'KL862', airline: 'KLM', origin: 'AMS', destination: 'HND', status: 'enroute', terminal: 'T3', type: 'arrival' },
  { flight: 'EK313', airline: 'Emirates', origin: 'DXB', destination: 'HND', status: 'enroute', terminal: 'T3', type: 'arrival' },
  { flight: 'QR812', airline: 'Qatar Airways', origin: 'DOH', destination: 'HND', status: 'enroute', terminal: 'T3', type: 'arrival' },
  { flight: 'CX520', airline: 'Cathay', origin: 'HKG', destination: 'HND', status: 'enroute', terminal: 'T3', type: 'arrival' },
  { flight: 'SQ634', airline: 'Singapore', origin: 'SIN', destination: 'HND', status: 'enroute', terminal: 'T3', type: 'arrival' },
  { flight: 'KE2701', airline: 'Korean Air', origin: 'GMP', destination: 'HND', status: 'arrived', terminal: 'T3', type: 'arrival' },
  { flight: 'OZ1085', airline: 'Asiana', origin: 'GMP', destination: 'HND', status: 'arrived', terminal: 'T3', type: 'arrival' },
  { flight: 'CA929', airline: 'Air China', origin: 'PEK', destination: 'HND', status: 'arrived', terminal: 'T3', type: 'arrival' },
];

async function tryAeroDataBox() {
  if (!AERODATABOX_KEY) return null;
  try {
    const ctrl = new AbortController();
    const timeout = setTimeout(() => ctrl.abort(), 10000);
    const now = new Date();
    const start = now.toISOString().slice(0, 16);
    const end = new Date(now.getTime() + 11 * 3600 * 1000).toISOString().slice(0, 16);
    const url = `${AERODATABOX_URL}/${start}/${end}?withLeg=true&direction=Both&withCancelled=true&withCargo=false`;
    const res = await fetch(url, {
      signal: ctrl.signal,
      headers: {
        'X-RapidAPI-Key': AERODATABOX_KEY,
        'X-RapidAPI-Host': 'aerodatabox.p.rapidapi.com',
      },
    });
    clearTimeout(timeout);
    if (!res.ok) return null;
    const data = await res.json();
    const all = [...(data.arrivals || []), ...(data.departures || [])];
    if (all.length === 0) return null;
    return all.slice(0, 200).map((f, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [HANEDA_LON, HANEDA_LAT] },
      properties: {
        flight_id: `HND_${String(i + 1).padStart(5, '0')}`,
        flight_number: f.number || null,
        airline: f.airline?.name || null,
        origin: f.movement?.airport?.iata || null,
        status: f.status || null,
        country: 'JP',
        source: 'aerodatabox_api',
      },
    }));
  } catch {
    return null;
  }
}

function generateSeedData() {
  const now = new Date();
  return SEED_FLIGHTS.map((f, i) => {
    const offset = (i % 10) * 0.005;
    return {
      type: 'Feature',
      geometry: {
        type: 'Point',
        coordinates: [HANEDA_LON + (i % 2 === 0 ? offset : -offset), HANEDA_LAT + (i % 3 === 0 ? offset : -offset)],
      },
      properties: {
        flight_id: `HND_${String(i + 1).padStart(5, '0')}`,
        flight_number: f.flight,
        airline: f.airline,
        origin: f.origin,
        destination: f.destination,
        status: f.status,
        terminal: f.terminal,
        type: f.type,
        country: 'JP',
        observed_at: now.toISOString(),
        source: 'haneda_seed',
      },
    };
  });
}

async function tryOpenSkyArea() {
  // OpenSky Network - free, no key. Bounding box around Haneda.
  const url = 'https://opensky-network.org/api/states/all?lamin=35.4&lomin=139.6&lamax=35.7&lomax=140.0';
  const data = await fetchJson(url, { timeoutMs: 8000 });
  if (!data || !Array.isArray(data.states)) return null;
  return data.states.filter(s => s[5] != null && s[6] != null).map((s, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [s[5], s[6]] },
    properties: {
      flight_id: `HND_OS_${String(i + 1).padStart(5, '0')}`,
      flight_number: (s[1] || '').trim(),
      icao24: s[0],
      origin_country: s[2],
      velocity_ms: s[9],
      altitude_m: s[7],
      heading_deg: s[10],
      on_ground: s[8],
      squawk: s[14],
      last_contact: new Date((s[4] || 0) * 1000).toISOString(),
      country: 'JP',
      airport: 'Haneda (RJTT/HND)',
      source: 'opensky_api',
    },
  }));
}

export default async function collectHanedaFlights() {
  let features = await tryAeroDataBox();
  let liveSource = 'aerodatabox_api';
  if (!features || features.length === 0) {
    features = await tryOpenSkyArea();
    liveSource = 'opensky_api';
  }
  const live = !!(features && features.length > 0);
  if (!live) {
    features = generateSeedData();
    liveSource = 'haneda_seed';
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'haneda_flights',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: liveSource,
      description: 'Tokyo Haneda Airport (RJTT/HND) arrivals/departures - AeroDataBox + OpenSky',
    },
    metadata: {},
  };
}
