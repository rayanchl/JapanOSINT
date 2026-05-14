/**
 * JMA Ocean Wave Collector
 *
 * Real upstream (scraped from the bosai/wave web app):
 *   - Stations:  https://www.jma.go.jp/bosai/wave/const/pointinfo.json
 *   - Per-stn:   https://www.jma.go.jp/bosai/wave/data/swjp/<number>.json
 *
 * Per-station JSON is a chronological array of hourly readings:
 *   { id, date (UTC), height (cm), period (s) }
 *
 * Falls back to a small seed of major coastal buoys if the JMA endpoints
 * fail (e.g. JMA reorganises the bosai pages again).
 */

const POINTINFO_URL = 'https://www.jma.go.jp/bosai/wave/const/pointinfo.json';
const PER_STATION_URL = (n) => `https://www.jma.go.jp/bosai/wave/data/swjp/${n}.json`;

const SEED_WAVE_BUOYS = [
  { name: '釧路沖', lat: 42.7000, lon: 144.7000, height_m: 2.1, period_s: 7.5, region: 'Pacific North' },
  { name: '銚子沖', lat: 35.7000, lon: 140.9500, height_m: 1.7, period_s: 6.8, region: 'Pacific Central' },
  { name: '潮岬沖', lat: 33.4000, lon: 135.7500, height_m: 1.8, period_s: 7.0, region: 'Pacific Central' },
  { name: '室戸沖', lat: 33.2000, lon: 134.1500, height_m: 1.9, period_s: 7.2, region: 'Pacific Central' },
  { name: '稚内沖', lat: 45.5000, lon: 141.5000, height_m: 1.4, period_s: 6.0, region: 'Japan Sea North' },
  { name: '対馬海峡', lat: 34.2000, lon: 129.5000, height_m: 1.4, period_s: 5.8, region: 'Japan Sea West' },
  { name: '奄美大島', lat: 28.4000, lon: 129.5000, height_m: 1.8, period_s: 7.0, region: 'Nansei' },
  { name: '沖縄本島東', lat: 26.5000, lon: 128.3000, height_m: 1.7, period_s: 7.0, region: 'Okinawa' },
];

async function fetchJsonShort(url, timeoutMs = 8000) {
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), timeoutMs);
    const res = await fetch(url, { signal: ctrl.signal });
    clearTimeout(t);
    if (!res.ok) return null;
    return await res.json();
  } catch { return null; }
}

async function tryJmaWave() {
  const stations = await fetchJsonShort(POINTINFO_URL);
  if (!Array.isArray(stations) || !stations.length) return null;

  const results = await Promise.all(stations.map(async (s) => {
    const series = await fetchJsonShort(PER_STATION_URL(s.number));
    if (!Array.isArray(series) || !series.length) return null;
    // Last entry with a finite height — JMA pads recent rows with nulls
    // for missing observations; walk backwards to find the latest valid one.
    let latest = null;
    for (let i = series.length - 1; i >= 0; i--) {
      const r = series[i];
      if (r && Number.isFinite(r.height)) { latest = r; break; }
    }
    if (!latest) return null;
    const [lat, lon] = Array.isArray(s.latlon) ? s.latlon : [null, null];
    const geocoded = Number.isFinite(lat) && Number.isFinite(lon);
    return {
      type: 'Feature',
      geometry: geocoded ? { type: 'Point', coordinates: [lon, lat] } : null,
      properties: {
        buoy_id: `JMA_WAVE_${s.number}`,
        name: s.name,
        wave_height_m: Math.round(latest.height) / 100, // cm → m
        period_s: latest.period ?? null,
        observed_at: latest.date,
        country: 'JP',
        source: 'jma_bosai_wave',
      },
    };
  }));
  const live = results.filter(Boolean);
  return live.length > 0 ? live : null;
}

function generateSeedData() {
  const now = new Date();
  return SEED_WAVE_BUOYS.map((b, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [b.lon, b.lat] },
    properties: {
      buoy_id: `WAV_${String(i + 1).padStart(5, '0')}`,
      name: b.name,
      wave_height_m: b.height_m,
      period_s: b.period_s,
      region: b.region,
      country: 'JP',
      observed_at: now.toISOString(),
      source: 'jma_wave_seed',
    },
  }));
}

export default async function collectJmaOceanWave() {
  let features = await tryJmaWave();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'jma_ocean_wave',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'jma_bosai_wave' : 'jma_wave_seed',
      description: 'JMA significant wave height observations (bosai/wave) — hourly per-station scraped',
    },
  };
}
