/**
 * Electrical Grid / Power Plants Collector
 * Live: OSM Overpass power=plant + power=substation across Japan.
 * Fallback: curated list of regional power companies' major facilities.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpass(
    'node["power"="plant"](area.jp);way["power"="plant"](area.jp);node["power"="substation"]["substation"!="minor_distribution"](area.jp);way["power"="substation"]["substation"!="minor_distribution"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        power_id: `OSM_${el.id}`,
        name: el.tags?.name || el.tags?.['name:en'] || `Power facility ${i + 1}`,
        type: el.tags?.power === 'plant' ? 'plant' : 'substation',
        fuel: el.tags?.['plant:source'] || el.tags?.['generator:source'] || 'unknown',
        capacity_mw: parseFloat(el.tags?.['plant:output:electricity']) || null,
        operator: el.tags?.operator || 'unknown',
        voltage: el.tags?.voltage || null,
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

const POWER_FACILITIES = [
  // Thermal power plants - TEPCO
  { name: '鹿島火力発電所', operator: 'JERA (旧TEPCO)', type: 'thermal', fuel: 'LNG', lat: 35.9300, lon: 140.6800, capacity_mw: 5660, grid: 'TEPCO' },
  { name: '富津火力発電所', operator: 'JERA (旧TEPCO)', type: 'thermal', fuel: 'LNG', lat: 35.3300, lon: 139.8400, capacity_mw: 5040, grid: 'TEPCO' },
  { name: '袖ヶ浦火力発電所', operator: 'JERA (旧TEPCO)', type: 'thermal', fuel: 'LNG', lat: 35.4400, lon: 140.0100, capacity_mw: 3600, grid: 'TEPCO' },
  { name: '横浜火力発電所', operator: 'JERA (旧TEPCO)', type: 'thermal', fuel: 'LNG', lat: 35.4900, lon: 139.6300, capacity_mw: 3325, grid: 'TEPCO' },
  { name: '川崎火力発電所', operator: 'JERA (旧TEPCO)', type: 'thermal', fuel: 'LNG', lat: 35.5100, lon: 139.7600, capacity_mw: 1500, grid: 'TEPCO' },
  { name: '広野火力発電所', operator: 'JERA (旧TEPCO)', type: 'thermal', fuel: 'coal', lat: 37.2100, lon: 141.0000, capacity_mw: 4400, grid: 'TEPCO' },
  // Kansai
  { name: '姫路第二発電所', operator: '関西電力', type: 'thermal', fuel: 'LNG', lat: 34.7900, lon: 134.6400, capacity_mw: 2919, grid: 'Kansai' },
  { name: '南港発電所', operator: '関西電力', type: 'thermal', fuel: 'LNG', lat: 34.6300, lon: 135.4100, capacity_mw: 1800, grid: 'Kansai' },
  { name: '堺港発電所', operator: '関西電力', type: 'thermal', fuel: 'LNG', lat: 34.5800, lon: 135.4400, capacity_mw: 2000, grid: 'Kansai' },
  { name: '舞鶴発電所', operator: '関西電力', type: 'thermal', fuel: 'coal', lat: 35.4800, lon: 135.3600, capacity_mw: 1800, grid: 'Kansai' },
  // Chubu
  { name: '碧南火力発電所', operator: 'JERA (旧中部電力)', type: 'thermal', fuel: 'coal', lat: 34.8200, lon: 136.9200, capacity_mw: 4100, grid: 'Chubu' },
  { name: '川越火力発電所', operator: 'JERA (旧中部電力)', type: 'thermal', fuel: 'LNG', lat: 35.0300, lon: 136.6600, capacity_mw: 4802, grid: 'Chubu' },
  { name: '知多火力発電所', operator: 'JERA (旧中部電力)', type: 'thermal', fuel: 'LNG', lat: 34.9100, lon: 136.8400, capacity_mw: 3966, grid: 'Chubu' },
  // Tohoku
  { name: '原町火力発電所', operator: '東北電力', type: 'thermal', fuel: 'coal', lat: 37.6300, lon: 141.0000, capacity_mw: 2000, grid: 'Tohoku' },
  { name: '能代火力発電所', operator: '東北電力', type: 'thermal', fuel: 'coal', lat: 40.2100, lon: 140.0500, capacity_mw: 1800, grid: 'Tohoku' },
  { name: '仙台火力発電所', operator: '東北電力', type: 'thermal', fuel: 'LNG', lat: 38.2700, lon: 141.0200, capacity_mw: 446, grid: 'Tohoku' },
  // Chugoku
  { name: '三隅発電所', operator: '中国電力', type: 'thermal', fuel: 'coal', lat: 34.8500, lon: 132.0000, capacity_mw: 1000, grid: 'Chugoku' },
  { name: '玉島発電所', operator: '中国電力', type: 'thermal', fuel: 'oil', lat: 34.5500, lon: 133.6900, capacity_mw: 1200, grid: 'Chugoku' },
  // Kyushu
  { name: '苓北発電所', operator: '九州電力', type: 'thermal', fuel: 'coal', lat: 32.4900, lon: 129.9700, capacity_mw: 1400, grid: 'Kyushu' },
  { name: '新大分発電所', operator: '九州電力', type: 'thermal', fuel: 'LNG', lat: 33.2400, lon: 131.7000, capacity_mw: 2295, grid: 'Kyushu' },
  // Hokkaido
  { name: '苫東厚真発電所', operator: '北海道電力', type: 'thermal', fuel: 'coal', lat: 42.7100, lon: 141.7800, capacity_mw: 1650, grid: 'Hokkaido' },
  { name: '伊達発電所', operator: '北海道電力', type: 'thermal', fuel: 'oil', lat: 42.4700, lon: 140.8500, capacity_mw: 700, grid: 'Hokkaido' },
  // Hokuriku
  { name: '七尾大田発電所', operator: '北陸電力', type: 'thermal', fuel: 'coal', lat: 37.0300, lon: 136.9700, capacity_mw: 1200, grid: 'Hokuriku' },
  // Shikoku
  { name: '橘湾発電所', operator: '四国電力', type: 'thermal', fuel: 'coal', lat: 33.8800, lon: 134.6000, capacity_mw: 700, grid: 'Shikoku' },
  // Okinawa
  { name: '具志川火力発電所', operator: '沖縄電力', type: 'thermal', fuel: 'coal', lat: 26.4000, lon: 127.8500, capacity_mw: 312, grid: 'Okinawa' },
  { name: '吉の浦火力発電所', operator: '沖縄電力', type: 'thermal', fuel: 'LNG', lat: 26.3000, lon: 127.7600, capacity_mw: 502, grid: 'Okinawa' },

  // Hydroelectric
  { name: '黒部川第四発電所 (黒四)', operator: '関西電力', type: 'hydro', fuel: 'water', lat: 36.5667, lon: 137.6633, capacity_mw: 335, grid: 'Kansai' },
  { name: '奥多々良木発電所', operator: '関西電力', type: 'hydro_pumped', fuel: 'water', lat: 35.3000, lon: 134.7600, capacity_mw: 1932, grid: 'Kansai' },
  { name: '神流川発電所', operator: '東京電力', type: 'hydro_pumped', fuel: 'water', lat: 36.0500, lon: 138.7000, capacity_mw: 2820, grid: 'TEPCO' },
  { name: '今市発電所', operator: '東京電力', type: 'hydro_pumped', fuel: 'water', lat: 36.7300, lon: 139.6500, capacity_mw: 1050, grid: 'TEPCO' },
  { name: '葛野川発電所', operator: '東京電力', type: 'hydro_pumped', fuel: 'water', lat: 35.6800, lon: 138.7800, capacity_mw: 1600, grid: 'TEPCO' },
  { name: '塩原発電所', operator: '東京電力', type: 'hydro_pumped', fuel: 'water', lat: 36.9800, lon: 139.8500, capacity_mw: 900, grid: 'TEPCO' },
  { name: '佐久間ダム発電所', operator: '電源開発', type: 'hydro', fuel: 'water', lat: 35.0900, lon: 137.8100, capacity_mw: 350, grid: 'Chubu' },
  { name: '奥只見発電所', operator: '電源開発', type: 'hydro', fuel: 'water', lat: 37.1700, lon: 139.2200, capacity_mw: 560, grid: 'TEPCO' },
  { name: '田子倉発電所', operator: '電源開発', type: 'hydro', fuel: 'water', lat: 37.0500, lon: 139.3500, capacity_mw: 400, grid: 'Tohoku' },
  { name: '宮中ダム', operator: 'JR東日本', type: 'hydro', fuel: 'water', lat: 37.0800, lon: 138.6400, capacity_mw: 449, grid: 'TEPCO' },
  { name: '玉原発電所', operator: '東京電力', type: 'hydro_pumped', fuel: 'water', lat: 36.7700, lon: 139.0100, capacity_mw: 1200, grid: 'TEPCO' },
  { name: '新高瀬川発電所', operator: '東京電力', type: 'hydro_pumped', fuel: 'water', lat: 36.4900, lon: 137.6500, capacity_mw: 1280, grid: 'TEPCO' },
  // Geothermal
  { name: '八丁原発電所', operator: '九州電力', type: 'geothermal', fuel: 'steam', lat: 33.0900, lon: 131.2400, capacity_mw: 112, grid: 'Kyushu' },
  { name: '葛根田発電所', operator: '東北電力', type: 'geothermal', fuel: 'steam', lat: 39.7600, lon: 140.7800, capacity_mw: 80, grid: 'Tohoku' },
  { name: '柳津西山発電所', operator: '東北電力', type: 'geothermal', fuel: 'steam', lat: 37.5800, lon: 139.6500, capacity_mw: 65, grid: 'Tohoku' },
  { name: '松川発電所', operator: '東北電力', type: 'geothermal', fuel: 'steam', lat: 39.9000, lon: 140.9000, capacity_mw: 23, grid: 'Tohoku' },
  { name: '森発電所', operator: '北海道電力', type: 'geothermal', fuel: 'steam', lat: 42.0900, lon: 140.5800, capacity_mw: 50, grid: 'Hokkaido' },
  // Wind farms
  { name: '六ヶ所村ウィンドファーム', operator: '日本風力開発', type: 'wind', fuel: 'wind', lat: 40.9700, lon: 141.3700, capacity_mw: 51, grid: 'Tohoku' },
  { name: '釜石広域風力発電所', operator: 'ユーラスエナジー', type: 'wind', fuel: 'wind', lat: 39.2800, lon: 141.7600, capacity_mw: 43, grid: 'Tohoku' },
  { name: '横浜町ウィンドファーム', operator: 'コスモエコパワー', type: 'wind', fuel: 'wind', lat: 41.0900, lon: 141.2600, capacity_mw: 32, grid: 'Tohoku' },
  // Solar farms (mega solar)
  { name: '瀬戸内Kirei太陽光発電所', operator: '中国電力', type: 'solar', fuel: 'sun', lat: 34.5500, lon: 133.7800, capacity_mw: 235, grid: 'Chugoku' },
  { name: '苫小牧勇払メガソーラー', operator: 'ソフトバンク', type: 'solar', fuel: 'sun', lat: 42.6300, lon: 141.6700, capacity_mw: 111, grid: 'Hokkaido' },
  { name: '田原ソーラー', operator: '三井化学', type: 'solar', fuel: 'sun', lat: 34.6700, lon: 137.3000, capacity_mw: 50, grid: 'Chubu' },
  { name: '大分ソーラーパワー', operator: 'JX日鉱日石', type: 'solar', fuel: 'sun', lat: 33.2900, lon: 131.6800, capacity_mw: 82, grid: 'Kyushu' },

  // Major substations / interconnects
  { name: '新豊洲変電所', operator: 'TEPCO', type: 'substation', fuel: 'grid', lat: 35.6500, lon: 139.7900, capacity_mw: 1500, grid: 'TEPCO' },
  { name: '新所沢変電所', operator: 'TEPCO', type: 'substation', fuel: 'grid', lat: 35.7800, lon: 139.4700, capacity_mw: 2000, grid: 'TEPCO' },
  { name: '佐久間周波数変換所', operator: '電源開発', type: 'frequency_converter', fuel: 'grid', lat: 35.0900, lon: 137.8100, capacity_mw: 300, grid: '50/60Hz' },
  { name: '新信濃変電所', operator: 'TEPCO', type: 'frequency_converter', fuel: 'grid', lat: 36.2700, lon: 138.0500, capacity_mw: 600, grid: '50/60Hz' },
  { name: '東清水変電所', operator: 'Chubu', type: 'frequency_converter', fuel: 'grid', lat: 35.0400, lon: 138.5100, capacity_mw: 300, grid: '50/60Hz' },
  { name: '北本連系線 函館変換所', operator: '電源開発', type: 'hvdc_converter', fuel: 'grid', lat: 41.7700, lon: 140.7200, capacity_mw: 600, grid: 'Hokkaido-Tohoku' },
  { name: '紀伊水道直流連系設備', operator: '関西電力', type: 'hvdc_converter', fuel: 'grid', lat: 33.6800, lon: 134.5400, capacity_mw: 1400, grid: 'Kansai-Shikoku' },
];

function generateSeedData() {
  const now = new Date();
  return POWER_FACILITIES.map((p, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [p.lon, p.lat] },
    properties: {
      facility_id: `PWR_${String(i + 1).padStart(4, '0')}`,
      name: p.name,
      operator: p.operator,
      facility_type: p.type,
      fuel: p.fuel,
      capacity_mw: p.capacity_mw,
      grid_region: p.grid,
      capacity_category: p.capacity_mw > 2000 ? 'mega' : p.capacity_mw > 500 ? 'large' : p.capacity_mw > 100 ? 'medium' : 'small',
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'electrical_grid',
    },
  }));
}

export default async function collectElectricalGrid() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'electrical_grid',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Japan electrical grid - power plants (thermal/hydro/wind/solar/geothermal), substations, frequency converters',
    },
    metadata: {},
  };
}
