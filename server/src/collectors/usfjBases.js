/**
 * USFJ Bases Collector
 * United States Forces Japan installations.
 * OSM Overpass + DoD-published list as fallback seed.
 */

const OVERPASS_URL = 'https://overpass-api.de/api/interpreter';

const SEED_USFJ = [
  // USAF
  { name: 'Yokota Air Base', lat: 35.7486, lon: 139.3486, branch: 'USAF', role: 'air_hq', region: 'Kanto' },
  { name: 'Misawa Air Base', lat: 40.7028, lon: 141.3681, branch: 'USAF', role: 'fighter', region: 'Tohoku' },
  { name: 'Kadena Air Base', lat: 26.3556, lon: 127.7681, branch: 'USAF', role: 'air_hq', region: 'Okinawa' },
  // USMC
  { name: 'Marine Corps Air Station Iwakuni', lat: 34.1442, lon: 132.2356, branch: 'USMC', role: 'air_station', region: 'Chugoku' },
  { name: 'Marine Corps Air Station Futenma', lat: 26.2722, lon: 127.7558, branch: 'USMC', role: 'air_station', region: 'Okinawa' },
  { name: 'Camp Foster (Camp Smedley D. Butler)', lat: 26.2914, lon: 127.7464, branch: 'USMC', role: 'hq', region: 'Okinawa' },
  { name: 'Camp Schwab', lat: 26.5239, lon: 128.0556, branch: 'USMC', role: 'training', region: 'Okinawa' },
  { name: 'Camp Hansen', lat: 26.4675, lon: 127.9197, branch: 'USMC', role: 'training', region: 'Okinawa' },
  { name: 'Camp Courtney', lat: 26.3697, lon: 127.8689, branch: 'USMC', role: 'cmd', region: 'Okinawa' },
  { name: 'Camp McTureous', lat: 26.3681, lon: 127.8631, branch: 'USMC', role: 'support', region: 'Okinawa' },
  { name: 'Camp Kinser', lat: 26.2389, lon: 127.7117, branch: 'USMC', role: 'logistics', region: 'Okinawa' },
  { name: 'Camp Lester', lat: 26.3253, lon: 127.7708, branch: 'USMC', role: 'medical', region: 'Okinawa' },
  { name: 'Camp Gonsalves (Jungle Warfare)', lat: 26.7333, lon: 128.2333, branch: 'USMC', role: 'training', region: 'Okinawa' },
  // USN
  { name: 'Fleet Activities Yokosuka', lat: 35.2917, lon: 139.6611, branch: 'USN', role: 'fleet_hq', region: 'Kanto' },
  { name: 'Naval Air Facility Atsugi', lat: 35.4544, lon: 139.4500, branch: 'USN', role: 'air_station', region: 'Kanto' },
  { name: 'Fleet Activities Sasebo', lat: 33.1592, lon: 129.7222, branch: 'USN', role: 'fleet', region: 'Kyushu' },
  { name: 'Naval Computer Telecommunications Station Yokosuka', lat: 35.3000, lon: 139.6700, branch: 'USN', role: 'comms', region: 'Kanto' },
  { name: 'Camp Fuji', lat: 35.2742, lon: 138.8772, branch: 'USMC', role: 'training', region: 'Chubu' },
  { name: 'Combined Arms Training Center Camp Fuji', lat: 35.3000, lon: 138.8800, branch: 'USMC', role: 'training', region: 'Chubu' },
  // US Army
  { name: 'Camp Zama', lat: 35.5111, lon: 139.4017, branch: 'USA', role: 'army_hq', region: 'Kanto' },
  { name: 'Sagami General Depot', lat: 35.5447, lon: 139.3683, branch: 'USA', role: 'depot', region: 'Kanto' },
  { name: 'Sagamihara Family Housing', lat: 35.5358, lon: 139.3869, branch: 'USA', role: 'housing', region: 'Kanto' },
  { name: 'Yokohama North Dock', lat: 35.4500, lon: 139.6500, branch: 'USA', role: 'port', region: 'Kanto' },
  { name: 'Akizuki Ammunition Depot', lat: 34.0283, lon: 132.0617, branch: 'USA', role: 'depot', region: 'Chugoku' },
  { name: 'Kawakami Ammunition Depot', lat: 34.0467, lon: 132.0867, branch: 'USA', role: 'depot', region: 'Chugoku' },
  { name: 'Hiro Ammunition Depot', lat: 34.3033, lon: 132.6017, branch: 'USA', role: 'depot', region: 'Chugoku' },
  { name: 'Kure Pier 6', lat: 34.2400, lon: 132.5550, branch: 'USA', role: 'port', region: 'Chugoku' },
  // Other Okinawa
  { name: 'White Beach Naval Facility', lat: 26.3275, lon: 127.9433, branch: 'USN', role: 'port', region: 'Okinawa' },
  { name: 'Torii Station', lat: 26.3344, lon: 127.7567, branch: 'USA', role: 'army', region: 'Okinawa' },
  { name: 'Camp Shields', lat: 26.3175, lon: 127.7972, branch: 'USA', role: 'support', region: 'Okinawa' },
  { name: 'Awase Communications Station', lat: 26.3306, lon: 127.8336, branch: 'USAF', role: 'comms', region: 'Okinawa' },
  { name: 'Sobe Communication Site', lat: 26.4083, lon: 127.7167, branch: 'USN', role: 'comms', region: 'Okinawa' },
  // Iwo Jima joint
  { name: 'Iwo Jima Air Base (Iwo To)', lat: 24.7836, lon: 141.3225, branch: 'USAF', role: 'aux_field', region: 'Ogasawara' },
];

async function tryOverpass() {
  const query = `[out:json][timeout:180];area["ISO3166-1"="JP"]->.jp;(way["landuse"="military"]["operator"~"US|United States"](area.jp);relation["landuse"="military"]["operator"~"US|United States"](area.jp););out center;`;
  try {
    const ctrl = new AbortController();
    const timeout = setTimeout(() => ctrl.abort(), 12000);
    const res = await fetch(OVERPASS_URL, {
      method: 'POST',
      signal: ctrl.signal,
      headers: { 'Content-Type': 'text/plain' },
      body: query,
    });
    clearTimeout(timeout);
    if (!res.ok) return null;
    const data = await res.json();
    if (!data?.elements?.length) return null;
    return data.elements
      .map((el) => {
        const lat = el.center?.lat ?? el.lat;
        const lon = el.center?.lon ?? el.lon;
        if (lat == null || lon == null) return null;
        return {
          type: 'Feature',
          geometry: { type: 'Point', coordinates: [lon, lat] },
          properties: {
            base_id: `OSM_${el.id}`,
            name: el.tags?.name || 'US Military Installation',
            branch: 'USFJ',
            source: 'osm_overpass',
          },
        };
      })
      .filter(Boolean);
  } catch {
    return null;
  }
}

function generateSeedData() {
  return SEED_USFJ.map((b, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [b.lon, b.lat] },
    properties: {
      base_id: `USFJ_${String(i + 1).padStart(5, '0')}`,
      name: b.name,
      branch: b.branch,
      role: b.role,
      region: b.region,
      country: 'US',
      source: 'usfj_seed',
    },
  }));
}

export default async function collectUsfjBases() {
  let features = await tryOverpass();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'usfj_bases',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'United States Forces Japan installations: USAF, USN, USMC, USA',
    },
    metadata: {},
  };
}
