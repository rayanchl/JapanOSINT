/**
 * Windy.com Japan weather collector (windy-japan).
 *
 * Live: Windy Point Forecast API (https://api.windy.com/api/point-forecast/v2)
 * queried over a sparse grid of major Japanese cities when WINDY_API_KEY is
 * set. There is no key-free Windy endpoint, so without a key (or on failure)
 * this returns an honest EMPTY FeatureCollection — never fabricated points.
 * The missing-key state surfaces via the apiCredentials map.
 */

import { envFor } from '../utils/collectorEnv.js';

const POINT_FORECAST_URL = 'https://api.windy.com/api/point-forecast/v2';
const TIMEOUT_MS = 15000;

// Sparse representative grid across Japan (kept small to respect Windy quota).
const GRID = [
  { name: 'Sapporo', lat: 43.06, lon: 141.35 },
  { name: 'Sendai', lat: 38.27, lon: 140.87 },
  { name: 'Tokyo', lat: 35.69, lon: 139.69 },
  { name: 'Nagoya', lat: 35.18, lon: 136.91 },
  { name: 'Osaka', lat: 34.69, lon: 135.50 },
  { name: 'Hiroshima', lat: 34.40, lon: 132.46 },
  { name: 'Fukuoka', lat: 33.59, lon: 130.40 },
  { name: 'Naha', lat: 26.21, lon: 127.68 },
];

function result(features, source) {
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'Windy.com point forecast over Japanese cities (requires WINDY_API_KEY)',
    },
  };
}

async function fetchPoint(key, p) {
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), TIMEOUT_MS);
    const res = await fetch(POINT_FORECAST_URL, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        lat: p.lat,
        lon: p.lon,
        model: 'gfs',
        parameters: ['temp', 'wind', 'pressure'],
        levels: ['surface'],
        key,
      }),
      signal: ctrl.signal,
    });
    clearTimeout(timer);
    if (!res.ok) return null;
    const data = await res.json();
    if (!data || data.error || !data.ts || !Array.isArray(data.ts) || data.ts.length === 0) return null;
    const tK = data['temp-surface']?.[0];
    const u = data['wind_u-surface']?.[0];
    const v = data['wind_v-surface']?.[0];
    const press = data['pressure-surface']?.[0];
    const windMs = (u != null && v != null) ? Math.sqrt(u * u + v * v) : null;
    return {
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [p.lon, p.lat] },
      properties: {
        city: p.name,
        forecast_time: new Date(data.ts[0]).toISOString(),
        temp_c: tK != null ? Math.round((tK - 273.15) * 10) / 10 : null,
        wind_ms: windMs != null ? Math.round(windMs * 10) / 10 : null,
        pressure_pa: press ?? null,
        model: 'gfs',
        source: 'windy_point_forecast',
      },
    };
  } catch {
    return null;
  }
}

export default async function collectWindyJapan() {
  const key = envFor('WINDY_API_KEY');
  if (!key) return result([], 'windy_no_key');

  const settled = await Promise.all(GRID.map((p) => fetchPoint(key, p)));
  const features = settled.filter(Boolean);
  return result(features, features.length ? 'windy_point_forecast' : 'windy_unavailable');
}
