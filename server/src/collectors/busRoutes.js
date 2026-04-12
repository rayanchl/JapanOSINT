/**
 * Bus Routes / Terminals Collector
 * Live: OSM Overpass query for amenity=bus_station across Japan.
 * Fallback: curated major highway/city bus terminals.
 */

import { fetchOverpass } from './_liveHelpers.js';

async function tryLive() {
  return fetchOverpass(
    'node["amenity"="bus_station"](area.jp);',
    (el, i, coords) => ({
      type: 'Feature',
      geometry: { type: 'Point', coordinates: coords },
      properties: {
        bus_id: `OSM_${el.id}`,
        name: el.tags?.name || el.tags?.['name:en'] || `Bus station ${i + 1}`,
        operator: el.tags?.operator || 'unknown',
        bus_type: 'terminal',
        country: 'JP',
        source: 'osm_overpass',
      },
    }),
  );
}

const BUS_TERMINALS = [
  // Major highway bus terminals
  { name: 'バスタ新宿', operator: 'JR/Willer/各社', lat: 35.6884, lon: 139.7008, type: 'highway_terminal', daily_buses: 1600 },
  { name: '東京駅 八重洲口', operator: 'JR Bus Kanto', lat: 35.6814, lon: 139.7700, type: 'highway_terminal', daily_buses: 1200 },
  { name: '池袋駅 西口', operator: 'West Bus', lat: 35.7281, lon: 139.7106, type: 'highway_terminal', daily_buses: 600 },
  { name: '渋谷マークシティ', operator: 'Tokyu Bus', lat: 35.6585, lon: 139.6987, type: 'highway_terminal', daily_buses: 500 },
  { name: '横浜駅東口', operator: 'Sotetsu Bus', lat: 35.4660, lon: 139.6240, type: 'highway_terminal', daily_buses: 700 },
  { name: '大阪駅 JR高速バスターミナル', operator: 'JR Bus West', lat: 34.7028, lon: 135.4955, type: 'highway_terminal', daily_buses: 1100 },
  { name: 'なんば 高速バスターミナル', operator: 'Nankai Bus', lat: 34.6627, lon: 135.5010, type: 'highway_terminal', daily_buses: 800 },
  { name: '名古屋駅 名鉄バスセンター', operator: 'Meitetsu Bus', lat: 35.1709, lon: 136.8836, type: 'highway_terminal', daily_buses: 900 },
  { name: '名古屋 JR高速バスのりば', operator: 'JR Tokai Bus', lat: 35.1707, lon: 136.8810, type: 'highway_terminal', daily_buses: 700 },
  { name: '博多バスターミナル', operator: 'Nishitetsu', lat: 33.5907, lon: 130.4204, type: 'highway_terminal', daily_buses: 900 },
  { name: '天神高速バスターミナル', operator: 'Nishitetsu', lat: 33.5901, lon: 130.3990, type: 'highway_terminal', daily_buses: 800 },
  { name: '札幌駅前バスターミナル', operator: 'JR Hokkaido Bus', lat: 43.0681, lon: 141.3500, type: 'highway_terminal', daily_buses: 600 },
  { name: '仙台駅東口バスプール', operator: 'JR Bus Tohoku', lat: 38.2603, lon: 140.8830, type: 'highway_terminal', daily_buses: 500 },
  { name: '広島バスセンター', operator: 'HD Bus', lat: 34.3963, lon: 132.4576, type: 'highway_terminal', daily_buses: 600 },
  { name: '京都駅八条口', operator: 'JR Bus West', lat: 34.9844, lon: 135.7585, type: 'highway_terminal', daily_buses: 700 },
  { name: '神戸三宮バスターミナル', operator: 'Hankyu Bus', lat: 34.6951, lon: 135.1979, type: 'highway_terminal', daily_buses: 500 },
  { name: '熊本桜町バスターミナル', operator: 'Sankoh Bus', lat: 32.8064, lon: 130.7058, type: 'highway_terminal', daily_buses: 500 },
  { name: '長崎県営バスターミナル', operator: 'Kenei Bus', lat: 32.7525, lon: 129.8694, type: 'highway_terminal', daily_buses: 400 },
  { name: '那覇バスターミナル', operator: '琉球バス', lat: 26.2148, lon: 127.6792, type: 'highway_terminal', daily_buses: 500 },
  { name: '金沢駅東口バスターミナル', operator: 'Hokuriku Bus', lat: 36.5780, lon: 136.6480, type: 'highway_terminal', daily_buses: 400 },
  { name: '富山駅前バスターミナル', operator: 'Toyama Chiho Bus', lat: 36.7014, lon: 137.2131, type: 'highway_terminal', daily_buses: 300 },
  { name: '高松駅前バスターミナル', operator: 'Kotoden Bus', lat: 34.3501, lon: 134.0467, type: 'highway_terminal', daily_buses: 350 },
  { name: '松山駅前バスターミナル', operator: 'Iyotetsu Bus', lat: 33.8395, lon: 132.7544, type: 'highway_terminal', daily_buses: 300 },
  { name: '高知駅前バスターミナル', operator: 'Tosaden Bus', lat: 33.5667, lon: 133.5436, type: 'highway_terminal', daily_buses: 250 },
  { name: '徳島駅前', operator: '徳島バス', lat: 34.0744, lon: 134.5517, type: 'highway_terminal', daily_buses: 300 },
  { name: '岡山駅前バスターミナル', operator: '両備バス', lat: 34.6661, lon: 133.9180, type: 'highway_terminal', daily_buses: 500 },
  { name: '宮崎駅前', operator: '宮崎交通', lat: 31.9164, lon: 131.4272, type: 'highway_terminal', daily_buses: 300 },
  { name: '鹿児島中央駅前', operator: '鹿児島交通', lat: 31.5842, lon: 130.5414, type: 'highway_terminal', daily_buses: 400 },
  { name: '青森駅前', operator: 'JRバス東北', lat: 40.8290, lon: 140.7283, type: 'highway_terminal', daily_buses: 250 },
  { name: '盛岡駅前', operator: 'JRバス東北', lat: 39.7014, lon: 141.1369, type: 'highway_terminal', daily_buses: 280 },
  { name: '秋田駅前', operator: '羽後交通', lat: 39.7183, lon: 140.1261, type: 'highway_terminal', daily_buses: 250 },
  { name: '山形駅前', operator: '山交バス', lat: 38.2483, lon: 140.3281, type: 'highway_terminal', daily_buses: 250 },
  { name: '福島駅東口', operator: '福島交通', lat: 37.7544, lon: 140.4597, type: 'highway_terminal', daily_buses: 280 },
  { name: '新潟駅万代口', operator: '新潟交通', lat: 37.9106, lon: 139.0564, type: 'highway_terminal', daily_buses: 350 },
  // City bus depots
  { name: '深川営業所 (都営バス)', operator: 'Toei Bus', lat: 35.6695, lon: 139.7980, type: 'city_depot', daily_buses: 200 },
  { name: '渋谷営業所 (都営バス)', operator: 'Toei Bus', lat: 35.6533, lon: 139.7011, type: 'city_depot', daily_buses: 180 },
  { name: '葛西営業所 (都営バス)', operator: 'Toei Bus', lat: 35.6661, lon: 139.8731, type: 'city_depot', daily_buses: 220 },
  { name: '南千住営業所 (都営バス)', operator: 'Toei Bus', lat: 35.7321, lon: 139.7975, type: 'city_depot', daily_buses: 190 },
  { name: '梅田営業所 (大阪シティバス)', operator: 'Osaka City Bus', lat: 34.7050, lon: 135.4970, type: 'city_depot', daily_buses: 250 },
  { name: '九条営業所 (大阪シティバス)', operator: 'Osaka City Bus', lat: 34.6720, lon: 135.4760, type: 'city_depot', daily_buses: 180 },
  { name: '基幹営業所 (名古屋市バス)', operator: 'Nagoya City Bus', lat: 35.1690, lon: 136.9000, type: 'city_depot', daily_buses: 200 },
  { name: '京都市交通局 烏丸営業所', operator: 'Kyoto City Bus', lat: 35.0095, lon: 135.7594, type: 'city_depot', daily_buses: 220 },
  { name: '神戸市バス 中央営業所', operator: 'Kobe City Bus', lat: 34.6900, lon: 135.1900, type: 'city_depot', daily_buses: 150 },
  { name: '横浜市営バス 港北営業所', operator: 'Yokohama City Bus', lat: 35.4900, lon: 139.6300, type: 'city_depot', daily_buses: 180 },
  { name: '川崎市バス 上平間営業所', operator: 'Kawasaki City Bus', lat: 35.5600, lon: 139.6650, type: 'city_depot', daily_buses: 140 },
];

function generateSeedData() {
  const now = new Date();
  return BUS_TERMINALS.map((b, i) => ({
    type: 'Feature',
    geometry: { type: 'Point', coordinates: [b.lon, b.lat] },
    properties: {
      bus_id: `BUS_${String(i + 1).padStart(4, '0')}`,
      name: b.name,
      operator: b.operator,
      bus_type: b.type,
      daily_buses: b.daily_buses,
      capacity_category: b.daily_buses > 800 ? 'mega' : b.daily_buses > 400 ? 'major' : 'standard',
      country: 'JP',
      updated_at: now.toISOString(),
      source: 'bus_routes',
    },
  }));
}

export default async function collectBusRoutes() {
  let features = await tryLive();
  const live = !!(features && features.length > 0);
  if (!live) features = generateSeedData();
  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'bus_routes',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      description: 'Bus terminals and depots across Japan - highway buses, city buses',
    },
    metadata: {},
  };
}
