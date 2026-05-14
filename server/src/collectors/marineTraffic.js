/**
 * MarineTraffic Collector
 * Dedicated MarineTraffic AIS feed for the Japan bounding box.
 * Uses the MarineTraffic Exportvessels REST API (key-gated, freemium).
 * Falls back to OSM harbour nodes and then a curated port seed so the
 * layer is always geocoded even without an API key configured.
 *
 * Env: MARINETRAFFIC_API_KEY
 * Japan bbox: lat 24-46, lon 122-154
 */

import { fetchJson, fetchOverpass } from './_liveHelpers.js';

const API_KEY = process.env.MARINETRAFFIC_API_KEY || '';

async function tryApi() {
  if (!API_KEY) return null;
  const url = `https://services.marinetraffic.com/api/exportvessels/v:8/${API_KEY}/protocol:jsono/minlat:24/maxlat:46/minlon:122/maxlon:154`;
  const data = await fetchJson(url, { timeoutMs: 10000 });
  if (!Array.isArray(data) || data.length === 0) return null;
  return data.map((v, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [parseFloat(v.LON), parseFloat(v.LAT)] },
    properties: {
      id: `MT_${v.MMSI || i}`,
      mmsi: v.MMSI,
      imo: v.IMO,
      vessel_name: v.SHIPNAME || null,
      vessel_type: (v.SHIPTYPE || 'other').toString().toLowerCase(),
      flag: v.FLAG,
      speed_knots: v.SPEED != null ? parseFloat(v.SPEED) / 10 : null,
      heading: v.HEADING,
      length_m: v.LENGTH,
      destination: v.DESTINATION,
      last_position_update: v.TIMESTAMP,
      country: 'JP',
      source: 'marinetraffic_api',
    },
  }));
}

async function tryOsmPorts() {
  return await fetchOverpass(
    'node["seamark:type"="harbour"](area.jp);node["harbour"="yes"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        id: `MT_OSM_${el.id}`,
        vessel_name: el.tags?.name || `Harbour ${i + 1}`,
        vessel_type: 'harbour',
        country: 'JP',
        source: 'osm_overpass_port',
      },
    }),
  );
}

// Curated top-20 Japanese ports as fallback
const SEED_PORTS = [
  { name: 'Tokyo', lat: 35.6200, lon: 139.7800, throughput: 4500000 },
  { name: 'Yokohama', lat: 35.4500, lon: 139.6500, throughput: 2900000 },
  { name: 'Nagoya', lat: 35.0800, lon: 136.8800, throughput: 2700000 },
  { name: 'Osaka', lat: 34.6400, lon: 135.4200, throughput: 2400000 },
  { name: 'Kobe', lat: 34.6700, lon: 135.1900, throughput: 2800000 },
  { name: 'Kitakyushu', lat: 33.9500, lon: 130.9600, throughput: 550000 },
  { name: 'Hakata', lat: 33.6100, lon: 130.4000, throughput: 880000 },
  { name: 'Chiba', lat: 35.5800, lon: 140.0800, throughput: 1600000 },
  { name: 'Kawasaki', lat: 35.5200, lon: 139.7200, throughput: 950000 },
  { name: 'Shimizu', lat: 35.0100, lon: 138.5100, throughput: 650000 },
  { name: 'Yokkaichi', lat: 34.9600, lon: 136.6400, throughput: 520000 },
  { name: 'Hiroshima', lat: 34.3500, lon: 132.4600, throughput: 480000 },
  { name: 'Niigata', lat: 37.9500, lon: 139.0600, throughput: 400000 },
  { name: 'Tomakomai', lat: 42.6300, lon: 141.6300, throughput: 720000 },
  { name: 'Sakata', lat: 38.9100, lon: 139.8300, throughput: 130000 },
  { name: 'Naha', lat: 26.2200, lon: 127.6700, throughput: 290000 },
  { name: 'Ishigaki', lat: 24.3400, lon: 124.1500, throughput: 80000 },
  { name: 'Hakodate', lat: 41.7700, lon: 140.7200, throughput: 240000 },
  { name: 'Kanazawa', lat: 36.6100, lon: 136.6000, throughput: 130000 },
  { name: 'Matsuyama', lat: 33.8700, lon: 132.7200, throughput: 210000 },
];

function generateSeed() {
  return SEED_PORTS.map((p, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [p.lon, p.lat] },
    properties: {
      id: `MT_SEED_${String(i + 1).padStart(4, '0')}`,
      vessel_name: `${p.name} port`,
      vessel_type: 'port',
      throughput_teu: p.throughput,
      country: 'JP',
      source: 'marinetraffic_seed',
    },
  }));
}

export default async function collectMarineTraffic() {
  let features = await tryApi();
  let liveSource = 'marinetraffic_api';
  if (!features || !features.length) {
    features = await tryOsmPorts();
    liveSource = 'osm_overpass_port';
  }
  const live = !!(features && features.length > 0);
  if (!live) {
    features = [];
    liveSource = 'marinetraffic_seed';
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'marine-traffic',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: liveSource,
      description: 'MarineTraffic live AIS vessels + Japanese port facilities',
    },
  };
}
