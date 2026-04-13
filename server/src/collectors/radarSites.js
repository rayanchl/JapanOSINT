/**
 * Radar Sites Collector
 * BMD/AEW radar installations across Japan: ASDF JADGE radar sites,
 * BMD X-band, plus weather radar.
 */

const OVERPASS_URL = 'https://overpass-api.de/api/interpreter';

const SEED_RADAR = [
  // ASDF JADGE FPS-3/FPS-7 sites (long-range air surveillance)
  { name: '稚内分屯基地 FPS-7', lat: 45.4083, lon: 141.7333, type: 'air_defense', system: 'FPS-7', branch: 'ASDF' },
  { name: '網走分屯基地 FPS-3', lat: 43.9914, lon: 144.2706, type: 'air_defense', system: 'FPS-3', branch: 'ASDF' },
  { name: '当別分屯基地 FPS-7', lat: 43.2364, lon: 141.5417, type: 'air_defense', system: 'FPS-7', branch: 'ASDF' },
  { name: '根室分屯基地 FPS-3', lat: 43.3306, lon: 145.6217, type: 'air_defense', system: 'FPS-3', branch: 'ASDF' },
  { name: '襟裳分屯基地 FPS-3', lat: 41.9358, lon: 143.2467, type: 'air_defense', system: 'FPS-3', branch: 'ASDF' },
  { name: '奥尻島分屯基地 FPS-2', lat: 42.1700, lon: 139.5067, type: 'air_defense', system: 'FPS-2', branch: 'ASDF' },
  { name: '大湊分屯基地 FPS-3', lat: 41.2403, lon: 141.1325, type: 'air_defense', system: 'FPS-3', branch: 'ASDF' },
  { name: '車力分屯基地 AN/TPY-2', lat: 40.9883, lon: 140.3208, type: 'bmd_xband', system: 'AN/TPY-2', branch: 'USAF' },
  { name: '加茂分屯基地 FPS-7', lat: 38.4806, lon: 139.6047, type: 'air_defense', system: 'FPS-7', branch: 'ASDF' },
  { name: '佐渡分屯基地 FPS-3', lat: 38.0739, lon: 138.4153, type: 'air_defense', system: 'FPS-3', branch: 'ASDF' },
  { name: '輪島分屯基地 FPS-7', lat: 37.4233, lon: 136.8983, type: 'air_defense', system: 'FPS-7', branch: 'ASDF' },
  { name: '御前崎分屯基地 FPS-3', lat: 34.6017, lon: 138.2336, type: 'air_defense', system: 'FPS-3', branch: 'ASDF' },
  { name: '笠取山分屯基地 FPS-7', lat: 34.4708, lon: 136.2747, type: 'air_defense', system: 'FPS-7', branch: 'ASDF' },
  { name: '串本分屯基地 FPS-3', lat: 33.4761, lon: 135.7794, type: 'air_defense', system: 'FPS-3', branch: 'ASDF' },
  { name: '経ヶ岬分屯基地 AN/TPY-2', lat: 35.7700, lon: 135.2550, type: 'bmd_xband', system: 'AN/TPY-2', branch: 'USAF' },
  { name: '海栗島分屯基地 FPS-3', lat: 34.4856, lon: 129.4267, type: 'air_defense', system: 'FPS-3', branch: 'ASDF' },
  { name: '背振山分屯基地 FPS-7', lat: 33.4292, lon: 130.3650, type: 'air_defense', system: 'FPS-7', branch: 'ASDF' },
  { name: '高畑山分屯基地 FPS-2', lat: 32.0925, lon: 131.5400, type: 'air_defense', system: 'FPS-2', branch: 'ASDF' },
  { name: '福江島分屯基地 FPS-3', lat: 32.7233, lon: 128.7050, type: 'air_defense', system: 'FPS-3', branch: 'ASDF' },
  { name: '下甑島分屯基地 FPS-3', lat: 31.7194, lon: 129.7475, type: 'air_defense', system: 'FPS-3', branch: 'ASDF' },
  { name: '奄美大島分屯基地 FPS-3', lat: 28.4286, lon: 129.6981, type: 'air_defense', system: 'FPS-3', branch: 'ASDF' },
  { name: '沖永良部分屯基地 FPS-3', lat: 27.4053, lon: 128.6500, type: 'air_defense', system: 'FPS-3', branch: 'ASDF' },
  { name: '与座岳分屯基地 FPS-7', lat: 26.1183, lon: 127.7311, type: 'air_defense', system: 'FPS-7', branch: 'ASDF' },
  { name: '宮古島分屯基地 FPS-7', lat: 24.7825, lon: 125.3261, type: 'air_defense', system: 'FPS-7', branch: 'ASDF' },
  { name: '久米島分屯基地 FPS-3', lat: 26.3675, lon: 126.7142, type: 'air_defense', system: 'FPS-3', branch: 'ASDF' },
  { name: '与那国島分屯基地 移動式', lat: 24.4500, lon: 122.9667, type: 'air_defense', system: 'mobile', branch: 'ASDF' },
  // JMA Doppler weather radar (selected)
  { name: '札幌気象レーダー', lat: 43.0150, lon: 141.0114, type: 'weather', system: 'Doppler-C', branch: 'JMA' },
  { name: '釧路気象レーダー', lat: 42.9858, lon: 144.3947, type: 'weather', system: 'Doppler-C', branch: 'JMA' },
  { name: '函館気象レーダー', lat: 41.8217, lon: 140.7569, type: 'weather', system: 'Doppler-C', branch: 'JMA' },
  { name: '仙台気象レーダー', lat: 38.2697, lon: 140.8956, type: 'weather', system: 'Doppler-C', branch: 'JMA' },
  { name: '東京気象レーダー (柏)', lat: 35.8636, lon: 139.9697, type: 'weather', system: 'Doppler-C', branch: 'JMA' },
  { name: '名古屋気象レーダー', lat: 35.1658, lon: 136.9742, type: 'weather', system: 'Doppler-C', branch: 'JMA' },
  { name: '新潟気象レーダー', lat: 37.8956, lon: 139.0181, type: 'weather', system: 'Doppler-C', branch: 'JMA' },
  { name: '大阪気象レーダー (高安山)', lat: 34.6361, lon: 135.6478, type: 'weather', system: 'Doppler-C', branch: 'JMA' },
  { name: '広島気象レーダー', lat: 34.3958, lon: 132.4567, type: 'weather', system: 'Doppler-C', branch: 'JMA' },
  { name: '高松気象レーダー', lat: 34.3389, lon: 134.0481, type: 'weather', system: 'Doppler-C', branch: 'JMA' },
  { name: '福岡気象レーダー (せふり山)', lat: 33.4264, lon: 130.3683, type: 'weather', system: 'Doppler-C', branch: 'JMA' },
  { name: '鹿児島気象レーダー', lat: 31.5556, lon: 130.5450, type: 'weather', system: 'Doppler-C', branch: 'JMA' },
  { name: '名瀬気象レーダー', lat: 28.3786, lon: 129.4933, type: 'weather', system: 'Doppler-C', branch: 'JMA' },
  { name: '沖縄気象レーダー (糸数)', lat: 26.1450, lon: 127.7642, type: 'weather', system: 'Doppler-C', branch: 'JMA' },
  { name: '石垣気象レーダー', lat: 24.3400, lon: 124.1700, type: 'weather', system: 'Doppler-C', branch: 'JMA' },
];

async function tryOverpass() {
  const query = `[out:json][timeout:180];area["ISO3166-1"="JP"]->.jp;(node["man_made"="radar"](area.jp);way["man_made"="radar"](area.jp););out center;`;
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
        return {
          type: 'Feature',
          geometry: { type: 'Point', coordinates: [lon, lat] },
          properties: {
            radar_id: `OSM_${el.id}`,
            name: el.tags?.name || 'Radar Site',
            type: 'radar',
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
  return SEED_RADAR.map((r, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [r.lon, r.lat] },
    properties: {
      radar_id: `RDR_${String(i + 1).padStart(5, '0')}`,
      name: r.name,
      type: r.type,
      system: r.system,
      branch: r.branch,
      source: 'radar_seed',
    },
  }));
}

export default async function collectRadarSites() {
  let features = await tryOverpass();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'radar_sites',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'JADGE air-defense radar, BMD X-band (AN/TPY-2), and JMA weather radar',
    },
    metadata: {},
  };
}
