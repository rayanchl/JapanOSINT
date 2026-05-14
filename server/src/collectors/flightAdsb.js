/**
 * Flight ADS-B Tracking Collector
 * Fuses three data sources for a single plane layer over Japan:
 *   - OpenSky Network (live ADS-B positions, OAuth2 — anonymous tier 429s)
 *   - adsb.lol (live ADS-B positions, no auth, community-fed dump1090 feed)
 *   - AeroDataBox (scheduled arrivals/departures for NRT + HND, RapidAPI key)
 *
 * The two live sources are merged by ICAO24; adsb.lol wins on collision
 * (community feeders surface fresher positions than OpenSky's aggregation
 * lag) but property bags are unioned so OpenSky-only fields are preserved.
 * Either source returning data is enough — only when both fail do we fall
 * back to the seed generator.
 */

import { getOAuthToken } from '../utils/openskyAuth.js';
import { classifyMilitary } from './_militaryIcao.js';
import { getEnv } from '../utils/credentials.js';

// Read at call time (not module load) so a tenant's BYOK key — or a value
// set via the API-keys overlay after boot — flows through. Platform-only
// today (tenantId=null); pass a real tenantId once the scheduler is
// tenant-aware.
const aeroDataboxKey = () => getEnv(null, 'AERODATABOX_KEY') || '';
const AERODATABOX_AIRPORTS = [
  { icao: 'RJAA', iata: 'NRT', name: 'Narita', lat: 35.7720, lon: 140.3929 },
  { icao: 'RJTT', iata: 'HND', name: 'Haneda', lat: 35.5494, lon: 139.7798 },
];

const POSITION_SOURCE = ['ADS-B', 'ASTERIX', 'MLAT', 'FLARM'];
const CATEGORY_LABELS = [
  'No info', 'No ADS-B category', 'Light (<15500 lbs)', 'Small (15500-75000 lbs)',
  'Large (75000-300000 lbs)', 'High Vortex Large', 'Heavy (>300000 lbs)',
  'High Performance', 'Rotorcraft', 'Glider/Sailplane', 'Lighter-than-air',
  'Parachutist/Skydiver', 'Ultralight/Paraglider', 'Reserved', 'UAV',
  'Space/Trans-atmospheric', 'Emergency Vehicle', 'Service Vehicle',
  'Point Obstacle', 'Cluster Obstacle', 'Line Obstacle',
];

const JAPAN_AIRPORTS = [
  { code: 'NRT', name: '成田国際空港', lat: 35.7720, lon: 140.3929, traffic: 10 },
  { code: 'HND', name: '羽田空港', lat: 35.5494, lon: 139.7798, traffic: 10 },
  { code: 'KIX', name: '関西国際空港', lat: 34.4320, lon: 135.2302, traffic: 9 },
  { code: 'ITM', name: '大阪伊丹空港', lat: 34.7855, lon: 135.4380, traffic: 8 },
  { code: 'NGO', name: '中部国際空港', lat: 34.8584, lon: 136.8125, traffic: 7 },
  { code: 'FUK', name: '福岡空港', lat: 33.5859, lon: 130.4508, traffic: 8 },
  { code: 'CTS', name: '新千歳空港', lat: 42.7752, lon: 141.6922, traffic: 8 },
  { code: 'OKA', name: '那覇空港', lat: 26.1958, lon: 127.6459, traffic: 7 },
  { code: 'SDJ', name: '仙台空港', lat: 38.1397, lon: 140.9170, traffic: 5 },
  { code: 'HIJ', name: '広島空港', lat: 34.4361, lon: 132.9194, traffic: 5 },
  { code: 'KMJ', name: '熊本空港', lat: 32.8373, lon: 130.8551, traffic: 4 },
  { code: 'KOJ', name: '鹿児島空港', lat: 31.8034, lon: 130.7186, traffic: 5 },
  { code: 'MYJ', name: '松山空港', lat: 33.8272, lon: 132.6997, traffic: 4 },
  { code: 'TAK', name: '高松空港', lat: 34.2142, lon: 134.0156, traffic: 3 },
  { code: 'OIT', name: '大分空港', lat: 33.4794, lon: 131.7373, traffic: 3 },
  { code: 'NGS', name: '長崎空港', lat: 32.9169, lon: 129.9136, traffic: 4 },
  { code: 'KMI', name: '宮崎空港', lat: 31.8772, lon: 131.4492, traffic: 3 },
  { code: 'AOJ', name: '青森空港', lat: 40.7347, lon: 140.6908, traffic: 3 },
  { code: 'AKJ', name: '旭川空港', lat: 43.6708, lon: 142.4475, traffic: 3 },
  { code: 'KKJ', name: '北九州空港', lat: 33.8459, lon: 131.0347, traffic: 3 },
  { code: 'ISG', name: '石垣空港', lat: 24.3964, lon: 124.2453, traffic: 3 },
  { code: 'MMY', name: '宮古空港', lat: 24.7828, lon: 125.2953, traffic: 2 },
  { code: 'OKD', name: '札幌丘珠空港', lat: 43.1161, lon: 141.3817, traffic: 2 },
  { code: 'TOY', name: '富山空港', lat: 36.6483, lon: 137.1876, traffic: 2 },
  { code: 'KMQ', name: '小松空港', lat: 36.3946, lon: 136.4069, traffic: 3 },
  // JSDF bases
  { code: 'RJTY', name: '横田基地', lat: 35.7485, lon: 139.3484, traffic: 2 },
  { code: 'RJAH', name: '百里基地', lat: 36.1811, lon: 140.4147, traffic: 2 },
  { code: 'RJNK', name: '小松基地', lat: 36.3946, lon: 136.4069, traffic: 2 },
];

const AIRLINES = [
  { code: 'ANA', name: 'All Nippon Airways', callsign: 'ALL NIPPON' },
  { code: 'JAL', name: 'Japan Airlines', callsign: 'JAPAN AIR' },
  { code: 'APJ', name: 'Peach Aviation', callsign: 'AIR PEACH' },
  { code: 'JJP', name: 'Jetstar Japan', callsign: 'ORANGE LINER' },
  { code: 'SKY', name: 'Skymark Airlines', callsign: 'SKYMARK' },
  { code: 'SFJ', name: 'StarFlyer', callsign: 'STAR FLYER' },
  { code: 'ADO', name: 'Air Do', callsign: 'AIR DO' },
  { code: 'SNA', name: 'Solaseed Air', callsign: 'SOLASEED' },
  { code: 'NCA', name: 'Nippon Cargo Airlines', callsign: 'NIPPON CARGO' },
  { code: 'JSDF', name: 'Japan Self-Defense Forces', callsign: 'JASDF' },
];

const AIRCRAFT_TYPES = ['B737', 'B767', 'B777', 'B787', 'A320', 'A321', 'A350', 'DHC8', 'E170', 'E190', 'C-130', 'F-15', 'P-1'];

function seededRandom(seed) {
  let x = Math.sin(seed * 9301 + 49297) * 233280;
  return x - Math.floor(x);
}

function generateSeedData() {
  const features = [];
  let idx = 0;
  const now = new Date();

  // Aircraft near airports (departing/arriving)
  for (const airport of JAPAN_AIRPORTS) {
    const count = Math.max(2, airport.traffic);
    for (let j = 0; j < count; j++) {
      idx++;
      const r1 = seededRandom(idx * 3);
      const r2 = seededRandom(idx * 7);
      const r3 = seededRandom(idx * 11);

      // Spread aircraft along approach/departure paths
      const dist = seededRandom(idx * 5) * 0.8;
      const angle = seededRandom(idx * 9) * Math.PI * 2;
      const lat = airport.lat + Math.sin(angle) * dist;
      const lon = airport.lon + Math.cos(angle) * dist;

      const airline = AIRLINES[Math.floor(r1 * AIRLINES.length)];
      const aircraftType = AIRCRAFT_TYPES[Math.floor(r2 * AIRCRAFT_TYPES.length)];
      const altitude = Math.floor(1000 + seededRandom(idx * 13) * 40000);
      const speed = Math.floor(150 + seededRandom(idx * 17) * 500);
      const heading = Math.floor(r3 * 360);
      const vertRate = altitude < 10000 ? Math.floor((seededRandom(idx * 19) - 0.3) * 3000) : 0;

      const flightNum = `${airline.code}${Math.floor(100 + seededRandom(idx * 23) * 900)}`;
      const icao24 = Math.floor(seededRandom(idx * 29) * 16777215).toString(16).padStart(6, '0');

      // Determine origin/destination
      const otherAirport = JAPAN_AIRPORTS[Math.floor(seededRandom(idx * 31) * JAPAN_AIRPORTS.length)];

      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [lon, lat] },
        properties: {
          id: `ADSB_${String(idx).padStart(5, '0')}`,
          icao24,
          callsign: flightNum,
          airline: airline.name,
          aircraft_type: aircraftType,
          altitude_ft: altitude,
          ground_speed_knots: speed,
          heading,
          vertical_rate_fpm: vertRate,
          on_ground: altitude < 100,
          origin: airport.code,
          destination: otherAirport.code,
          squawk: String(Math.floor(1000 + seededRandom(idx * 37) * 6777)).padStart(4, '0'),
          last_update: new Date(now - Math.floor(seededRandom(idx * 41) * 5) * 60000).toISOString(),
          source: 'adsb_tracking',
        },
      });
    }
  }

  // En-route aircraft (between airports)
  for (let i = 0; i < 30; i++) {
    idx++;
    const lat = 28 + seededRandom(idx * 3) * 17; // 28N to 45N
    const lon = 128 + seededRandom(idx * 7) * 18; // 128E to 146E

    const airline = AIRLINES[Math.floor(seededRandom(idx * 11) * AIRLINES.length)];

    features.push({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [lon, lat] },
      properties: {
        id: `ADSB_${String(idx).padStart(5, '0')}`,
        icao24: Math.floor(seededRandom(idx * 13) * 16777215).toString(16).padStart(6, '0'),
        callsign: `${airline.code}${Math.floor(100 + seededRandom(idx * 17) * 900)}`,
        airline: airline.name,
        aircraft_type: AIRCRAFT_TYPES[Math.floor(seededRandom(idx * 19) * AIRCRAFT_TYPES.length)],
        altitude_ft: Math.floor(30000 + seededRandom(idx * 23) * 12000),
        ground_speed_knots: Math.floor(400 + seededRandom(idx * 29) * 200),
        heading: Math.floor(seededRandom(idx * 31) * 360),
        vertical_rate_fpm: 0,
        on_ground: false,
        status: 'en_route',
        last_update: new Date(now - Math.floor(seededRandom(idx * 37) * 3) * 60000).toISOString(),
        source: 'adsb_tracking',
      },
    });
  }

  return features;
}

export { AERODATABOX_AIRPORTS };

export async function tryOpenSkyAPI() {
  try {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 10000);

    const headers = {};
    const token = await getOAuthToken();
    if (token) headers.Authorization = `Bearer ${token}`;

    // Japan bounding box: lat 24-46, lon 122-154
    const res = await fetch(
      'https://opensky-network.org/api/states/all?lamin=24&lomin=122&lamax=46&lomax=154',
      { headers, signal: controller.signal }
    );
    clearTimeout(timeout);
    if (!res.ok) return null;
    const data = await res.json();
    if (!data.states) return null;
    return data.states.slice(0, 200).map((s, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [s[5] || 139.7, s[6] || 35.6] },
      properties: {
        id: `ADSB_LIVE_${i}`,
        icao24: s[0],
        callsign: (s[1] || '').trim(),
        origin_country: s[2],
        time_position: s[3],
        last_contact: s[4],
        baro_altitude_m: s[7],
        altitude_ft: s[7] != null ? Math.round(s[7] * 3.28084) : null,
        on_ground: s[8],
        velocity_mps: s[9],
        ground_speed_knots: s[9] != null ? Math.round(s[9] * 1.94384) : null,
        heading: Math.round(s[10] || 0),
        true_track: s[10] != null ? Math.round(s[10]) : null,
        vertical_rate_fpm: s[11] != null ? Math.round(s[11] * 196.85) : null,
        geo_altitude_m: s[13],
        geo_altitude_ft: s[13] != null ? Math.round(s[13] * 3.28084) : null,
        squawk: s[14],
        spi: s[15],
        position_source: POSITION_SOURCE[s[16]] || s[16],
        category: s[17] != null ? (CATEGORY_LABELS[s[17]] || s[17]) : null,
        source: 'opensky_api',
      },
    }));
  } catch {
    return null;
  }
}

// adsb.lol is a community ADS-B aggregator that exposes a dump1090-style
// JSON feed at /v2/lat/<lat>/lon/<lon>/dist/<nm>. The radius is capped at
// 250 nm (~463 km), so covering the Japanese archipelago needs four
// overlapping queries — Hokkaido, Honshu, Kyushu/Shikoku, and Sakishima/
// Okinawa. Inter-quadrant duplicates collapse on ICAO24 inside the helper
// so the caller sees one Feature per aircraft.
const ADSBLOL_QUADRANTS = [
  { lat: 43, lon: 142, dist: 250 }, // Hokkaido + northern Tohoku
  { lat: 36, lon: 138, dist: 250 }, // central Honshu
  { lat: 32, lon: 131, dist: 250 }, // Kyushu / Shikoku
  { lat: 26, lon: 128, dist: 250 }, // Okinawa / Sakishima
];

export async function tryAdsbLol() {
  const ok = [];
  let failedQuadrants = 0;
  await Promise.all(ADSBLOL_QUADRANTS.map(async (q) => {
    const ctrl = new AbortController();
    const timeout = setTimeout(() => ctrl.abort(), 8000);
    try {
      const res = await fetch(
        `https://api.adsb.lol/v2/lat/${q.lat}/lon/${q.lon}/dist/${q.dist}`,
        { signal: ctrl.signal, headers: { accept: 'application/json' } }
      );
      if (!res.ok) { failedQuadrants += 1; return; }
      const data = await res.json();
      if (Array.isArray(data?.ac)) ok.push(...data.ac);
    } catch {
      failedQuadrants += 1;
    } finally {
      clearTimeout(timeout);
    }
  }));
  // Treat a complete blackout as a hard failure so the poller can mark the
  // source down and back off. Partial failures (1–3 quadrants) still yield
  // useful coverage and are reported as success.
  if (failedQuadrants === ADSBLOL_QUADRANTS.length) return null;

  const byIcao = new Map();
  for (const ac of ok) {
    const icao = (ac?.hex || '').toLowerCase();
    if (!icao) continue;
    if (typeof ac.lat !== 'number' || typeof ac.lon !== 'number') continue;
    byIcao.set(icao, ac);
  }
  return [...byIcao.values()].map((ac) => {
    const callsign = (ac.flight || '').trim() || null;
    const heading = typeof ac.track === 'number' ? Math.round(ac.track)
      : typeof ac.true_heading === 'number' ? Math.round(ac.true_heading)
      : 0;
    const altFt = typeof ac.alt_baro === 'number' ? ac.alt_baro : null;
    const onGround = ac.alt_baro === 'ground';
    return {
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [ac.lon, ac.lat] },
      properties: {
        id: `ADSBLOL_${ac.hex}`,
        icao24: ac.hex.toLowerCase(),
        callsign,
        registration: ac.r || null,
        aircraft_type: ac.t || null,
        altitude_ft: altFt,
        geo_altitude_ft: typeof ac.alt_geom === 'number' ? ac.alt_geom : null,
        ground_speed_knots: typeof ac.gs === 'number' ? Math.round(ac.gs) : null,
        true_airspeed_knots: typeof ac.tas === 'number' ? Math.round(ac.tas) : null,
        indicated_airspeed_knots: typeof ac.ias === 'number' ? Math.round(ac.ias) : null,
        mach: typeof ac.mach === 'number' ? ac.mach : null,
        heading,
        true_track: typeof ac.track === 'number' ? Math.round(ac.track) : null,
        magnetic_heading: typeof ac.mag_heading === 'number' ? Math.round(ac.mag_heading) : null,
        vertical_rate_fpm: typeof ac.baro_rate === 'number' ? ac.baro_rate
          : typeof ac.geom_rate === 'number' ? ac.geom_rate
          : null,
        squawk: ac.squawk || null,
        category: ac.category || null, // already an ADS-B category string ("A3" etc.)
        emergency: ac.emergency || null,
        on_ground: onGround,
        last_seen_s: typeof ac.seen === 'number' ? ac.seen : null,
        rssi: typeof ac.rssi === 'number' ? ac.rssi : null,
        source: 'adsblol_api',
      },
    };
  });
}

/**
 * Merge any number of live-position source arrays into a single ICAO24-
 * keyed list. Later sources overwrite earlier ones on the geometry / id /
 * source fields, but property bags are unioned so each source's unique
 * fields survive. Pass sources in order of increasing freshness — for
 * Japan that's [opensky, adsblol]: adsb.lol's community feed sees aircraft
 * seconds after they squawk, OpenSky's aggregation runs minutes behind.
 */
export function mergeLiveByIcao(...sources) {
  const merged = new Map();
  for (const list of sources) {
    if (!Array.isArray(list)) continue;
    for (const f of list) {
      const icao = (f?.properties?.icao24 || '').toLowerCase();
      if (!icao) continue;
      f.properties.icao24 = icao;
      const prev = merged.get(icao);
      if (prev) {
        const prevSrc = prev.properties.source;
        const thisSrc = f.properties.source;
        f.properties = {
          ...prev.properties,
          ...f.properties,
          source: prevSrc && thisSrc && prevSrc !== thisSrc
            ? `${prevSrc}+${thisSrc}`
            : (thisSrc || prevSrc || null),
        };
      }
      merged.set(icao, f);
    }
  }
  return [...merged.values()];
}

export async function tryAeroDataBoxAirport(airport) {
  const key = aeroDataboxKey();
  if (!key) return [];
  try {
    const ctrl = new AbortController();
    const timeout = setTimeout(() => ctrl.abort(), 10000);
    const now = new Date();
    const start = now.toISOString().slice(0, 16);
    const end = new Date(now.getTime() + 11 * 3600 * 1000).toISOString().slice(0, 16);
    const url = `https://aerodatabox.p.rapidapi.com/flights/airports/icao/${airport.icao}/${start}/${end}?withLeg=true&direction=Both&withCancelled=true&withCargo=false`;
    const res = await fetch(url, {
      signal: ctrl.signal,
      headers: {
        'X-RapidAPI-Key': key,
        'X-RapidAPI-Host': 'aerodatabox.p.rapidapi.com',
      },
    });
    clearTimeout(timeout);
    if (!res.ok) return [];
    const data = await res.json();
    const arrivals = (data.arrivals || []).map(f => ({ f, dir: 'arrival' }));
    const departures = (data.departures || []).map(f => ({ f, dir: 'departure' }));
    const all = [...arrivals, ...departures].slice(0, 150);
    return all.map(({ f, dir }, i) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [airport.lon, airport.lat] },
      properties: {
        id: `${airport.iata}_ADB_${String(i + 1).padStart(5, '0')}`,
        callsign: f.number || null,
        flight_number: f.number || null,
        airline: f.airline?.name || null,
        aircraft_type: f.aircraft?.model || null,
        origin: dir === 'arrival' ? (f.movement?.airport?.iata || null) : airport.iata,
        destination: dir === 'arrival' ? airport.iata : (f.movement?.airport?.iata || null),
        status: f.status || null,
        type: dir,
        airport: `${airport.name} (${airport.icao}/${airport.iata})`,
        scheduled_time: f.movement?.scheduledTime?.utc || null,
        revised_time: f.movement?.revisedTime?.utc || null,
        terminal: f.movement?.terminal || null,
        source: 'aerodatabox_api',
      },
    }));
  } catch {
    return [];
  }
}

// IATA→ICAO airline prefix map for callsign normalization (flights often appear
// as "NH106" in AeroDataBox but "ANA106" in OpenSky callsigns).
const IATA_TO_ICAO = {
  NH: 'ANA', JL: 'JAL', MM: 'APJ', GK: 'JJP', BC: 'SKY', '7G': 'SFJ',
  HD: 'ADO', '6J': 'SNA', KZ: 'NCA', NQ: 'AJX', JW: 'VNL', IJ: 'SJO',
};

export function normalizeCallsign(raw) {
  if (!raw) return null;
  const s = String(raw).trim().toUpperCase().replace(/\s+/g, '');
  if (!s) return null;
  const m = s.match(/^([A-Z0-9]{2,3})(\d+[A-Z]?)$/);
  if (!m) return s;
  const [, prefix, num] = m;
  const n = String(parseInt(num, 10));
  const icao = IATA_TO_ICAO[prefix] || prefix;
  return `${icao}${n}`;
}

export function dedupeFeatures(openskyFeatures, aeroFeatures) {
  const byKey = new Map();
  const result = [];

  for (const f of openskyFeatures || []) {
    const key = normalizeCallsign(f.properties?.callsign);
    if (key) byKey.set(key, f);
    result.push(f);
  }

  for (const f of aeroFeatures) {
    const key = normalizeCallsign(f.properties?.flight_number || f.properties?.callsign);
    if (key && byKey.has(key)) {
      const live = byKey.get(key);
      live.properties = {
        ...f.properties,
        ...live.properties,
        airline: live.properties.airline || f.properties.airline,
        scheduled_time: f.properties.scheduled_time,
        revised_time: f.properties.revised_time,
        terminal: f.properties.terminal,
        status: live.properties.status || f.properties.status,
        type: live.properties.type || f.properties.type,
        source: 'opensky+aerodatabox',
      };
      continue;
    }
    if (key) byKey.set(key, f);
    result.push(f);
  }

  return result;
}

export default async function collectFlightAdsb() {
  const [openskyFeatures, adsbLolFeatures, ...aeroResults] = await Promise.all([
    tryOpenSkyAPI(),
    tryAdsbLol(),
    ...AERODATABOX_AIRPORTS.map(tryAeroDataBoxAirport),
  ]);
  const aeroFeatures = aeroResults.flat();
  // OpenSky first so adsb.lol's fresher position wins on the union.
  const liveFeatures = mergeLiveByIcao(openskyFeatures, adsbLolFeatures);

  let features = dedupeFeatures(liveFeatures, aeroFeatures);
  const tags = [];
  if (Array.isArray(openskyFeatures) && openskyFeatures.length) tags.push('opensky');
  if (Array.isArray(adsbLolFeatures) && adsbLolFeatures.length) tags.push('adsblol');
  if (aeroFeatures.length) tags.push('aerodatabox');
  let liveSource = tags.length ? tags.join('+') : 'seed';

  if (features.length === 0) {
    features = [];
    liveSource = 'seed';
  }

  for (const f of features) {
    const tag = classifyMilitary({
      icao24: f.properties?.icao24,
      callsign: f.properties?.callsign || f.properties?.flight_number,
    });
    f.properties = { ...f.properties, ...tag };
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'adsb_tracking',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live_source: liveSource,
      description: 'ADS-B aircraft over Japan (OpenSky + adsb.lol) fused with scheduled NRT/HND flights (AeroDataBox)',
    },
  };
}
