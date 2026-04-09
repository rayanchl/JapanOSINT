/**
 * Internet Exchanges Collector
 * Tries PeeringDB API for Japan IXPs, falls back to seed of major IXPs.
 */

const PEERINGDB_URL = 'https://www.peeringdb.com/api/ix?country=JP';

const SEED_IX = [
  // JPNAP (Internet Multifeed)
  { name: 'JPNAP Tokyo I', lat: 35.6900, lon: 139.7650, operator: 'JPNAP', members: 230, traffic_gbps: 6500, location: 'Tokyo' },
  { name: 'JPNAP Tokyo II', lat: 35.6800, lon: 139.7700, operator: 'JPNAP', members: 200, traffic_gbps: 5500, location: 'Tokyo' },
  { name: 'JPNAP Tokyo III', lat: 35.6850, lon: 139.7600, operator: 'JPNAP', members: 180, traffic_gbps: 4800, location: 'Tokyo' },
  { name: 'JPNAP Osaka', lat: 34.6913, lon: 135.5023, operator: 'JPNAP', members: 100, traffic_gbps: 1200, location: 'Osaka' },
  // JPIX
  { name: 'JPIX Tokyo Otemachi', lat: 35.6889, lon: 139.7650, operator: 'JPIX', members: 215, traffic_gbps: 6000, location: 'Tokyo' },
  { name: 'JPIX Tokyo Akihabara', lat: 35.6983, lon: 139.7731, operator: 'JPIX', members: 150, traffic_gbps: 4000, location: 'Tokyo' },
  { name: 'JPIX Osaka', lat: 34.6925, lon: 135.5050, operator: 'JPIX', members: 90, traffic_gbps: 1100, location: 'Osaka' },
  { name: 'JPIX Nagoya', lat: 35.1700, lon: 136.8800, operator: 'JPIX', members: 35, traffic_gbps: 350, location: 'Nagoya' },
  { name: 'JPIX Fukuoka', lat: 33.5900, lon: 130.4017, operator: 'JPIX', members: 25, traffic_gbps: 200, location: 'Fukuoka' },
  { name: 'JPIX Hokkaido', lat: 43.0640, lon: 141.3469, operator: 'JPIX', members: 20, traffic_gbps: 150, location: 'Sapporo' },
  { name: 'JPIX Sendai', lat: 38.2682, lon: 140.8721, operator: 'JPIX', members: 18, traffic_gbps: 100, location: 'Sendai' },
  { name: 'JPIX Okinawa', lat: 26.2125, lon: 127.6809, operator: 'JPIX', members: 12, traffic_gbps: 50, location: 'Okinawa' },
  // BBIX (SoftBank)
  { name: 'BBIX Tokyo', lat: 35.6694, lon: 139.7508, operator: 'BBIX', members: 195, traffic_gbps: 5800, location: 'Tokyo' },
  { name: 'BBIX Osaka', lat: 34.6913, lon: 135.5023, operator: 'BBIX', members: 80, traffic_gbps: 950, location: 'Osaka' },
  { name: 'BBIX Nagoya', lat: 35.1700, lon: 136.8800, operator: 'BBIX', members: 30, traffic_gbps: 300, location: 'Nagoya' },
  { name: 'BBIX Fukuoka', lat: 33.5900, lon: 130.4017, operator: 'BBIX', members: 22, traffic_gbps: 180, location: 'Fukuoka' },
  // Equinix Internet Exchange
  { name: 'Equinix IX Tokyo', lat: 35.6669, lon: 139.7656, operator: 'Equinix IX', members: 130, traffic_gbps: 1800, location: 'Tokyo' },
  { name: 'Equinix IX Osaka', lat: 34.6913, lon: 135.5023, operator: 'Equinix IX', members: 60, traffic_gbps: 600, location: 'Osaka' },
  // DIX-IE (Distributed IX)
  { name: 'DIX-IE Tokyo', lat: 35.6900, lon: 139.7650, operator: 'DIX-IE', members: 30, traffic_gbps: 100, location: 'Tokyo' },
  // EIE (KDDI)
  { name: 'KDDI Power IX (TIX)', lat: 35.6500, lon: 139.7300, operator: 'KDDI', members: 50, traffic_gbps: 400, location: 'Tokyo' },
  // Other regional
  { name: 'Hokkaido KIX', lat: 43.0640, lon: 141.3469, operator: 'Hokuyu', members: 15, traffic_gbps: 80, location: 'Sapporo' },
  { name: 'Hiroshima HIX', lat: 34.3963, lon: 132.4596, operator: 'HIX', members: 12, traffic_gbps: 60, location: 'Hiroshima' },
  { name: 'Okinawa OIX', lat: 26.2125, lon: 127.6809, operator: 'OIX', members: 10, traffic_gbps: 30, location: 'Naha' },
];

async function tryPeeringDb() {
  try {
    const ctrl = new AbortController();
    const timeout = setTimeout(() => ctrl.abort(), 10000);
    const res = await fetch(PEERINGDB_URL, {
      signal: ctrl.signal,
      headers: { Accept: 'application/json' },
    });
    clearTimeout(timeout);
    if (!res.ok) return null;
    const data = await res.json();
    if (!data?.data?.length) return null;
    // PeeringDB ix entries don't always include lat/lon — fall back to seed if they don't.
    const features = data.data
      .filter((ix) => ix.latitude != null && ix.longitude != null)
      .map((ix) => ({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [ix.longitude, ix.latitude] },
        properties: {
          ix_id: `PDB_${ix.id}`,
          name: ix.name,
          operator: ix.org_name || 'unknown',
          city: ix.city,
          source: 'peeringdb',
        },
      }));
    return features.length ? features : null;
  } catch {
    return null;
  }
}

function generateSeedData() {
  return SEED_IX.map((x, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [x.lon, x.lat] },
    properties: {
      ix_id: `IX_${String(i + 1).padStart(5, '0')}`,
      name: x.name,
      operator: x.operator,
      members: x.members,
      traffic_gbps: x.traffic_gbps,
      location: x.location,
      country: 'JP',
      source: 'ixp_seed',
    },
  }));
}

export default async function collectInternetExchanges() {
  let features = await tryPeeringDb();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'internet_exchanges',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Japanese internet exchange points: JPNAP, JPIX, BBIX, Equinix IX, DIX-IE, regional IXPs (PeeringDB API)',
    },
    metadata: {},
  };
}
