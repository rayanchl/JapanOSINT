/**
 * Narita Airport Flights Collector
 * Fetches arrivals/departures from Narita Airport using AeroDataBox API.
 * Falls back to seed of representative scheduled flights.
 */

const AERODATABOX_KEY = process.env.AERODATABOX_KEY || '';
const AERODATABOX_URL = 'https://aerodatabox.p.rapidapi.com/flights/airports/icao/RJAA';

const NARITA_LAT = 35.7720;
const NARITA_LON = 140.3929;

const SEED_FLIGHTS = [
  // Major scheduled airlines at NRT (Narita)
  { flight: 'NH010', airline: 'ANA', origin: 'IAH', destination: 'NRT', status: 'scheduled', terminal: 'T1S', type: 'arrival' },
  { flight: 'NH001', airline: 'ANA', origin: 'NRT', destination: 'IAH', status: 'departed', terminal: 'T1S', type: 'departure' },
  { flight: 'JL005', airline: 'JAL', origin: 'JFK', destination: 'NRT', status: 'arrived', terminal: 'T2', type: 'arrival' },
  { flight: 'JL006', airline: 'JAL', origin: 'NRT', destination: 'JFK', status: 'scheduled', terminal: 'T2', type: 'departure' },
  { flight: 'UA837', airline: 'United', origin: 'SFO', destination: 'NRT', status: 'scheduled', terminal: 'T1S', type: 'arrival' },
  { flight: 'UA838', airline: 'United', origin: 'NRT', destination: 'SFO', status: 'scheduled', terminal: 'T1S', type: 'departure' },
  { flight: 'AA169', airline: 'American', origin: 'DFW', destination: 'NRT', status: 'enroute', terminal: 'T2', type: 'arrival' },
  { flight: 'DL167', airline: 'Delta', origin: 'ATL', destination: 'NRT', status: 'enroute', terminal: 'T1', type: 'arrival' },
  { flight: 'CX500', airline: 'Cathay', origin: 'HKG', destination: 'NRT', status: 'arrived', terminal: 'T2', type: 'arrival' },
  { flight: 'SQ012', airline: 'Singapore', origin: 'SIN', destination: 'NRT', status: 'enroute', terminal: 'T1S', type: 'arrival' },
  { flight: 'KE703', airline: 'Korean Air', origin: 'ICN', destination: 'NRT', status: 'arrived', terminal: 'T1', type: 'arrival' },
  { flight: 'OZ102', airline: 'Asiana', origin: 'ICN', destination: 'NRT', status: 'arrived', terminal: 'T1', type: 'arrival' },
  { flight: 'CA181', airline: 'Air China', origin: 'PEK', destination: 'NRT', status: 'arrived', terminal: 'T2', type: 'arrival' },
  { flight: 'CI100', airline: 'China Airlines', origin: 'TPE', destination: 'NRT', status: 'arrived', terminal: 'T2', type: 'arrival' },
  { flight: 'BR196', airline: 'EVA Air', origin: 'TPE', destination: 'NRT', status: 'enroute', terminal: 'T2', type: 'arrival' },
  { flight: 'TG676', airline: 'Thai Airways', origin: 'BKK', destination: 'NRT', status: 'enroute', terminal: 'T1', type: 'arrival' },
  { flight: 'LH710', airline: 'Lufthansa', origin: 'FRA', destination: 'NRT', status: 'enroute', terminal: 'T1', type: 'arrival' },
  { flight: 'AF276', airline: 'Air France', origin: 'CDG', destination: 'NRT', status: 'enroute', terminal: 'T1', type: 'arrival' },
  { flight: 'BA005', airline: 'British Airways', origin: 'LHR', destination: 'NRT', status: 'enroute', terminal: 'T2', type: 'arrival' },
  { flight: 'KL861', airline: 'KLM', origin: 'AMS', destination: 'NRT', status: 'enroute', terminal: 'T1', type: 'arrival' },
  { flight: 'TK052', airline: 'Turkish Airlines', origin: 'IST', destination: 'NRT', status: 'enroute', terminal: 'T1', type: 'arrival' },
  { flight: 'EK318', airline: 'Emirates', origin: 'DXB', destination: 'NRT', status: 'arrived', terminal: 'T2', type: 'arrival' },
  { flight: 'QR802', airline: 'Qatar Airways', origin: 'DOH', destination: 'NRT', status: 'enroute', terminal: 'T2', type: 'arrival' },
  { flight: 'EY870', airline: 'Etihad', origin: 'AUH', destination: 'NRT', status: 'enroute', terminal: 'T1', type: 'arrival' },
  { flight: 'PR432', airline: 'Philippine Airlines', origin: 'MNL', destination: 'NRT', status: 'arrived', terminal: 'T1', type: 'arrival' },
  { flight: 'GA882', airline: 'Garuda Indonesia', origin: 'DPS', destination: 'NRT', status: 'arrived', terminal: 'T1', type: 'arrival' },
  { flight: 'MH088', airline: 'Malaysia Airlines', origin: 'KUL', destination: 'NRT', status: 'arrived', terminal: 'T1', type: 'arrival' },
  { flight: 'VN300', airline: 'Vietnam Airlines', origin: 'HAN', destination: 'NRT', status: 'arrived', terminal: 'T1', type: 'arrival' },
  { flight: 'NZ090', airline: 'Air New Zealand', origin: 'AKL', destination: 'NRT', status: 'enroute', terminal: 'T1', type: 'arrival' },
  { flight: 'QF025', airline: 'Qantas', origin: 'SYD', destination: 'NRT', status: 'enroute', terminal: 'T2', type: 'arrival' },
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
      geometry: { type: 'Point', coordinates: [NARITA_LON, NARITA_LAT] },
      properties: {
        flight_id: `NRT_${String(i + 1).padStart(5, '0')}`,
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
        coordinates: [NARITA_LON + (i % 2 === 0 ? offset : -offset), NARITA_LAT + (i % 3 === 0 ? offset : -offset)],
      },
      properties: {
        flight_id: `NRT_${String(i + 1).padStart(5, '0')}`,
        flight_number: f.flight,
        airline: f.airline,
        origin: f.origin,
        destination: f.destination,
        status: f.status,
        terminal: f.terminal,
        type: f.type,
        country: 'JP',
        observed_at: now.toISOString(),
        source: 'narita_seed',
      },
    };
  });
}

export default async function collectNaritaFlights() {
  let features = await tryAeroDataBox();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'narita_flights',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Narita International Airport (RJAA/NRT) arrivals and departures',
    },
    metadata: {},
  };
}
