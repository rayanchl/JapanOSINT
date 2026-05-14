/**
 * JMA Seismic Intensity Collector
 * Fetches recent earthquake intensity reports from JMA bosai feed.
 * Falls back to a curated seed of recent significant intensity events.
 */

const JMA_QUAKE_LIST = 'https://www.jma.go.jp/bosai/quake/data/list.json';

const SEED_INTENSITY = [
  { name: '能登半島地震 珠洲市', lat: 37.4283, lon: 137.2614, magnitude: 7.6, intensity: '7', date: '2024-01-01', prefecture: '石川県' },
  { name: '能登半島地震 輪島市', lat: 37.3919, lon: 136.8989, magnitude: 7.6, intensity: '7', date: '2024-01-01', prefecture: '石川県' },
  { name: '福島県沖地震', lat: 37.7050, lon: 141.5897, magnitude: 7.4, intensity: '6+', date: '2022-03-16', prefecture: '福島県' },
  { name: '熊本地震 益城町', lat: 32.7867, lon: 130.8133, magnitude: 7.3, intensity: '7', date: '2016-04-16', prefecture: '熊本県' },
  { name: '東日本大震災 宮城沖', lat: 38.3220, lon: 142.3690, magnitude: 9.1, intensity: '7', date: '2011-03-11', prefecture: '宮城県' },
  { name: '北海道胆振東部地震', lat: 42.6906, lon: 142.0072, magnitude: 6.7, intensity: '7', date: '2018-09-06', prefecture: '北海道' },
  { name: '大阪府北部地震', lat: 34.8442, lon: 135.6219, magnitude: 6.1, intensity: '6-', date: '2018-06-18', prefecture: '大阪府' },
  { name: '長野県北部地震', lat: 36.7000, lon: 138.5833, magnitude: 6.7, intensity: '6+', date: '2014-11-22', prefecture: '長野県' },
  { name: '新潟県中越地震', lat: 37.3072, lon: 138.8347, magnitude: 6.8, intensity: '7', date: '2004-10-23', prefecture: '新潟県' },
  { name: '岩手・宮城内陸地震', lat: 39.0264, lon: 140.8800, magnitude: 7.2, intensity: '6+', date: '2008-06-14', prefecture: '岩手県' },
  { name: '鳥取県中部地震', lat: 35.3811, lon: 133.8553, magnitude: 6.6, intensity: '6-', date: '2016-10-21', prefecture: '鳥取県' },
  { name: '島根県西部地震', lat: 35.1817, lon: 132.5936, magnitude: 6.1, intensity: '5+', date: '2018-04-09', prefecture: '島根県' },
  { name: '宮城県沖地震', lat: 38.2700, lon: 141.5000, magnitude: 6.9, intensity: '5+', date: '2021-03-20', prefecture: '宮城県' },
  { name: '紀伊半島南東沖地震', lat: 33.1458, lon: 136.6189, magnitude: 7.4, intensity: '5-', date: '2004-09-05', prefecture: '三重県' },
  { name: '十勝沖地震', lat: 41.7783, lon: 144.0792, magnitude: 8.0, intensity: '6-', date: '2003-09-26', prefecture: '北海道' },
];

function intensityToNumeric(s) {
  if (s == null) return null;
  const m = String(s).match(/(\d)([+-]?)/);
  if (!m) return null;
  const n = parseInt(m[1]);
  if (m[2] === '+') return n + 0.5;
  if (m[2] === '-') return n - 0.5;
  return n;
}

async function tryJma() {
  try {
    const ctrl = new AbortController();
    const timeout = setTimeout(() => ctrl.abort(), 10000);
    const res = await fetch(JMA_QUAKE_LIST, {
      signal: ctrl.signal,
      headers: { 'User-Agent': 'JapanOSINT/1.0' },
    });
    clearTimeout(timeout);
    if (!res.ok) return null;
    const data = await res.json();
    if (!Array.isArray(data) || data.length === 0) return null;
    return data.slice(0, 100).map((q, i) => {
      // JMA format: anm = name, mag, maxi = max intensity, ...
      return {
        type: 'Feature',
        geometry: { type: 'Point', coordinates: [parseFloat(q.cod?.split('+')?.[1]) || 139.0, parseFloat(q.cod?.split('+')?.[0]) || 36.0] },
        properties: {
          event_id: `JMA_INT_${String(i + 1).padStart(5, '0')}`,
          name: q.anm || 'Unknown',
          magnitude: parseFloat(q.mag) || null,
          intensity: q.maxi || null,
          intensity_numeric: intensityToNumeric(q.maxi),
          time: q.at || null,
          country: 'JP',
          source: 'jma_bosai',
        },
      };
    });
  } catch {
    return null;
  }
}

function generateSeedData() {
  const now = new Date();
  return SEED_INTENSITY.map((q, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [q.lon, q.lat] },
    properties: {
      event_id: `JMA_INT_${String(i + 1).padStart(5, '0')}`,
      name: q.name,
      magnitude: q.magnitude,
      intensity: q.intensity,
      intensity_numeric: intensityToNumeric(q.intensity),
      date: q.date,
      prefecture: q.prefecture,
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'jma_intensity_seed',
    },
  }));
}

export default async function collectJmaIntensity() {
  let features = await tryJma();
  const live = !!(features && features.length > 0);
  if (!live) features = [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'jma_intensity',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'JMA seismic intensity reports for recent significant earthquakes',
    },
  };
}
