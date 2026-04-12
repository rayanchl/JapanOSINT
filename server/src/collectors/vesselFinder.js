/**
 * VesselFinder Collector
 * Dedicated VesselFinder live AIS layer for the Japan bounding box.
 * Uses the VesselFinder Master REST API (key-gated).
 * Falls back to OSM ferry/cargo terminals and then a curated ferry hub seed.
 *
 * Env: VESSELFINDER_API_KEY
 * Japan bbox: lat 24-46, lon 122-154
 */

import { fetchJson, fetchOverpass } from './_liveHelpers.js';

const API_KEY = process.env.VESSELFINDER_API_KEY || '';

async function tryApi() {
  if (!API_KEY) return null;
  const url = `https://api.vesselfinder.com/vesselslist?userkey=${API_KEY}&bbox=122,24,154,46&format=json`;
  const data = await fetchJson(url, { timeoutMs: 10000 });
  if (!Array.isArray(data) || data.length === 0) return null;
  return data.map((v, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [parseFloat(v.LONGITUDE), parseFloat(v.LATITUDE)] },
    properties: {
      id: `VF_${v.MMSI || i}`,
      mmsi: v.MMSI,
      imo: v.IMO,
      vessel_name: v.NAME,
      vessel_type: (v.TYPE || 'other').toString().toLowerCase(),
      flag: v.FLAG,
      speed_knots: v.SPEED,
      heading: v.COURSE,
      destination: v.DESTINATION,
      length_m: v.LENGTH,
      last_position_update: v.TIMESTAMP,
      country: 'JP',
      source: 'vesselfinder_api',
    },
  }));
}

async function tryOsmFerries() {
  return await fetchOverpass(
    'node["amenity"="ferry_terminal"](area.jp);way["amenity"="ferry_terminal"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        id: `VF_OSM_${el.id}`,
        vessel_name: el.tags?.name || `Ferry terminal ${i + 1}`,
        vessel_type: 'ferry_terminal',
        operator: el.tags?.operator || null,
        country: 'JP',
        source: 'osm_overpass_ferry',
      },
    }),
  );
}

// Curated major ferry/international passenger hubs
const SEED_HUBS = [
  { name: 'Tokyo International Cruise Terminal', lat: 35.6166, lon: 139.7780 },
  { name: 'Yokohama Daikoku Pier', lat: 35.4568, lon: 139.6784 },
  { name: 'Kobe Port Terminal', lat: 34.6855, lon: 135.1924 },
  { name: 'Osaka Tempozan', lat: 34.6575, lon: 135.4306 },
  { name: 'Nagoya Garden Pier', lat: 35.0883, lon: 136.8814 },
  { name: 'Hakata Port International Terminal', lat: 33.6110, lon: 130.4070 },
  { name: 'Shimonoseki International Terminal', lat: 33.9476, lon: 130.9251 },
  { name: 'Kanmon Ferry Terminal', lat: 33.9470, lon: 130.9700 },
  { name: 'Naha Port Ferry Terminal', lat: 26.2156, lon: 127.6744 },
  { name: 'Hakodate Ferry Terminal', lat: 41.7700, lon: 140.7220 },
  { name: 'Tomakomai East Port', lat: 42.6340, lon: 141.6336 },
  { name: 'Otaru Ferry Terminal', lat: 43.2042, lon: 141.0010 },
  { name: 'Niigata West Port', lat: 37.9453, lon: 139.0430 },
  { name: 'Tsuruga Passenger Terminal', lat: 35.6558, lon: 136.0720 },
  { name: 'Takamatsu Port', lat: 34.3567, lon: 134.0500 },
  { name: 'Matsuyama Mitsuhama', lat: 33.8811, lon: 132.7222 },
  { name: 'Beppu International Tourism Port', lat: 33.2950, lon: 131.5000 },
  { name: 'Kagoshima Shin Port', lat: 31.5816, lon: 130.5700 },
  { name: 'Amami Naze Port', lat: 28.3813, lon: 129.4917 },
  { name: 'Miyakojima Hirara Port', lat: 24.8073, lon: 125.2811 },
  { name: 'Ishigaki Port', lat: 24.3417, lon: 124.1550 },
];

function generateSeed() {
  return SEED_HUBS.map((p, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [p.lon, p.lat] },
    properties: {
      id: `VF_SEED_${String(i + 1).padStart(4, '0')}`,
      vessel_name: p.name,
      vessel_type: 'ferry_terminal',
      country: 'JP',
      source: 'vesselfinder_seed',
    },
  }));
}

export default async function collectVesselFinder() {
  let features = await tryApi();
  let liveSource = 'vesselfinder_api';
  if (!features || !features.length) {
    features = await tryOsmFerries();
    liveSource = 'osm_overpass_ferry';
  }
  const live = !!(features && features.length > 0);
  if (!live) {
    features = generateSeed();
    liveSource = 'vesselfinder_seed';
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'vessel-finder',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: liveSource,
      description: 'VesselFinder live AIS vessels + ferry/cruise terminals around Japan',
    },
    metadata: {},
  };
}
