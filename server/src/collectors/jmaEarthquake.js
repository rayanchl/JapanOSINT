/**
 * JMA Earthquake Data Collector
 * Fetches earthquake data from Japan Meteorological Agency
 * Source: https://www.jma.go.jp/bosai/quake/data/list.json
 */

const API_URL = 'https://www.jma.go.jp/bosai/quake/data/list.json';
const TIMEOUT_MS = 5000;

// JMA's list.json encodes the hypocenter in a single ISO 6709 string
// (`cod` field, e.g. "+36.5+137.9-10000/" → lat 36.5, lon 137.9, depth 10000m).
// Returns { lat, lon, depthKm } or null if it can't be parsed.
function parseIso6709(cod) {
  if (typeof cod !== 'string') return null;
  // Two or three signed decimal numbers in a row, optional trailing '/'.
  const m = cod.match(/^([+-]\d+(?:\.\d+)?)([+-]\d+(?:\.\d+)?)(?:([+-]\d+(?:\.\d+)?))?/);
  if (!m) return null;
  const lat = parseFloat(m[1]);
  const lon = parseFloat(m[2]);
  const depthM = m[3] != null ? parseFloat(m[3]) : null;
  if (!Number.isFinite(lat) || !Number.isFinite(lon)) return null;
  return {
    lat,
    lon,
    depthKm: depthM != null && Number.isFinite(depthM) ? Math.abs(depthM) / 1000 : null,
  };
}

function buildFeature(eq) {
  let lat = eq.lat ?? eq.hypocenter?.lat;
  let lon = eq.lon ?? eq.hypocenter?.lon;
  let depthKm = eq.dep ?? eq.depth ?? null;
  if (lat == null || lon == null) {
    const parsed = parseIso6709(eq.cod);
    if (!parsed) return null;
    lat = parsed.lat;
    lon = parsed.lon;
    if (depthKm == null) depthKm = parsed.depthKm;
  }

  const magRaw = eq.mag ?? eq.magnitude ?? null;
  const mag = magRaw != null && magRaw !== '' ? parseFloat(magRaw) : null;
  const maxIntensity = eq.maxi ?? eq.maxIntensity ?? null;

  return {
    type: 'Feature',
    geometry: {
      type: 'Point',
      coordinates: [lon, lat],
    },
    properties: {
      earthquake_id: eq.eid ?? eq.id ?? null,
      magnitude: Number.isFinite(mag) ? mag : magRaw,
      depth_km: depthKm,
      max_intensity: maxIntensity,
      timestamp: eq.at ?? eq.time ?? eq.rdt ?? null,
      place: eq.anm ?? eq.areaName ?? eq.hypocenter?.name ?? eq.en_anm ?? null,
      title: eq.ttl ?? null,
      tsunami_warning: eq.tsu ?? false,
      source: 'jma',
    },
  };
}

function parseLiveData(data) {
  const items = Array.isArray(data) ? data : [];
  const features = [];
  for (const eq of items) {
    const f = buildFeature(eq);
    if (f) features.push(f);
  }
  return features;
}

function getSeedData() {
  const quakes = [
    { lat: 36.945, lon: 141.148, mag: 4.2, depth: 30, place: '福島県沖', intensity: '3', time: '2026-04-05T14:23:00+09:00' },
    { lat: 33.252, lon: 132.108, mag: 3.8, depth: 40, place: '愛媛県南予', intensity: '3', time: '2026-04-05T11:05:00+09:00' },
    { lat: 35.301, lon: 139.472, mag: 3.1, depth: 20, place: '神奈川県西部', intensity: '2', time: '2026-04-05T08:47:00+09:00' },
    { lat: 42.931, lon: 145.378, mag: 5.1, depth: 60, place: '釧路沖', intensity: '4', time: '2026-04-04T22:15:00+09:00' },
    { lat: 34.081, lon: 135.598, mag: 2.9, depth: 10, place: '和歌山県北部', intensity: '2', time: '2026-04-04T19:33:00+09:00' },
    { lat: 38.789, lon: 141.892, mag: 4.5, depth: 50, place: '宮城県沖', intensity: '3', time: '2026-04-04T16:01:00+09:00' },
    { lat: 31.468, lon: 130.672, mag: 3.4, depth: 15, place: '鹿児島湾', intensity: '2', time: '2026-04-04T13:28:00+09:00' },
    { lat: 35.879, lon: 137.521, mag: 2.7, depth: 8, place: '長野県南部', intensity: '1', time: '2026-04-04T10:55:00+09:00' },
    { lat: 37.394, lon: 141.787, mag: 4.8, depth: 45, place: '福島県沖', intensity: '4', time: '2026-04-04T07:12:00+09:00' },
    { lat: 34.951, lon: 138.872, mag: 3.0, depth: 25, place: '静岡県中部', intensity: '2', time: '2026-04-03T23:44:00+09:00' },
    { lat: 28.398, lon: 129.512, mag: 4.1, depth: 35, place: '奄美大島近海', intensity: '3', time: '2026-04-03T20:18:00+09:00' },
    { lat: 43.312, lon: 145.812, mag: 3.6, depth: 70, place: '根室半島南東沖', intensity: '2', time: '2026-04-03T17:05:00+09:00' },
    { lat: 36.512, lon: 140.622, mag: 3.3, depth: 55, place: '茨城県北部', intensity: '2', time: '2026-04-03T14:33:00+09:00' },
    { lat: 33.801, lon: 131.972, mag: 2.5, depth: 12, place: '大分県中部', intensity: '1', time: '2026-04-03T11:00:00+09:00' },
    { lat: 39.661, lon: 140.095, mag: 3.7, depth: 10, place: '秋田県内陸南部', intensity: '3', time: '2026-04-03T08:22:00+09:00' },
    { lat: 35.362, lon: 133.332, mag: 2.8, depth: 14, place: '鳥取県中部', intensity: '1', time: '2026-04-03T05:45:00+09:00' },
    { lat: 32.752, lon: 131.078, mag: 3.9, depth: 20, place: '熊本県熊本地方', intensity: '3', time: '2026-04-02T22:10:00+09:00' },
    { lat: 40.821, lon: 143.228, mag: 5.3, depth: 80, place: '青森県東方沖', intensity: '3', time: '2026-04-02T18:35:00+09:00' },
    { lat: 34.378, lon: 136.518, mag: 2.6, depth: 18, place: '三重県中部', intensity: '1', time: '2026-04-02T15:08:00+09:00' },
    { lat: 26.312, lon: 127.772, mag: 4.0, depth: 30, place: '沖縄本島近海', intensity: '2', time: '2026-04-02T12:42:00+09:00' },
  ];

  return quakes.map((q, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [q.lon, q.lat] },
    properties: {
      earthquake_id: `seed_eq_${i + 1}`,
      magnitude: q.mag,
      depth_km: q.depth,
      max_intensity: q.intensity,
      timestamp: q.time,
      place: q.place,
      tsunami_warning: false,
      source: 'jma_seed',
    },
  }));
}

export default async function collectJmaEarthquake() {
  let features = [];
  let source = 'jma_live';

  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(API_URL, { signal: controller.signal });
    clearTimeout(timer);

    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.json();
    features = parseLiveData(data);
    if (features.length === 0) throw new Error('No features parsed');
  } catch {
    features = getSeedData();
    source = 'jma_seed';
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Earthquake data from Japan Meteorological Agency',
    },
  };
}
