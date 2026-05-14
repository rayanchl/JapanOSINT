/**
 * JMA Tide Observation Collector
 *
 * Real upstream (scraped from the bosai/tidelevel web app):
 *   - Stations:    https://www.jma.go.jp/bosai/tidelevel/const/tide_area.json
 *                  Maps class20 area code → list of class30 ports → list of
 *                  stations with name, lat, lon, max-recorded level, etc.
 *   - Estimates:   https://www.jma.go.jp/bosai/tidelevel/data/tide/tide_estimate.json
 *                  Keyed on `<class20Code>_<stationCode>` → { estimate, astro }
 *                  arrays (cm). `estimate` is recent observed levels; `astro`
 *                  is the astronomical-tide prediction series.
 *
 * Falls back to a curated seed of major tide gauges if the JMA endpoints
 * fail.
 */

const TIDE_AREA_URL = 'https://www.jma.go.jp/bosai/tidelevel/const/tide_area.json';
const TIDE_ESTIMATE_URL = 'https://www.jma.go.jp/bosai/tidelevel/data/tide/tide_estimate.json';

const SEED_TIDE_STATIONS = [
  { name: '稚内', lat: 45.4082, lon: 141.6864, level_cm: 105, anomaly_cm: 5, region: 'Hokkaido' },
  { name: '函館', lat: 41.7775, lon: 140.7286, level_cm: 95, anomaly_cm: 4, region: 'Hokkaido' },
  { name: '宮古', lat: 39.6447, lon: 141.9711, level_cm: 110, anomaly_cm: 8, region: 'Tohoku' },
  { name: '銚子', lat: 35.7406, lon: 140.8689, level_cm: 92, anomaly_cm: 3, region: 'Kanto' },
  { name: '東京晴海', lat: 35.6500, lon: 139.7700, level_cm: 105, anomaly_cm: 5, region: 'Kanto' },
  { name: '名古屋', lat: 35.0917, lon: 136.8806, level_cm: 110, anomaly_cm: 8, region: 'Tokai' },
  { name: '潮岬', lat: 33.4500, lon: 135.7600, level_cm: 92, anomaly_cm: 3, region: 'Kansai' },
  { name: '神戸', lat: 34.6833, lon: 135.1833, level_cm: 105, anomaly_cm: 6, region: 'Kansai' },
  { name: '高知', lat: 33.5089, lon: 133.5694, level_cm: 100, anomaly_cm: 5, region: 'Shikoku' },
  { name: '長崎', lat: 32.7497, lon: 129.8775, level_cm: 110, anomaly_cm: 6, region: 'Kyushu' },
  { name: '那覇', lat: 26.2125, lon: 127.6809, level_cm: 95, anomaly_cm: 3, region: 'Okinawa' },
];

async function fetchJsonShort(url, timeoutMs = 10000) {
  try {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), timeoutMs);
    const res = await fetch(url, { signal: ctrl.signal });
    clearTimeout(t);
    if (!res.ok) return null;
    return await res.json();
  } catch { return null; }
}

// Pull the latest non-null observed level (cm) and matching astronomical
// prediction so we can report a meaningful anomaly. JMA's `estimate` can
// have leading null entries for hours not yet reported.
function latestObserved(estimate, astro) {
  if (!Array.isArray(estimate)) return null;
  for (let i = estimate.length - 1; i >= 0; i--) {
    const v = estimate[i];
    if (Number.isFinite(v)) {
      // Astro array spans the day; pick the same hour-of-day index.
      // estimate[] holds the most-recent N hours; the array length tells
      // us how many hours back we are. We map index i to the matching
      // astro slot using the offset from the end of estimate.
      const astroIdx = Array.isArray(astro)
        ? astro.length - (estimate.length - 1 - i) - 1
        : -1;
      const astroVal = astroIdx >= 0 && astroIdx < (astro?.length || 0)
        ? astro[astroIdx]
        : null;
      return {
        level_cm: v,
        astro_cm: Number.isFinite(astroVal) ? astroVal : null,
        hours_ago: estimate.length - 1 - i,
      };
    }
  }
  return null;
}

async function tryJmaTide() {
  const [areas, estimates] = await Promise.all([
    fetchJsonShort(TIDE_AREA_URL),
    fetchJsonShort(TIDE_ESTIMATE_URL),
  ]);
  if (!areas || !estimates || !estimates.areas) return null;

  const features = [];
  // tide_area.json is keyed by class20 area code → { name, class30s: [{ stations:[...] }, ...] }
  for (const [class20Code, area] of Object.entries(areas)) {
    if (!area?.class30s) continue;
    for (const class30 of area.class30s) {
      // The tide_estimate.json keys are `<class20Code>_<class30Code>` —
      // one estimate stream per class30 group, not per individual station.
      const estKey = `${class20Code}_${class30.code}`;
      const est = estimates.areas[estKey];
      const obs = est ? latestObserved(est.estimate, est.astro) : null;

      for (const station of class30?.stations || []) {
        const lat = Number(station.lat);
        const lon = Number(station.lon);
        const geocoded = Number.isFinite(lat) && Number.isFinite(lon);

        features.push({
          type: 'Feature',
          geometry: geocoded ? { type: 'Point', coordinates: [lon, lat] } : null,
          properties: {
            station_id: `JMA_TIDE_${station.code}`,
            name: station.name || `Station ${station.code}`,
            level_cm: obs ? obs.level_cm : null,
            astro_cm: obs ? obs.astro_cm : null,
            anomaly_cm: obs && obs.astro_cm != null ? obs.level_cm - obs.astro_cm : null,
            hours_ago: obs ? obs.hours_ago : null,
            class20_name: area.name || null,
            address: station.addr || null,
            reference: station.reference || null,
            operator_type: station.typeName || station.type || null,
            country: 'JP',
            observed_at: estimates.time || null,
            source: 'jma_bosai_tidelevel',
          },
        });
      }
    }
  }
  return features.length > 0 ? features : null;
}

function generateSeedData() {
  const now = new Date();
  return SEED_TIDE_STATIONS.map((s, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [s.lon, s.lat] },
    properties: {
      station_id: `TIDE_${String(i + 1).padStart(5, '0')}`,
      name: s.name,
      level_cm: s.level_cm,
      anomaly_cm: s.anomaly_cm,
      region: s.region,
      country: 'JP',
      observed_at: now.toISOString(),
      source: 'jma_tide_seed',
    },
  }));
}

export default async function collectJmaTide() {
  let features = await tryJmaTide();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'jma_tide',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'jma_bosai_tidelevel' : 'jma_tide_seed',
      description: 'JMA tide gauge stations + latest observed level vs astronomical prediction',
    },
  };
}
