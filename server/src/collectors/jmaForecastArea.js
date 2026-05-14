/**
 * JMA regional forecast JSON
 * https://www.jma.go.jp/bosai/forecast/data/forecast/{areaCode}.json
 * Iterates a short list of prefecture codes and returns a representative point per area.
 */

const AREAS = [
  { code: '016000', name: '北海道・石狩', lat: 43.06, lon: 141.35 },
  { code: '040000', name: '宮城県', lat: 38.27, lon: 140.87 },
  { code: '130000', name: '東京都', lat: 35.69, lon: 139.69 },
  { code: '140000', name: '神奈川県', lat: 35.45, lon: 139.64 },
  { code: '230000', name: '愛知県', lat: 35.18, lon: 136.91 },
  { code: '270000', name: '大阪府', lat: 34.69, lon: 135.50 },
  { code: '280000', name: '兵庫県', lat: 34.69, lon: 135.18 },
  { code: '340000', name: '広島県', lat: 34.40, lon: 132.46 },
  { code: '400000', name: '福岡県', lat: 33.59, lon: 130.40 },
  { code: '471000', name: '沖縄・本島', lat: 26.21, lon: 127.68 },
];
const TIMEOUT_MS = 8000;

async function fetchOne(area) {
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(`https://www.jma.go.jp/bosai/forecast/data/forecast/${area.code}.json`, { signal: controller.signal });
    clearTimeout(timer);
    if (!res.ok) return null;
    const arr = await res.json();
    const first = Array.isArray(arr) ? arr[0] : null;
    const weather = first?.timeSeries?.[0]?.areas?.[0]?.weathers?.[0] ?? null;
    const wind = first?.timeSeries?.[0]?.areas?.[0]?.winds?.[0] ?? null;
    return { area, weather, wind, report_at: first?.reportDatetime ?? null };
  } catch {
    return null;
  }
}

export default async function collectJmaForecastArea() {
  const results = await Promise.all(AREAS.map(fetchOne));
  let source = 'live';
  let features = [];
  for (let i = 0; i < results.length; i++) {
    const r = results[i];
    const a = AREAS[i];
    if (r?.weather) {
      features.push({
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [a.lon, a.lat] },
        properties: {
          area_code: a.code,
          area_name: a.name,
          weather: r.weather,
          wind: r.wind,
          report_at: r.report_at,
          source: 'jma_forecast',
        },
      });
    }
  }
  if (features.length === 0) {
    source = 'seed';
    features = AREAS.map(a => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: [a.lon, a.lat] },
      properties: { area_code: a.code, area_name: a.name, weather: 'くもり時々晴れ', source: 'jma_forecast_seed' },
    }));
  }
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source,
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      description: 'JMA regional weather forecast per prefecture',
    },
  };
}
