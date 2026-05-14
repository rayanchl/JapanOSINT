/**
 * Open-Meteo JMA mirror - free, anonymous
 * https://api.open-meteo.com/v1/jma
 */

import { fetchJson } from './_liveHelpers.js';

const CITIES = [
  { name: 'Sapporo', lat: 43.06, lon: 141.35 },
  { name: 'Sendai', lat: 38.27, lon: 140.87 },
  { name: 'Tokyo', lat: 35.69, lon: 139.69 },
  { name: 'Yokohama', lat: 35.45, lon: 139.64 },
  { name: 'Nagoya', lat: 35.18, lon: 136.91 },
  { name: 'Osaka', lat: 34.69, lon: 135.50 },
  { name: 'Hiroshima', lat: 34.40, lon: 132.46 },
  { name: 'Fukuoka', lat: 33.59, lon: 130.40 },
  { name: 'Naha', lat: 26.21, lon: 127.68 },
];
const TIMEOUT_MS = 8000;

async function fetchCity(c) {
  const url = `https://api.open-meteo.com/v1/jma?latitude=${c.lat}&longitude=${c.lon}&current=temperature_2m,wind_speed_10m,precipitation`;
  const d = await fetchJson(url, { timeoutMs: TIMEOUT_MS });
  return d?.current ?? null;
}

export default async function collectOpenmeteoJma() {
  const out = await Promise.all(CITIES.map(fetchCity));
  let source = 'live';
  const features = [];
  for (let i = 0; i < CITIES.length; i++) {
    const c = CITIES[i];
    const cur = out[i];
    if (cur) {
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [c.lon, c.lat] },
        properties: {
          city: c.name,
          temperature_c: cur.temperature_2m ?? null,
          wind_mps: cur.wind_speed_10m ?? null,
          precipitation_mm: cur.precipitation ?? null,
          observed_at: cur.time ?? null,
          source: 'open_meteo_jma',
        },
      });
    }
  }
  if (features.length === 0) {
    source = 'seed';
    for (const c of CITIES) {
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [c.lon, c.lat] },
        properties: { city: c.name, temperature_c: 15, wind_mps: 3, precipitation_mm: 0, source: 'open_meteo_seed' },
      });
    }
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Open-Meteo JMA free weather mirror',
    },
  };
}
