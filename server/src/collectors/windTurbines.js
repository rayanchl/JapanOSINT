/**
 * Wind Turbines Collector
 * OSM Overpass `power=generator generator:source=wind` for wind farms
 * across Japan, with curated seed of major onshore + offshore farms.
 */

const OVERPASS_URL = 'https://overpass-api.de/api/interpreter';

const SEED_WIND = [
  // Hokkaido — windiest region
  { name: '苫前ウインビラ発電所', lat: 44.3000, lon: 141.6500, capacity_mw: 30.6, turbines: 19, kind: 'onshore' },
  { name: 'ユーラスエナジー 苫前ウィンドファーム', lat: 44.3072, lon: 141.6644, capacity_mw: 30.6, turbines: 19, kind: 'onshore' },
  { name: '宗谷岬ウィンドファーム', lat: 45.5000, lon: 141.9333, capacity_mw: 57.0, turbines: 57, kind: 'onshore' },
  { name: '稚内 オロロン風力', lat: 45.4083, lon: 141.7333, capacity_mw: 6.0, turbines: 4, kind: 'onshore' },
  { name: 'さらきとまない発電所', lat: 45.0000, lon: 141.6889, capacity_mw: 14.85, turbines: 9, kind: 'onshore' },
  { name: '島牧ウィンドファーム', lat: 42.7333, lon: 140.1167, capacity_mw: 33.0, turbines: 11, kind: 'onshore' },
  { name: '瀬棚臨海風力発電所 (洋上)', lat: 42.4500, lon: 139.8500, capacity_mw: 4.0, turbines: 2, kind: 'offshore' },
  { name: '石狩湾新港洋上ウィンド', lat: 43.2167, lon: 141.2667, capacity_mw: 112.0, turbines: 14, kind: 'offshore' },
  // Tohoku
  { name: '能代風力発電所', lat: 40.2122, lon: 139.9258, capacity_mw: 35.7, turbines: 17, kind: 'onshore' },
  { name: '青山高原ウィンドファーム', lat: 34.6833, lon: 136.3500, capacity_mw: 95.0, turbines: 40, kind: 'onshore' },
  { name: '新出雲ウィンドファーム', lat: 35.4500, lon: 132.6833, capacity_mw: 78.0, turbines: 26, kind: 'onshore' },
  { name: '岩屋ウィンドファーム (六ヶ所)', lat: 41.0167, lon: 141.3667, capacity_mw: 32.5, turbines: 15, kind: 'onshore' },
  { name: '釜石広域風力発電所', lat: 39.2500, lon: 141.7167, capacity_mw: 42.9, turbines: 13, kind: 'onshore' },
  { name: '会津若松ウィンドファーム', lat: 37.5000, lon: 139.9300, capacity_mw: 16.0, turbines: 8, kind: 'onshore' },
  { name: '酒田風力発電所', lat: 38.9167, lon: 139.8500, capacity_mw: 16.5, turbines: 5, kind: 'onshore' },
  { name: '秋田港・能代港洋上ウィンド', lat: 39.7500, lon: 140.0500, capacity_mw: 140.0, turbines: 33, kind: 'offshore' },
  // Kanto / Chubu
  { name: '銚子洋上風力発電所', lat: 35.7167, lon: 140.8833, capacity_mw: 2.4, turbines: 1, kind: 'offshore' },
  { name: '鹿島風力発電所 (海岸)', lat: 35.9667, lon: 140.6833, capacity_mw: 9.95, turbines: 8, kind: 'onshore' },
  { name: '神栖風力発電所', lat: 35.8000, lon: 140.7833, capacity_mw: 14.0, turbines: 7, kind: 'onshore' },
  { name: '波崎ウィンドファーム', lat: 35.7333, lon: 140.8500, capacity_mw: 15.0, turbines: 10, kind: 'onshore' },
  { name: '伊豆半島東伊豆ウィンドファーム', lat: 34.7667, lon: 139.0500, capacity_mw: 18.4, turbines: 8, kind: 'onshore' },
  { name: '御前崎風力発電所', lat: 34.6017, lon: 138.2336, capacity_mw: 8.0, turbines: 4, kind: 'onshore' },
  { name: '津軽風力発電所', lat: 41.0500, lon: 140.4500, capacity_mw: 121.6, turbines: 38, kind: 'onshore' },
  // Kansai / Chugoku
  { name: '淡路風力発電所', lat: 34.6500, lon: 134.8500, capacity_mw: 12.0, turbines: 6, kind: 'onshore' },
  { name: '南あわじ風力発電所', lat: 34.3000, lon: 134.7500, capacity_mw: 5.0, turbines: 5, kind: 'onshore' },
  { name: '島根太陽風力発電所', lat: 35.0833, lon: 132.4833, capacity_mw: 3.0, turbines: 2, kind: 'onshore' },
  { name: '高戸山風力発電所', lat: 33.7833, lon: 132.2667, capacity_mw: 20.0, turbines: 10, kind: 'onshore' },
  { name: '岡山日生風力発電所', lat: 34.7333, lon: 134.2667, capacity_mw: 6.0, turbines: 3, kind: 'onshore' },
  // Kyushu
  { name: '鹿児島輝北ウィンドファーム', lat: 31.6167, lon: 130.9333, capacity_mw: 18.0, turbines: 9, kind: 'onshore' },
  { name: '長崎五島浮体式洋上風力', lat: 32.6500, lon: 128.7833, capacity_mw: 16.8, turbines: 8, kind: 'offshore_floating' },
  { name: '北九州響灘洋上ウィンドファーム', lat: 33.9333, lon: 130.8167, capacity_mw: 15.0, turbines: 5, kind: 'offshore' },
  { name: '長島風力発電所', lat: 32.1833, lon: 130.1667, capacity_mw: 50.4, turbines: 21, kind: 'onshore' },
  { name: '阿蘇高森ウィンドファーム', lat: 32.8500, lon: 131.1167, capacity_mw: 6.0, turbines: 3, kind: 'onshore' },
  { name: '佐多岬ウィンドファーム', lat: 30.9889, lon: 130.6606, capacity_mw: 10.0, turbines: 5, kind: 'onshore' },
];

async function tryOverpass() {
  const query = `[out:json][timeout:180];area["ISO3166-1"="JP"]->.jp;(node["power"="generator"]["generator:source"="wind"](area.jp);way["power"="generator"]["generator:source"="wind"](area.jp););out center;`;
  try {
    const ctrl = new AbortController();
    const timeout = setTimeout(() => ctrl.abort(), 12000);
    const res = await fetch(OVERPASS_URL, {
      method: 'POST',
      signal: ctrl.signal,
      headers: { 'Content-Type': 'text/plain' },
      body: query,
    });
    clearTimeout(timeout);
    if (!res.ok) return null;
    const data = await res.json();
    if (!data?.elements?.length) return null;
    return data.elements
      .map((el) => {
        const lat = el.center?.lat ?? el.lat;
        const lon = el.center?.lon ?? el.lon;
        if (lat == null || lon == null) return null;
        const out = parseFloat(el.tags?.['generator:output:electricity'] || '0');
        return {
          type: 'Feature',
          geometry: { type: 'Point', coordinates: [lon, lat] },
          properties: {
            turbine_id: `OSM_${el.id}`,
            name: el.tags?.name || 'Wind Turbine',
            capacity_mw: out / 1000000,
            source: 'osm_overpass',
          },
        };
      })
      .filter(Boolean);
  } catch {
    return null;
  }
}

function generateSeedData() {
  return SEED_WIND.map((w, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [w.lon, w.lat] },
    properties: {
      farm_id: `WIND_${String(i + 1).padStart(5, '0')}`,
      name: w.name,
      capacity_mw: w.capacity_mw,
      turbines: w.turbines,
      kind: w.kind,
      country: 'JP',
      source: 'wind_seed',
    },
  }));
}

export default async function collectWindTurbines() {
  let features = await tryOverpass();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'wind_turbines',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Japanese wind farms: onshore, fixed offshore, and floating offshore (Goto)',
    },
    metadata: {},
  };
}
