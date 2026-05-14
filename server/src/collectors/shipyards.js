/**
 * Shipyards Collector
 * Major Japanese shipbuilders and dry-dock facilities.
 * Shipbuilders Assoc Japan list + OSM `industrial=shipyard`.
 */

import { fetchOverpass } from './_liveHelpers.js';

const SEED_SHIPYARDS = [
  // Imabari Shipbuilding (largest)
  { name: '今治造船 西条工場', lat: 33.9239, lon: 133.1817, company: 'Imabari', kind: 'commercial', max_dwt: 250000 },
  { name: '今治造船 丸亀事業本部', lat: 34.2922, lon: 133.7583, company: 'Imabari', kind: 'commercial', max_dwt: 200000 },
  { name: '今治造船 広島工場', lat: 34.2861, lon: 132.5664, company: 'Imabari', kind: 'commercial', max_dwt: 180000 },
  { name: '今治造船 多度津造船', lat: 34.2750, lon: 133.7500, company: 'Imabari', kind: 'commercial', max_dwt: 100000 },
  { name: '今治造船 岩城工場', lat: 34.2500, lon: 132.9667, company: 'Imabari', kind: 'commercial', max_dwt: 80000 },
  // JMU (Japan Marine United)
  { name: 'JMU 呉事業所', lat: 34.2406, lon: 132.5544, company: 'JMU', kind: 'commercial_navy', max_dwt: 320000 },
  { name: 'JMU 津事業所', lat: 34.7258, lon: 136.5447, company: 'JMU', kind: 'commercial', max_dwt: 200000 },
  { name: 'JMU 因島工場', lat: 34.3306, lon: 133.1786, company: 'JMU', kind: 'commercial', max_dwt: 150000 },
  { name: 'JMU 横浜工場 (磯子)', lat: 35.3917, lon: 139.6481, company: 'JMU', kind: 'commercial_navy', max_dwt: 200000 },
  { name: 'JMU 舞鶴事業所', lat: 35.4761, lon: 135.3917, company: 'JMU', kind: 'navy', max_dwt: 0 },
  // Mitsubishi Heavy Industries
  { name: '三菱重工 長崎造船所', lat: 32.7397, lon: 129.8514, company: 'MHI', kind: 'commercial_navy', max_dwt: 200000 },
  { name: '三菱重工 神戸造船所', lat: 34.6750, lon: 135.1842, company: 'MHI', kind: 'submarine', max_dwt: 0 },
  { name: '三菱重工 下関造船所', lat: 33.9522, lon: 130.9322, company: 'MHI', kind: 'commercial', max_dwt: 80000 },
  // Kawasaki Heavy Industries
  { name: '川崎重工 神戸工場', lat: 34.6833, lon: 135.1833, company: 'KHI', kind: 'submarine', max_dwt: 0 },
  { name: '川崎重工 坂出工場', lat: 34.3222, lon: 133.8531, company: 'KHI', kind: 'commercial', max_dwt: 200000 },
  // Mitsui E&S
  { name: '三井E&S 玉野艦船工場', lat: 34.4886, lon: 133.9650, company: 'Mitsui ES', kind: 'commercial_navy', max_dwt: 200000 },
  { name: '三井E&S 千葉事業所', lat: 35.6147, lon: 140.0297, company: 'Mitsui ES', kind: 'commercial', max_dwt: 150000 },
  // Sumitomo Heavy
  { name: '住友重機械 横須賀製造所', lat: 35.2522, lon: 139.6822, company: 'SHI', kind: 'commercial', max_dwt: 200000 },
  // Namura
  { name: '名村造船所 本社工場 (伊万里)', lat: 33.2622, lon: 129.9217, company: 'Namura', kind: 'commercial', max_dwt: 320000 },
  { name: '名村造船所 函館どつく', lat: 41.7825, lon: 140.7250, company: 'Namura', kind: 'commercial', max_dwt: 100000 },
  // Tsuneishi
  { name: '常石造船 本社工場', lat: 34.4011, lon: 133.4317, company: 'Tsuneishi', kind: 'commercial', max_dwt: 200000 },
  { name: '常石造船 多度津工場', lat: 34.2733, lon: 133.7456, company: 'Tsuneishi', kind: 'commercial', max_dwt: 100000 },
  // Onomichi
  { name: '尾道造船 本社工場', lat: 34.4044, lon: 133.2017, company: 'Onomichi', kind: 'commercial', max_dwt: 100000 },
  { name: '尾道造船 向島工場', lat: 34.3889, lon: 133.1917, company: 'Onomichi', kind: 'commercial', max_dwt: 80000 },
  // Sanoyas
  { name: '三井住友海上 (旧サノヤス) 水島造船', lat: 34.4992, lon: 133.7722, company: 'Sanoyas', kind: 'commercial', max_dwt: 100000 },
  // Mitsubishi
  { name: '三菱造船 下関工場', lat: 33.9533, lon: 130.9300, company: 'MHI', kind: 'commercial', max_dwt: 80000 },
  // Hakodate
  { name: '函館どつく 本社工場', lat: 41.7864, lon: 140.7228, company: 'Hakodate Dock', kind: 'commercial', max_dwt: 80000 },
  // Kanda
  { name: '神田造船所 川尻工場', lat: 34.2050, lon: 132.6489, company: 'Kanda', kind: 'commercial', max_dwt: 50000 },
  // Saiki
  { name: '佐伯重工業 本社工場', lat: 32.9606, lon: 131.9019, company: 'Saiki HI', kind: 'commercial', max_dwt: 80000 },
  // Higaki / Iwagi
  { name: '檜垣造船 本社工場', lat: 34.0383, lon: 132.8833, company: 'Higaki', kind: 'commercial', max_dwt: 60000 },
  // Naikai
  { name: '内海造船 瀬戸田工場', lat: 34.3083, lon: 133.0922, company: 'Naikai', kind: 'commercial', max_dwt: 30000 },
  { name: '内海造船 田熊工場', lat: 34.4047, lon: 133.1947, company: 'Naikai', kind: 'commercial', max_dwt: 30000 },
];

async function tryOverpass() {
  return fetchOverpass(
    'way["industrial"="shipyard"](area.jp);way["landuse"="industrial"]["shipyard"="yes"](area.jp);',
    (el, _i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        yard_id: `OSM_${el.id}`,
        name: el.tags?.name || 'Shipyard',
        company: el.tags?.operator || 'unknown',
        source: 'osm_overpass',
      },
    }),
  );
}

function generateSeedData() {
  return SEED_SHIPYARDS.map((s, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [s.lon, s.lat] },
    properties: {
      yard_id: `YARD_${String(i + 1).padStart(5, '0')}`,
      name: s.name,
      company: s.company,
      kind: s.kind,
      max_dwt: s.max_dwt,
      country: 'JP',
      source: 'shipyards_seed',
    },
  }));
}

export default async function collectShipyards() {
  let features = await tryOverpass();
  const live = !!(features && features.length > 0);
  if (!live) features = [];
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'shipyards',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Japanese shipyards: Imabari, JMU, MHI, KHI, Mitsui ES, Tsuneishi, Onomichi, Hakodate Dock',
    },
  };
}
